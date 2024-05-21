#include <linux/tcp.h>
#include <linux/getcpu.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <linux/atomic.h>
#include <linux/build_bug.h>
#include <linux/crc32c.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/genetlink.h>
#include <linux/gfp.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/printk.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <net/dsfield.h>
#include <net/rtnetlink.h>

#include "init.h"

#define TRX_TOTAL_LEN 1078

static uint16_t __seq;

int etl_mcu_skb_send_msg(struct sk_buff *skb)
{
	int ret = 0;
	__be16 ethertype;
	u8 hdrlen;

	mutex_lock(&etl_mutex.mutex);

	ethertype = htons(ETH_P_IP);
	hdrlen = sizeof(struct minmhdr);

	ether_addr_copy(mmhdr_template.eth_hdr.h_source, eth_dev->dev_addr);
	ether_addr_copy(mmhdr_template.eth_hdr.h_dest, hx_mac_addr);

	mmhdr_template.msghdr.type = HXTYPE_CMD;
	mmhdr_template.msghdr.length = skb->len;
	mmhdr_template.eth_hdr.h_proto = ethertype;
	mmhdr_template.msghdr.seq = __seq++;
	memcpy(skb_push(skb, hdrlen), &mmhdr_template, hdrlen);

	skb_reset_mac_header(skb);
	if (skb_put_padto(skb, TRX_TOTAL_LEN))
		goto err_free;

	skb->dev = eth_dev;

	ret = dev_queue_xmit(skb);

	mutex_unlock(&etl_mutex.mutex);

	return ret;

err_free:
	mutex_unlock(&etl_mutex.mutex);
	printk(KERN_ERR "etl[%s]: send msg error\n", __func__);
	kfree_skb(skb);
	return -1;
}

void etl_txhPacketOut(struct sk_buff *skb) 
{
	struct sk_buff *skb2;
	uint16_t *seq_ptr;
	int ret, radiolen;
	__be16 ethertype = htons(ETH_P_IP);
	__le16 fc;
	u8 hdrlen = sizeof(struct minmhdr);
	u8 *data;

	if (rdp.len) {
		// Do not need to copy skb again, just try to expand the head
		if (pskb_expand_head(skb, rdp.len, 0, GFP_ATOMIC))
			return;
		/* skb = skb_copy_expand(skb, rdp.len, 0, GFP_ATOMIC);
		if (skb == NULL)
			return; */

		data = skb_push(skb, rdp.len);
		memcpy(data, rdp.data, rdp.len);
	} else {
		data = (u8 *)skb_mac_header(skb);
	}

	etl_pkt_hex_dump(skb, "etl tx monitor 1", 0, ETL_DEBUG_TX_DUMP);
	radiolen = (int)(data[2]);
	fc = (__le16)(*(data + radiolen));

	// Check the frame control. If it is ACK frame, dropping it
	if (fc == cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_ACK)) {
		etl_dbg(ETL_DEBUG_MSG, "--- Get the ACK frame(frame_control=0x%04x), drop it\n", fc);
		return;
	}

	// Check the frame control, distinguish BEACON or others
	if (ieee80211_is_mgmt(fc) &&
	   ((fc & cpu_to_le16(IEEE80211_FCTL_STYPE)) == cpu_to_le16(IEEE80211_STYPE_BEACON)))
		mmhdr_template.msghdr.type = HXTYPE_BEACON;
	else
		mmhdr_template.msghdr.type = HXTYPE_DATA;

	mmhdr_template.msghdr.length = skb->len;
	/* record the seq position. add it after the frame is already to tx
	 * to avoid adding it but drop by later step
	 */
	seq_ptr = &mmhdr_template.msghdr.seq;

	/* The ether source is local eth0 interface, and the dest is filled by 'HX_ADDR' in config file */
	ether_addr_copy(mmhdr_template.eth_hdr.h_source, eth_dev->dev_addr);
	ether_addr_copy(mmhdr_template.eth_hdr.h_dest, hx_mac_addr);

	mmhdr_template.eth_hdr.h_proto = ethertype;
	etl_dbg(ETL_DEBUG_MSG, "[etl] Tx_handler: dst=%pM, src=%pM, proto=%04x(%s), len=%d, seq=%d, fc=%04x\n",
							mmhdr_template.eth_hdr.h_dest, mmhdr_template.eth_hdr.h_source,
							mmhdr_template.eth_hdr.h_proto, mmhdr_template.msghdr.type? "OTHERS" : "BEACON",
							mmhdr_template.msghdr.length, mmhdr_template.msghdr.seq, fc);
	/*etl_dbg(ETL_DEBUG_MSG, "[etl] Tx_handler: Hx header=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
							mmhdr_template.hx[0], mmhdr_template.hx[1], mmhdr_template.hx[2],
							mmhdr_template.hx[3], mmhdr_template.hx[4],	mmhdr_template.hx[5],
							mmhdr_template.hx[6], mmhdr_template.hx[7], mmhdr_template.hx[8],
							mmhdr_template.hx[9], mmhdr_template.hx[10], mmhdr_template.hx[11]);*/

	skb2 = skb_share_check(skb, GFP_ATOMIC);
	if (!skb2) {
		skb2 = skb;
		goto out;
	}

	if (skb_cow_head(skb2, hdrlen)) {
		etl_dbg(ETL_DEBUG_MSG, "[etl] Tx_handler: skb_cow_head failed\n");
		goto out;
	}

	memcpy(skb_push(skb2, hdrlen), &mmhdr_template, hdrlen);

	// hxhdrlen + radiotap + ieee80211_hdr(24)
	skb_set_network_header(skb2, hdrlen + radiolen + 24);
	skb_reset_mac_header(skb2);

	/* if skb_put_padto error, the function will free_skb by itselt.
	 * so just go out and drop the packet
	 */
	if (skb_put_padto(skb2, TRX_TOTAL_LEN))
		return;

	*seq_ptr = __seq++;
	etl_pkt_hex_dump(skb2, "skb2 etl tx monitor 2", 0, ETL_DEBUG_TX_DUMP);

	skb2->dev = eth_dev;
	//pr_info("net:%p, data:%p, tail:%p\n", skb_network_header(skb2), skb2->data, skb_tail_pointer(skb2));
	ret = dev_queue_xmit(skb2);

	/* skb was consumed */
	skb2 = NULL;

out:
	kfree_skb(skb2);
}

