#ifndef _KFAULT_INT_H
#define _KFAULT_INT_H

#include <linux/list.h>
#include <linux/rtc.h>

#define CONFIG_SAVE_BMC_MASK     0x01

#define KLOG_WR_FLAG   0xaa55

enum event_type
{
 HARDDEADLOCK_EVENT,
 SOFTDEADLOCK_EVENT,
 PANIC_EVENT,
};


struct kfault_header
{
 int rec_count;
 unsigned long logflag;
 unsigned long reserved[4];
};

struct kfault_recorder
{
 enum event_type type; 
 struct rtc_time tm;    
 int kmsg_area_offset;
 int kmsg_area_size;
 int kernel_log_offset;
};



struct send_data {
 unsigned int originlen;
 unsigned int compresslen;
 unsigned char *sendbuf;
};

struct recv_data {
 unsigned int originlen;
 unsigned int compresslen;
 unsigned int maxrecvlen;
 unsigned char *recvbuf;
};

struct si_handlers {
 int (*write_data)(struct send_data *log, int *reallen);
 int (*read_data)(struct recv_data *log, int *reallen);
};

struct kfault_store_info {
 int           save_mode;
 struct si_handlers *handlers;
 struct list_head   si_info_link;
};

extern void register_store_media(struct kfault_store_info * si_media);
extern void unregister_store_media(struct kfault_store_info * si_media);

#endif /* _KFAULT_INT_H */
