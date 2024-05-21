#ifndef _ETL_INIT_H_
#define _ETL_INIT_H_

#ifndef ETL_AUTHOR
#define ETL_AUTHOR "AkiraNet-Patrick <bai@akiranet.com.tw>"
#endif
#ifndef ETL_DRIVER_DESC
#define ETL_DRIVER_DESC "Transport mac-in-mac frame between 802.3 and 802.11 in ethernet tunnel"
#endif
#ifndef ETL_VERSION
#define ETL_VERSION "rc5-14"
#endif

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

enum DBG_OPT {
	ETL_TX_DUMP_BY_KERN_LOG = 0b0001,
	ETL_TX_DUMP_BY_RECORD	= 0b0010,
	ETL_RX_DUMP_BY_KERN_LOG	= 0b0100,
	ETL_RX_DUMP_BY_RECORD	= 0b1000,
};

enum DBG_LEVEL {
	ETL_DEBUG_NONE	= 0b0000,
	ETL_DEBUG_TX_DUMP = 0b0001,
	ETL_DEBUG_RX_DUMP = 0b0010,
	ETL_DEBUG_ALL_DUMP = 0b0011,
	ETL_DEBUG_MSG = 0b0100,
	ETL_DEBUG_INFO = 0b1000,
};

struct etl_mutex {
	struct mutex mutex;
};

struct fixed_radiotap {
	u8 len;
	u8 *data;
};

struct etl_conf {
	char name[16];
	u8 str_len;
	u8 data_len;
	u8 *addr;
};

#include "hwsim.h"
#include "rx.h"
#include "log.h"
#include "tx.h"
#include "trace.h"
#include "hyperX.h"

// RADIOTAP should be put in the last position
#define ETL_PARSING_STR {"HX_ADDR", "DEST_IP", "DEST_UDP", "HX_HDR", "TEST_PC_ADDR", "RADIOTAP"}

int etl_init(const char *cpath, const char *logpath);
void etl_deinit(void);

extern struct net_device *eth_dev;
extern struct minmhdr mmhdr_template;
extern struct fixed_radiotap rdp;
extern struct etl_mutex etl_mutex;
extern u8 hx_mac_addr[ETH_ALEN];
extern u8 test_pc_addr[ETH_ALEN];

#endif // _ETL_INIT_H_
