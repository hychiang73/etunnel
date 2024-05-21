#ifndef _ETL_HYPERX_H_
#define _ETL_HYPERX_H_

#define HX_MAGIC_LEN 12

enum {
	HX_BA_INITIATOR = 0,
	HX_BA_RECIPIENT,
};

enum {
	HX_STA_DISCONNECTED = 0,
	HX_STA_CONNECTED,
};

enum {
	HX_CMD_RESERVED = 0,
	HX_CMD_BA_SET,
	HX_CMD_STA_INFO,
};

enum hxmsg_type {
	HXTYPE_BEACON = 0,
	HXTYPE_DATA,
	HXTYPE_CMD,
};

struct hxmsg_header {
	uint16_t type:4;
	uint16_t length:12;
	uint16_t seq;
};

struct minmhdr {
	struct ethhdr eth_hdr;
	struct iphdr ip_hdr;
	struct udphdr udp_hdr;
	u8 hx[HX_MAGIC_LEN];
	struct hxmsg_header msghdr;
} __attribute__((packed));

struct etl_tlv {
	__le16 tag;
	__le16 len;
} __packed;

struct tlv_sta_info {
	__le16 tag;
	__le16 len;
	u8 addr[6];
	u8 status;
	u8 reserved;
} __packed;

struct tlv_ba {
	__le16 tag;
	__le16 len;
	u8 sta_addr[6];
	u8 tid;
	u8 ba_type;
	u8 amsdu;
	u8 ba_en;
	__le16 ssn;
	__le16 ba_win_sz;
	u8 reserved[2];
} __packed;


#endif // _ETL_HYPERX_H_
