#include "init.h"

struct etl_logger {
	struct file *fp;
	struct mutex etl_mutex;
	loff_t pos;
	u32 frames_th;
	u32 frames;
	u8 opt;
};

enum etl_dump_opt {
	OPT_KERN_LOG = 0b0001,
	OPT_RECORD = 0b0010,
};

static struct etl_logger *tx_logger;
static struct etl_logger *rx_logger;

static char __logpath[256];
static char *tx_file = "etl_tx_logger";
static char *rx_file = "etl_rx_logger";

static struct dentry *etl_dentry;
static u32 etl_debug_opt;

static int etl_create_file(char *logpath, struct etl_logger **plogger, char *filename)
{
	struct file *fp;
	int failed_times = 0;

	do {
		char concate[256];

		sprintf(concate, "%s%s%d", logpath, filename, failed_times);
		fp = filp_open(concate, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			(*plogger)->fp = filp_open(concate, O_CREAT | O_RDWR, 0644);
			if (IS_ERR((*plogger)->fp)) {
				printk(KERN_ERR "etl: create %s file at %s failed\n", filename, concate);
				(*plogger)->fp = NULL;
			} else {
				printk(KERN_INFO "etl: create %s file at %s\n", filename, concate);
				mutex_init(&(*plogger)->etl_mutex);
			}

			break;
		} else {
			filp_close(fp, NULL);
			failed_times++;
		}

	} while (failed_times < 100);

	if (failed_times == 100)
		return -1;

	return 0;
}

static void __etl_debug_stop(struct etl_logger **plogger)
{
	(*plogger)->frames = 0;
	(*plogger)->opt = 0;
	(*plogger)->pos = 0;

	if ((*plogger)->fp)
		filp_close((*plogger)->fp, NULL);

	(*plogger)->fp = NULL;
	mutex_destroy(&(*plogger)->etl_mutex);
}

static void etl_debug_stop(void)
{
	__etl_debug_stop(&tx_logger);
	__etl_debug_stop(&rx_logger);
}

static ssize_t etl_debug_start(struct file *fp, const char __user *user_buffer,
                                size_t count, loff_t *position)
{
	char kern_buf[2];
	u8 tx_opt, rx_opt;
	if (count != 2) {
		etl_debug_stop();
		printk(KERN_ERR "etl: debug start parameter is only length 1 [0 | 1], stop to dump...\n");
		return count;
	}

	if (copy_from_user(kern_buf, user_buffer, 2))
		return -EFAULT;

	printk(KERN_INFO "etl: Start parameter is %c\n", kern_buf[0]);
	if (kern_buf[0] == '1') {
		printk(KERN_INFO "etl: start to dump...\n");
		tx_opt = (etl_debug_opt & 0x3);
		rx_opt = ((etl_debug_opt >> 2) & 0x3);
		etl_debug_opt = 0;
		tx_logger->frames = rx_logger->frames = 0;

		if (tx_opt & OPT_RECORD) {
			if (!strcmp(__logpath, "")) {
				printk(KERN_ERR "etl: logpath is NULL, do not record\n");
				tx_opt &= (~OPT_RECORD);
			} else {
				if (etl_create_file(__logpath, &tx_logger, tx_file)) {
					printk(KERN_ERR "etl: Create Tx logger file failed\n");
					tx_opt &= (~OPT_RECORD);
				}
			}
		}
		tx_logger->opt = tx_opt;

		if (rx_opt & OPT_RECORD) {
			if (!strcmp(__logpath, "")) {
				printk(KERN_ERR "etl: logpath is NULL, do not record\n");
				rx_opt &= (~OPT_RECORD);
				/* rx_logger->opt &= (~OPT_RECORD);*/
			} else {
				if (etl_create_file(__logpath, &rx_logger, rx_file)) {
					printk(KERN_ERR "etl: Create Rx logger file failed\n");
					/* rx_logger->opt &= ~(OPT_RECORD); */
					rx_opt &= ~(OPT_RECORD);
				}
			}
		}
		rx_logger->opt = rx_opt;
	} else {
		etl_debug_stop();
		printk(KERN_INFO "etl: stop to dump...\n");
	}

	return count;
}

static const struct file_operations fops_debug_start = {
        .write = etl_debug_start,
};

static int etl_write_file(struct etl_logger **plogger, char *buf)
{
	int ret;

	ret = kernel_write((*plogger)->fp, buf, strlen(buf), &(*plogger)->pos);

	if (ret < 0)
		printk(KERN_ERR "etl: kernel write file failed\n");

	return ret;
}

#define LOGGER_BUF_SIZE 512
void etl_pkt_hex_dump(struct sk_buff *pskb, const char *type, int offset, int flag)
{
	struct etl_logger **plogger;
	struct sk_buff *skb;
	size_t len;
	int rowsize = 16;
	int i, l, linelen, remaining;
	int li = 0;
	int bufpos = 0;
	u8 *data, ch, buf[LOGGER_BUF_SIZE];
	u8 lock_get = 0, kern_dump = 0;

	if (flag & ETL_DEBUG_TX_DUMP)
		plogger = &tx_logger;
	else if (flag & ETL_DEBUG_RX_DUMP)
		plogger = &rx_logger;
	else
		return;

	if (!(*plogger)->opt)
		return;

	if ((*plogger)->opt & OPT_RECORD)
		if (mutex_trylock(&(*plogger)->etl_mutex))
			lock_get = 1;

	if ((*plogger)->opt & OPT_KERN_LOG)
		kern_dump = 1;

	skb = skb_copy(pskb, GFP_ATOMIC);
	data = (u8 *)skb_mac_header(skb);

	if (skb_is_nonlinear(skb))
		len = skb->data_len;
	else
		len = skb->len;

	if (skb->data != data) {
		len += 14;
	}

	remaining = len + 2 + offset;

	if (kern_dump) {
		printk(KERN_INFO "=============== %s (len = %ld) ===============\n", type, len);
		for (i = 0; i < len; i += rowsize) {
			printk(KERN_INFO "%06d\t", li);

			linelen = min(remaining, rowsize);
			remaining -= rowsize;

			for (l = 0; l < linelen; l++) {
				ch = data[l];
				printk(KERN_CONT "%02X ", (uint32_t)ch);
			}

			data += linelen;
			li += 10;

			printk(KERN_CONT "\n");
		}

		printk(KERN_INFO "==========================================================\n\n");
	}

	data = (u8 *)skb_mac_header(skb);
	remaining = len + 2 + offset;
	if (lock_get) {
		sprintf(buf, "============= %s (len = %ld) =============\n", type, len);
		if(etl_write_file(plogger, buf) < 0)
			goto err_write;

		for (i = 0; i < len; i += rowsize) {
			memset(buf, '\0', LOGGER_BUF_SIZE);
			bufpos = 0;
			sprintf(buf, "%06d\t", li);
			bufpos = strlen(buf);

			linelen = min(remaining, rowsize);
			remaining -= rowsize;

			for (l = 0; l < linelen; l++) {
				ch = data[l];
				sprintf(buf + bufpos, "%02X ", (uint32_t)ch);
				bufpos = strlen(buf);
			}

			data += linelen;
			li += 10;

			sprintf(buf + bufpos, "\n");
			if(etl_write_file(plogger, buf) < 0)
				goto err_write;
		}

		sprintf(buf, "%s\n\n", "==========================================================");
		if(etl_write_file(plogger, buf) < 0)
			goto err_write;
	}

	(*plogger)->frames++;
	if ((*plogger)->frames_th) {
		if ((*plogger)->frames == (*plogger)->frames_th) {
			printk(KERN_INFO "etl: debug %s dump is finished, frame count is %d\n",
					(flag & ETL_DEBUG_TX_DUMP) ? "tx" : "rx",
					(*plogger)->frames);
			(*plogger)->frames = 0;
			(*plogger)->opt = 0;
			(*plogger)->pos = 0;

			if ((*plogger)->fp)
				filp_close((*plogger)->fp, NULL);

			(*plogger)->fp = NULL;
			mutex_destroy(&(*plogger)->etl_mutex);
		}
	}

	if (lock_get)
		mutex_unlock(&(*plogger)->etl_mutex);

err_write:
	kfree_skb(skb);
}

int etl_debug_log(enum DBG_LEVEL dbg_level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	if (dbg_level == ETL_DEBUG_MSG)
		trace_etl_dbg_msg(&vaf);

	va_end(args);

	return 0;
}

static void __etl_logger_constructor(struct etl_logger **plogger, char *s)
{
	*plogger = kmalloc(sizeof(struct etl_logger), GFP_KERNEL);

	debugfs_create_u32(s, 0644, etl_dentry, &(*plogger)->frames_th);

	(*plogger)->frames_th = 1;
	(*plogger)->frames = 0;
	(*plogger)->fp = NULL;
	(*plogger)->opt = 0;
}

void etl_register_debugfs(const char *logpath)
{
	etl_dentry = debugfs_create_dir("etunnel", NULL);

	/* Control inode for user
	 * debug_opt: debug option for user to decide
	 * tx_frame: how many frames should we dump tx
	 * rx_frame: how many frames shoud we dump in rx
	 * fop_debug_start: start to record/dump txrx frames
	 */
	debugfs_create_u32("debug_opt",  0644, etl_dentry, &etl_debug_opt);
	debugfs_create_file("start", 0644, etl_dentry, NULL, &fops_debug_start);

	__etl_logger_constructor(&tx_logger, "tx_frames");
	__etl_logger_constructor(&rx_logger, "rx_frames");

	sprintf(__logpath, "%s", logpath);
}

static void __etl_logger_destructor(struct etl_logger **plogger)
{
	if ((*plogger)->fp)
		filp_close((*plogger)->fp, NULL);

	if ((*plogger)->opt & OPT_RECORD)
		mutex_destroy(&(*plogger)->etl_mutex);

	kfree(*plogger);
}


void etl_unregister_debugfs(void)
{
	debugfs_remove_recursive(etl_dentry);
	__etl_logger_destructor(&tx_logger);
	__etl_logger_destructor(&rx_logger);
}
