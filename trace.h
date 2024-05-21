#ifndef _ETL_TRACE_H_
#define _ETL_TRACE_H_
#endif

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/percpu.h>
#include <linux/printk.h>
#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM etunnel

#define ETL_MAX_MSG_LEN 256


TRACE_EVENT(etl_dbg_msg,

		TP_PROTO(struct va_format *vaf),

		TP_ARGS(vaf),

		TP_STRUCT__entry(
			__dynamic_array(char, msg, ETL_MAX_MSG_LEN)
			),

		TP_fast_assign(
			WARN_ON_ONCE(vsnprintf(__get_dynamic_array(msg),
					ETL_MAX_MSG_LEN,
					vaf->fmt,
					*vaf->va) >= ETL_MAX_MSG_LEN);
			),

		TP_printk(
			"%s",
			__get_str(msg)
			)
		);


#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace

/* This part must be outside protection */
#include <trace/define_trace.h>

