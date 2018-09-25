/**
 * log_transfer.c
 * BUG: 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/sched.h>

#include <linux/ipmi.h>
#include <linux/ipmi_smi.h>

#include "kfault_int.h"

/**
 * Getting data length:
 * Request Message Format
 * Netfn/LUN    Cmd    Data0
 * 0x3a         0xb2   0x00
 *
 * Response Message Format
 * Netfn/LUN    Cmd    Data0        Data1~4            Data5~8
 * 0x3a+1       0xb2   CompleteCode FileOrignalSize    FileCompressSize
 * --------------------
 * Setting data length:
 * Request Message Format
 * Netfn/LUN    Cmd    Data0    Data1~4            Data5~8
 * 0x3a         0xb2   0x01     FileOrignalSize    FileCompressSize
 *
 * Response Message Format
 * Netfn/LUN    Cmd    Data0
 * 0x3a+1       0xb2   CompleteCode
 * --------------------------------
 * Getting data:
 * Request Message Format
 * Netfn/LUN    Cmd    Data0    Data1~4            Data5
 * 0x3a         0xb3   0x00     FileBufferOffset   FileBufferlength
 * 
 * Response Message Format
 * Netfn/LUN    Cmd    Data0        Data1~4            Data5            Data6~FileBufferlength
 * 0x3a+1       0xb3   CompleteCode FileBufferOffset   FileBufferlength Data
 * ----------------------
 * Setting data:
 * Request Message Format
 * Netfn/LUN    Cmd    Data0    Data1~4            Data5            Data6~FileBufferlength
 * 0x3a         0xb3   0x01     FileBufferOffset   FileBufferlength Data
 * 
 * Response Message Format
 * Netfn/LUN    Cmd    Data0
 * 0x3a+1       0xb3   CompleteCode
 */


typedef struct requestB3 {
 unsigned int FileBufferOffset;
 unsigned char FileBufferlength;
 unsigned char *data;
} __attribute__((packed)) requestB3_t;

typedef struct responseB3 {
 unsigned int FileBufferOffset;
 unsigned char FileBufferlength;
 unsigned char *data;
} __attribute__((packed)) responseB3_t;


typedef struct _globals {
 unsigned char cmd;
 unsigned char *buf;
 unsigned int reallen;
 unsigned char completecode;
} globals;


static globals global_data;

extern void kcs_send_data(struct kernel_ipmi_msg *message); 
extern void kcs_send_prepare(void);

#define MAX_LOG (1024*1024U)

#define CONFIG_SAVE_BMC      0x01
#define CONFIG_SAVE_BMC_MASK     0x01


static int  save_mode = 0;
module_param(save_mode, int, S_IRUGO);

static ipmi_user_t dump_user;

static void dummy_smi_free(struct ipmi_smi_msg *msg)
{
 //atomic_dec(&dummy_count);
}
static void dummy_recv_free(struct ipmi_recv_msg *msg)
{
 //atomic_dec(&dummy_count);
}
static struct ipmi_smi_msg dump_smi_msg = {
 .done = dummy_smi_free
};
static struct ipmi_recv_msg dump_recv_msg = {
 .done = dummy_recv_free
};

static void ipmi_msg_handler(struct ipmi_recv_msg *msg, void *handler_data)
{
 struct completion *comp = msg->user_msg_data;
 
 responseB3_t res_b3;
 
 if (global_data.cmd == 0xb2) {
  global_data.completecode = *(msg->msg.data);	
  
  memcpy(global_data.buf, msg->msg.data + 1, msg->msg.data_len - 1);
  global_data.reallen = msg->msg.data_len - 1;
  
  ipmi_free_recv_msg(msg);
  complete(comp);
 }
 else if (global_data.cmd == 0xb3) {
  global_data.completecode = *(msg->msg.data);		
  
  res_b3.FileBufferOffset = *(unsigned int *)(msg->msg.data + 1);
  res_b3.FileBufferlength = *(msg->msg.data + 5);
  res_b3.data = msg->msg.data + 6;
  
  memcpy(global_data.buf + res_b3.FileBufferOffset, res_b3.data, res_b3.FileBufferlength);
  global_data.reallen += res_b3.FileBufferlength; 
  
  ipmi_free_recv_msg(msg);
  complete(comp);
 }
}

static struct ipmi_user_hndl ipmi_hndlrs = {
 .ipmi_recv_hndl           = ipmi_msg_handler,
};


/**
 * flags: 0 stands for cmd 0xb2
 *        1 stabds for cmd 0xb3
 *
 * return value: 0 means all good, other value is not impossable.
 *
 * NOTE: BMC doesn't return the information of real write's data length, so here "reallen" is just "len". 
 */
static void __write_data(int flags, void *buf, int len, int *reallen)
{
 int times;
 int i;
 struct kernel_ipmi_msg            msg;
 requestB3_t req_b3;
 unsigned char  tmpbuf[512];
 
 msg.netfn = 0x3a;
 tmpbuf[0]=0x01;
 
 if (flags == 0) {
  msg.cmd = 0xb2;
  memcpy(&tmpbuf[1], buf, len);
  msg.data = tmpbuf;
  msg.data_len = len + 1;
  
  kcs_send_prepare();
  kcs_send_data(&msg);
 } else {
  msg.cmd = 0xb3;
  if (0 != len % (253-6))
   times = len / (253-6) + 1;
  else
   times = len / (253-6);
  
  kcs_send_prepare();
  
  for (i = 0; i < times; i++) {
   req_b3.FileBufferOffset = i * (253-6);
   req_b3.data = buf + req_b3.FileBufferOffset;
   if (i == times - 1)
    req_b3.FileBufferlength = len - (253-6) * (times - 1);
   else
    req_b3.FileBufferlength = 253-6;
   
   memcpy(&tmpbuf[1], &(req_b3.FileBufferOffset), 4);
   memcpy(&tmpbuf[5], &(req_b3.FileBufferlength), 1);
   memcpy(&tmpbuf[6], req_b3.data, req_b3.FileBufferlength);
   
   msg.data = tmpbuf;
   msg.data_len = 1 + 4 + 1 + req_b3.FileBufferlength;
   
   kcs_send_data(&msg);
  }
 }
 
 *reallen = len;
}

/**
 * flags: 0 stands for cmd 0xb2
 *        1 stabds for cmd 0xb3
 *
 * return value: 0 means all good, otherwise means BMC returns a non zero completecode or
 *               ipmi_request_supply_msgs make mistakes
 * 
 * NOTE: If the length of log in BMC less than "len", BMC should return the whole log (reallen)
 */
static int __read_data(int flags, void *buf, int len, int *reallen)
{
 int ret;
 int i;
 int times;
 
 struct ipmi_system_interface_addr addr;
 struct kernel_ipmi_msg            msg;
 requestB3_t req_b3;
 unsigned char  tmpbuf[512];
 
 struct completion comp;
 
 addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
 addr.channel = IPMI_BMC_CHANNEL;
 addr.lun = 0;
 
 msg.netfn = 0x3a;
 tmpbuf[0] = 0x00;
 
 if (flags == 0) {
  init_completion(&comp);
  
  global_data.cmd = 0xb2;
  
  msg.cmd = 0xb2;
  msg.data = tmpbuf;
  msg.data_len = 1;
  ret = ipmi_request_supply_msgs(dump_user, (struct ipmi_addr *) &addr, 0, &msg, &comp, &dump_smi_msg, &dump_recv_msg, 0);
  if (ret)
   return ret;
  
  wait_for_completion(&comp);
 } else {
  global_data.cmd = 0xb3;
  
  msg.cmd = 0xb3;
  if (0 != len % (253-6))
   times = len / (253-6) + 1;
  else
   times = len / (253-6);
  
  for (i = 0; i < times; i++) {
   reinit_completion(&comp);
   
   req_b3.FileBufferOffset = i * (253-6);
   if (i == times - 1)
    req_b3.FileBufferlength = (unsigned char)(len - (253-6) * (times - 1));
   else
    req_b3.FileBufferlength = (unsigned char)(253-6);
   
   memcpy(&tmpbuf[1], &(req_b3.FileBufferOffset), 4);
   tmpbuf[5] = req_b3.FileBufferlength;
   msg.data = tmpbuf;
   msg.data_len = 1 + 4 + 1;
   
   ret = ipmi_request_supply_msgs(dump_user, (struct ipmi_addr *) &addr, 0, &msg, &comp, &dump_smi_msg, &dump_recv_msg, 0);
   if (ret)
    return ret;
   
   wait_for_completion(&comp);
  }
 }
 
 memcpy(buf, global_data.buf, global_data.reallen);
 *reallen = global_data.reallen;
 
 return global_data.completecode;
}

/**
 * simple wrap: first write log's length, then write log itself.
 */
static int write_data(struct send_data *log, int *reallen)
{
 unsigned char tmp[64];
 
 if (log->compresslen > MAX_LOG) {
  printk("WRITE: Our log is too big, BMC doesn't have enough space. \n");
  return -ENOSPC;
 }
 
 memcpy(tmp, &(log->originlen), 4);
 memcpy(&tmp[4], &(log->compresslen), 4);
 
 __write_data(0, tmp, 8, reallen);
 __write_data(1, log->sendbuf, log->compresslen, reallen);
 
 return 0;
}

/**
 * simple wrap: first read log's length, then read log itself.
 */
static int read_data(struct recv_data *log, int *reallen)
{
 unsigned char tmp[64];
 int ret;
 
 ret = __read_data(0, tmp, 8, reallen);
 if (ret == 0xcc) {
  printk("BMC has no log!\n");
  return ret;
 } else if (ret)
  return ret;
 
 memcpy(&(log->originlen), tmp, 4);
 memcpy(&(log->compresslen), &tmp[4], 4);
 
 if (log->compresslen <= log->maxrecvlen) {
  ret = __read_data(1, log->recvbuf, log->compresslen, reallen);
  if (ret)
   return ret;
 }
 else {
  printk("READ: 'maxrecvlen' is not large enough to read the log in BMC. \n");
  return -ENOMEM;
 }
 
 return 0;
}

static struct si_handlers bmc_handler = {
 .write_data = write_data,
 .read_data = read_data,
};

static struct kfault_store_info bmc_media_info = {
 .save_mode = CONFIG_SAVE_BMC,
 .handlers	= &bmc_handler,
 .si_info_link	=	LIST_HEAD_INIT(bmc_media_info.si_info_link),
};


static int __init init_transfer(void)
{
 int ret;
 
 global_data.buf = kzalloc(MAX_LOG, GFP_KERNEL);
 if (global_data.buf == NULL)
  return -ENOMEM;
 
 global_data.reallen = 0;
 
 /** 
  * the first parameter "if_num" is 0 if system only has one kcs interface.
  * 
  * multiple SMI interfaces may be registered to the
  * message handler, each of these is assigned an interface number when
  * they register with the message handler.  They are generally assigned
  * in the order they register, although if an SMI unregisters and then
  * another one registers, all bets are off.
  */
 ret = ipmi_create_user(0, &ipmi_hndlrs, NULL, &dump_user);
 if (ret < 0) {
  printk("Unable to register with ipmi\n");
  return ret;
 }
 
 /* Determine whether the BMC supports log storage */
 {
  struct completion comp;
  struct ipmi_system_interface_addr addr;
  struct kernel_ipmi_msg            msg;
  unsigned char  tmpbuf[512];
  
  init_completion(&comp);
  
  global_data.cmd = 0xb2;
  
  addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
  addr.channel = IPMI_BMC_CHANNEL;
  addr.lun = 0;
  
  tmpbuf[0] = 0x00;
  
  msg.netfn = 0x3a;
  msg.cmd = 0xb2;
  msg.data = tmpbuf;
  msg.data_len = 1;
  
  ret = ipmi_request_supply_msgs(dump_user, (struct ipmi_addr *) &addr, 0, &msg, &comp, &dump_smi_msg, &dump_recv_msg, 0);
  if (ret)
   return ret;
  
  wait_for_completion(&comp);
  
  if (global_data.completecode != 0x00 && global_data.completecode != 0xcc) {
   printk("completecode = %d, the BMC does NOT support log storage! \n", global_data.completecode);
   ipmi_destroy_user(dump_user);
   dump_user = NULL;
   kfree(global_data.buf);
   global_data.buf = NULL;
   
   return -ENODEV;
  }
 }
 
 if (save_mode & CONFIG_SAVE_BMC_MASK)
  register_store_media(&bmc_media_info);
 
 return 0;
}

static void __exit exit_transfer(void)
{
 int ret;
 
 kfree(global_data.buf);
 global_data.buf = NULL;
 
 if (save_mode & CONFIG_SAVE_BMC_MASK)
  unregister_store_media(&bmc_media_info);
 
 ret = ipmi_destroy_user(dump_user);
 if (ret)
  printk("%s error, ret = %d\n", __func__, ret);
 
 dump_user = NULL;
}

module_init(init_transfer);
module_exit(exit_transfer);
MODULE_LICENSE("GPL");

