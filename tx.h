#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/getcpu.h>

void etl_txhPacketOut(struct sk_buff *skb);
int etl_mcu_skb_send_msg(struct sk_buff *skb);

