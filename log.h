#ifndef _ETL_LOG_H_
#define _ETL_LOG_H_

#include "init.h"

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/printk.h>

void etl_register_debugfs(const char *logpath);
void etl_unregister_debugfs(void);

void etl_pkt_hex_dump(struct sk_buff *pskb, const char *type, int offset, int flag);

int etl_debug_log(enum DBG_LEVEL dbg_level, const char *fmt, ...)
__printf(2, 3);

#define _etl_dbg(type, ratelimited, fmt, arg...)		\
	do {								\
		etl_debug_log((type), fmt, ## arg); \
	}	\
	while (0)

#define etl_dbg(type, arg...) \
	_etl_dbg(type, 0, ## arg)

#endif /* _ETL_LOG_H_ */

