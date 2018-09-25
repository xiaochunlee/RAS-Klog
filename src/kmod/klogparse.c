/*
 *   /proc/klog
 *
 *  Copyright (C) 2017  by guanhj
 *
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/syslog.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/rtc.h>
#include <linux/vmalloc.h>

#include "kfault_int.h"

extern struct list_head si_infos;

//#define DEBUG   1

int parse_kfault_syslog(struct seq_file *m, char * kbuf)
{
 int msg = 0;
 size_t sysloglen = 0;
 sysloglen = *((size_t *)kbuf);
 
#ifdef DEBUG
 printk("parse_kfault_syslog sysloglen = %ld\n", sysloglen);
#endif
 if (sysloglen > 0)
  seq_printf(m, "kernel log info : \n");	
 
 for(msg = 0; msg < sysloglen; msg++)
 {
  seq_putc(m, kbuf[msg+8]);
 }
 
 return 0;
 
}


int parse_kfault_record(struct seq_file *m, struct kfault_recorder * recordinfo, char * tempbuf)
{
 int i = 0;
 
 switch(recordinfo->type)
 {
  case HARDDEADLOCK_EVENT:
   seq_printf(m, "die info : hard deadlock \n");  
   break;
  case SOFTDEADLOCK_EVENT:
   seq_printf(m, "die info : soft deadlock \n");
   break;
  case PANIC_EVENT:
   seq_printf(m, "die info : panic \n");
   break;
  default:
   seq_printf(m, "die info : unknown reason \n");
 }
 seq_printf(m, "die time : UTC  %02d:%02d:%02d %02d/%02d/%04d\n",
  recordinfo->tm.tm_hour, recordinfo->tm.tm_min,
 	recordinfo->tm.tm_sec, recordinfo->tm.tm_mon + 1,
 	recordinfo->tm.tm_mday,
 	recordinfo->tm.tm_year + 1900); 

 if (recordinfo->kmsg_area_size > 0)
 {
  for (i = 0; i < recordinfo->kmsg_area_size; i++)
   seq_putc(m, *(tempbuf+recordinfo->kmsg_area_offset+i));
 }
 
 
 parse_kfault_syslog(m, tempbuf+recordinfo->kernel_log_offset);
 
 return 0;
}


//static LIST_HEAD(si_infos);
static int klog_proc_show(struct seq_file *m, void *v)  
{
 struct list_head *pos, *node;
 struct kfault_store_info *f = NULL;
 int status = 0;
 int reallen = 0; 
 char * tempbuf = NULL;
 struct kfault_header  * headinfo = NULL; 
 struct kfault_recorder * recordinfo = NULL;
 int ret = 0;
 int i = 0;
 
 struct recv_data recvlog;
 
 
 if (!list_empty(&si_infos))
 {
  tempbuf = vmalloc(1<<20);
  if (!tempbuf)
   return -ENOMEM;
  
  recvlog.maxrecvlen = (1<<20);
  recvlog.recvbuf = tempbuf;
  
  list_for_each_safe(pos, node, &si_infos) 
  {
   f = list_entry(pos, struct kfault_store_info, si_info_link);
   status = f->handlers->read_data(&recvlog, &reallen);
   //if (status == OK)
   if (status == 0)
    break;
  }
  
 }
 else 
  return 0;


 headinfo = (struct kfault_header *)tempbuf;
 
 if (headinfo->logflag != 0xaa55)
 {
  vfree(tempbuf);
  return 0;
 }

#ifdef DEBUG
 printk("klog_ read : headinfo->rec_count  = %d \n", headinfo->rec_count);
#endif
 if (headinfo->rec_count > 0)
 {
  for(i = 0; i < headinfo->rec_count; i++)
  {
   recordinfo = (struct kfault_recorder *)(tempbuf + 
                 sizeof(struct kfault_header) + i * sizeof(struct kfault_recorder));
#ifdef DEBUG
   printk("recordinfo : type : %d, kmsg_area_offset = 0x%x, 
           kmsg_area_size = 0x%x, kernel_log_offset = 0x%x\n",
           recordinfo->type, recordinfo->kmsg_area_offset, 
           recordinfo->kmsg_area_size, recordinfo->kernel_log_offset);
#endif
   
   
   ret = parse_kfault_record(m, recordinfo, tempbuf);
   if (ret < 0)
    break;
  }
 }

 vfree(tempbuf);
 
 if (ret < 0)
  return ret;
 
 return 0;
}

static int klog_open(struct inode * inode, struct file * file)
{
 return single_open(file, klog_proc_show, NULL);  
}



static const struct file_operations proc_klog_operations = {
 .owner = THIS_MODULE,
 .read = seq_read,
 .llseek = seq_lseek,
 .open = klog_open,
 .release = single_release,
};


int proc_klog_init(void)
{
 proc_create("klog", S_IRUSR, NULL, &proc_klog_operations);
 return 0;
}
int proc_klog_exit(void)
{
 remove_proc_entry("klog",  NULL);
 return 0;
}
//module_init(proc_kmsg_init);
