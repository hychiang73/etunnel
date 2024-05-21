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

static char *eth_if ="eth0";
module_param(eth_if, charp, 0644);

struct net_device *eth_dev = NULL;
struct minmhdr mmhdr_template;
struct fixed_radiotap rdp;
struct etl_mutex etl_mutex;
u8 hx_mac_addr[ETH_ALEN];
u8 test_pc_addr[ETH_ALEN];

enum {
	HX_ADDR_CONF = 0,
	DEST_IP_CONF,
	DEST_UDP_CONF,
	HX_HDR_CONF,
	TEST_PC_ADDR_CONF,

	// Must be the second last, Can not move because the len is dynamic
	RADIOTAP_CONF,

	// Must be last, the total number of conf
	NUM_OF_CONF,
};

/*
 * If you want to add new config option, add it before the RADIOTAP_CONF
 * @name: string name in config file
 * @str_len: the len of string name
 * @data_len: the total data len
 * @addr: where to put the data
 */

static struct etl_conf etl_conf_member[NUM_OF_CONF] = {
	[HX_ADDR_CONF] = {
		.name = "HX_ADDR",
		.str_len = 7,
		.data_len = ETH_ALEN,
		.addr = &hx_mac_addr[0],
	},

	[DEST_IP_CONF] = {
		.name = "DEST_IP",
		.str_len = 7,
		.data_len = sizeof(struct iphdr),
		.addr = (u8*)(&mmhdr_template.ip_hdr),
	},

	[DEST_UDP_CONF] = {
		.name = "DEST_UDP",
		.str_len = 8,
		.data_len = sizeof(struct udphdr),
		.addr = (u8*)(&mmhdr_template.udp_hdr),
	},

	[HX_HDR_CONF] = {
		.name = "HX_HDR",
		.str_len = 6,
		.data_len = HX_MAGIC_LEN,
		.addr = &mmhdr_template.hx[0],
	},

	[TEST_PC_ADDR_CONF] = {
		.name = "TEST_PC_ADDR",
		.str_len = 12,
		.data_len = ETH_ALEN,
		.addr = &test_pc_addr[0],
	},

	[RADIOTAP_CONF] = {
		.name = "RADIOTAP",
		.str_len = 8,
		.data_len = 0,
		/*.addr = rdp.data,*/
	}
};


static int etl_chartohex(u8 ch)
{
	int res;
	if (ch >= '0' && ch <= '9') {
		// data is between 0 ~ 9
		res = (u8)(ch - '0');
	} else if (ch >= 'A' && ch <= 'F') {
		// data is between A ~ F
		res = (u8)(ch - 'A') + 10;
	} else if (ch >= 'a' && ch <= 'f') {
		// data is between a ~ f
		res = (u8)(ch - 'a') + 10;
	} else {
		res = -1;
		printk(KERN_ERR "etl: Parsing failed : %c(%d)\n", ch, ch);
	}

	return res;
}

/*
 * char valid check and convert the string to int
 * return: -1 -> the parsing check is failed
 * return: 0  -> convert successful
 */
static int etl_parse_check(u8 c1, u8 c2, u8 *val)
{
	int d1, d2;
	d1 = etl_chartohex(c1);
	d2 = etl_chartohex(c2);

	if (d1 < 0 || d2 < 0)
		return -1;

	*val = (d1 << 4) + d2;
	return 0;
}

#define ETL_CONFIG_SIZE	512
static int etl_radiotap_check(u8 *pbuf, int ret, int pos, int *sel) {
	// radiotap len position = byte 2
	if (pos + 6 + 1 >= ret) {
		rdp.len = 0;
		return 0;
	}

	if (etl_parse_check(*(pbuf + 6), *(pbuf + 6 + 1), &etl_conf_member[*sel].data_len))
		return -1;

	rdp.data = kmalloc(etl_conf_member[*sel].data_len, GFP_KERNEL);
	if (!rdp.data)
		return -1;

	rdp.len = etl_conf_member[*sel].data_len;
	return 0;
}

static int etl_parse_config(const char *cpath)
{
	struct file *fp;
	int ret, j, sel = 0, pos;
	u8 buf[ETL_CONFIG_SIZE];
	u8 *data;

	rdp.len = 0;
	fp = filp_open(cpath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		printk(KERN_ERR "Cannot open the file %ld\n", PTR_ERR(fp));
		return -1;
	}

	memset(buf, 0, ETL_CONFIG_SIZE);
	ret = kernel_read(fp, buf, ETL_CONFIG_SIZE, 0);

	while (pos < ret && sel < NUM_OF_CONF) {
		if (pos + etl_conf_member[sel].str_len >= ret) {
			// remain data is less than (pos + strlen)
			ret = -1;
			goto out;
		}

		if (!memcmp(&buf[pos], &etl_conf_member[sel].name[0], etl_conf_member[sel].str_len)) {
			// conf string len + 1("=")
			pos += etl_conf_member[sel].str_len + 1;
			printk(KERN_INFO "etl: [%s] = ", etl_conf_member[sel].name);
			if (sel == RADIOTAP_CONF) {
				if (etl_radiotap_check(&buf[pos], ret, pos, &sel)) goto out;

				data = rdp.data;
			} else {
				data = etl_conf_member[sel].addr;
			}

			for (j = 0; j < etl_conf_member[sel].data_len; j++) {
				// check 8 bit a time
				if (pos + 1 >= ret || etl_parse_check(buf[pos], buf[pos + 1], data)) {
					ret = -1;
					goto out;
				}

				printk(KERN_CONT "%02x ", *data);
				// check data 8 bit + "space" = 2 + 1 = 3
				pos += 3;
				data++;
			}

			printk(KERN_CONT "\n");
			sel++;
		} else {
			pos++;
		}
	}
out:
	filp_close(fp, NULL);
	if (sel != NUM_OF_CONF || ret < 0) {
		if (rdp.data)
			kfree(rdp.data);

		return ret;
	}

	return 0;
}

static struct net_device *etl_get_netdev(void) {
	struct net_device *dev;

	read_lock(&dev_base_lock);
	dev = first_net_device(&init_net);
	while(dev) {
		printk(KERN_INFO "etl: Found [%s] netdevice\n", dev->name);

		if (!strcmp(dev->name, eth_if))
			break;
		else
			dev = next_net_device(dev);
	}

	read_unlock(&dev_base_lock);
	return dev;
}

int etl_init(const char *cpath, const char *logpath) {

	printk(KERN_INFO "etl: load etl version %s\n", ETL_VERSION);

	mutex_init(&etl_mutex.mutex);

	if (etl_parse_config(cpath))
		return -1;

	etl_register_debugfs(logpath);
	eth_dev = etl_get_netdev();
	if (!eth_dev)
		return -1;

	if (etl_register_rxHandlers())
		goto out_free_debugfs;

	return 0;

out_free_debugfs:
	etl_unregister_debugfs();
	return -1;
}

void etl_deinit(void) {

	printk(KERN_INFO "etl: unload etl\n");

	mutex_destroy(&etl_mutex.mutex);

	if (rdp.data)
		kfree(rdp.data);

	etl_unregister_debugfs();
	etl_unregister_rxHandlers();
}
#undef ETL_CONFIG_SIZE


