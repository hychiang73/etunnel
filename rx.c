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

rx_handler_result_t etl_rxhPacketIn(struct sk_buff **ppkt) {
	struct sk_buff *skb;
	struct ieee80211_hdr *hdr11;
	struct hxmsg_header *hxmsg;
	struct minmhdr *pmmhdr;
	int i;
	u8 radiolen;

	skb = *ppkt;
	pmmhdr = (struct minmhdr*)skb_mac_header(skb);

	if (!ether_addr_equal(pmmhdr->eth_hdr.h_source, hx_mac_addr))
		return RX_HANDLER_PASS;

	// check if the len is large than minmhdr size and hx_magic code is match
	if (skb->len < sizeof(struct minmhdr)) {
		etl_dbg(ETL_DEBUG_MSG, "[etl] Rx_handler: src is hx_addr, but the length is not enough. Dropping it\n");

		kfree_skb(skb);
		return RX_HANDLER_CONSUMED;
	}

#if 0
	for (i = 0; i < HX_MAGIC_LEN; i++) {
		if (pmmhdr->hx[i] != mmhdr_template.hx[i]) {
			etl_dbg(ETL_DEBUG_MSG, "[etl] Rx_handler: src is hx_addr, but the hx_magic code is not match. Dropping it\n");

			kfree_skb(skb);
			return RX_HANDLER_CONSUMED;
		}
	}
#endif

	etl_pkt_hex_dump(skb, "etl rx monitor1", 0, ETL_DEBUG_RX_DUMP);

	/* ethhdr=14, ip=20, udp=8, hx=12, radiotap(read data[3]), 802.11 header, payload
	 * 1. remove ip/udp header -> 20 + 8
	 * 2. remove hx -> 12, msg_header -> 2
	 * 3. shift mac header ptr -> pdata + radio len
	 */

	// ip, udp
	skb_pull(skb, sizeof(struct iphdr));
	skb_pull(skb, sizeof(struct udphdr));

	// hx, msg_header, radiotap
	skb_pull(skb, HX_MAGIC_LEN);

	hxmsg = (struct hxmsg_header *)(skb->data);
	skb_pull(skb, sizeof(struct hxmsg_header));

	radiolen = (u8)(*(skb->data + 2));

	// Removing padding data from buffer tail
	skb_trim(skb, hxmsg->length);
	skb_pull(skb, radiolen);

	// mac header handler
	skb_reset_mac_header(skb);
	hdr11 = (struct ieee80211_hdr *)skb_mac_header(skb);
	etl_dbg(ETL_DEBUG_MSG, "[etl] Rx_handler: hdr11 dst=%pM, hdr11 src=%pM, fc=%04x\n",
							 hdr11->addr1, hdr11->addr2, hdr11->frame_control);

	etl_pkt_hex_dump(skb, "etl rx monitor2", 0, ETL_DEBUG_RX_DUMP);
	etl_rx_fwdingToHwsim(skb, radiolen);
	//kfree_skb(skb);
	return RX_HANDLER_CONSUMED;
}

int etl_register_rxHandlers(void) {
	int regerr;
	if (!eth_dev) {
		printk(KERN_INFO "etl: [rx Handler] Do not get eth_dev, exit!!\n");
		return -1;
	}

	rtnl_lock();
	regerr = netdev_rx_handler_register(eth_dev, etl_rxhPacketIn, NULL);
	rtnl_unlock();
	if (regerr) {
		printk(KERN_INFO "etl: [rx Handler] Could not register handler with device [%s], error %i\n", eth_dev->name, regerr);
		return -1;
	} else {
		printk(KERN_INFO "etl: [rx Handler] Handler registered with device [%s]\n", eth_dev->name);
	}

	return 0;
}

void etl_unregister_rxHandlers(void) {
	if (!eth_dev) return;

	rtnl_lock();
	netdev_rx_handler_unregister(eth_dev);
	rtnl_unlock();
	printk(KERN_INFO "etl: [rx Handler] un-registered with device [%s]\n", eth_dev->name);
}

