#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/getcpu.h>

int etl_register_rxHandlers(void);
void etl_unregister_rxHandlers(void);

