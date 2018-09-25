
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/kmsg_dump.h>
#include <linux/smp.h>
#include <linux/utsname.h>
#include <linux/kallsyms.h>
#include <asm/processor.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <asm/debugreg.h>
#include <linux/ptrace.h>
#include <linux/stacktrace.h>

#include <linux/percpu.h>
#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/mce.h>
#include <asm/hw_irq.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>


#include "kfault_int.h"

/*      
1M area format:
----------------------
0~2K  recorder  header info
----------------------

area 0 ~ 5

---------------------
every area have 128K storage space. 
32k kernel log at the beginning of area.

*/

//#define  DEBUG  0

enum storage_type
{
 STORE_BMC,
 STORE_NVRAM,
 STORE_NET,
};

#define MAX_AREA_ID   5
#define KFAULT_RECORDER_HEAD_LENGTH  2048
#define KFAULT_AREA_LENGTH                      (128*1024)
#define KMSG_LOG_LENGTH                           (32*1024)
#define KERN_STACK_LENGTH    (32*1024)

#define  KMSG_KLOG_GAP   32

struct kfault_dumper {
 enum event_type type;
 int length;
 unsigned long timestamp;
 char *  databuf;
};

static int event_config = 1;
static int save_mode = 1;

module_param(event_config, int, 0);
module_param(save_mode, int, 0);

//LIST_HEAD(si_infos);
struct list_head si_infos;

#define CONFIG_PANIC_MASK        0x01
#define CONFIG_SOFTLOCK_MASK     0x02
#define CONFIG_HARDLOCK_MASK     0x04

#define CONFIG_SAVE_BMC_MASK     0x01
#define CONFIG_SAVE_NVM_MASK     0x02


//module_param(save_mode, int, 0);

//static int g_kmsgareaid = 0;
//static int g_kernstackid = 0;
//static int g_recordcount = 0;

struct kfault_header * g_khead;

#define MAX_STACK_TRACE_DEPTH   64


#define __KFAULT_LOG_BUF_LEN (1 << 20)
static char _kfault_log_buf[__KFAULT_LOG_BUF_LEN] __aligned(4);

//static u32 log_buf_len = __KFAULT_LOG_BUF_LEN;
//static u32 log_next_idx = 0;;

static char *kfault_log_buf = _kfault_log_buf;


static void kfault_get_kernlog(struct kmsg_dumper *dumper,
         enum kmsg_dump_reason reason);

static struct kmsg_dumper kfault_kmsg_dumper = {
 .dump = kfault_get_kernlog
};

#define KFAULT_KMSG_LEN       (32 * 1024)

static char *kfault_kernlog_data;
static size_t kfault_data_sz;

static void kfault_get_kernlog(struct kmsg_dumper *dumper,
         enum kmsg_dump_reason reason)
{
 bool ret = false;
 size_t text_len;
 
 switch (reason) {
 case   KFAULT_DUMP_PANIC:
 case   KFAULT_DUMP_HARDDEADLOCK:
 case   KFAULT_DUMP_SOFTDEADLOCK:
  break;
 default:
  //pr_err("%s: ignoring unrecognized KMSG_DUMP_* reason %d\n", __func__, (int) reason);
  return;
 }
 
#ifdef DEBUG
 printk("kfautl get kernel log begin  \n");
#endif
 
 kfault_data_sz = KFAULT_KMSG_LEN;
 
 ret = kmsg_dump_get_buffer(dumper, false, &kfault_kernlog_data[8], kfault_data_sz-8, &text_len);
 if (!ret)
  pr_err(" kfault :get kmsg err\n");
 
 memcpy(kfault_kernlog_data, &text_len, 8);
 
#ifdef  DEBUG
 printk("kfautl get kernel log :  text_len = %ld\n", text_len);
 for(i = 0; i < text_len; i++)
  printk("%c", kfault_kernlog_data[i+8]);
 printk("kfautl get kernel log  end \n");
#endif
 
 return;
 

}

void save_mc_regs(struct pt_regs * regs, void * regsbuf, int * len)
{
 int count = 0;
 unsigned long cr0 = 0L, cr2 = 0L, cr3 = 0L, cr4 = 0L, fs, gs, shadowgs;
 unsigned long d0, d1, d2, d3, d6, d7;
 unsigned int fsindex, gsindex;
 unsigned int ds, cs, es;
 
 count += sprintf(regsbuf + count,  "RIP: %04lx:[<%016lx>] ", regs->cs & 0xffff, regs->ip);
 count += sprintf(regsbuf + count,  "RSP: %04lx:%016lx  EFLAGS: %08lx\n", regs->ss,
  regs->sp, regs->flags);
 count += sprintf(regsbuf + count,  "RAX: %016lx RBX: %016lx RCX: %016lx\n",
  regs->ax, regs->bx, regs->cx);
 count += sprintf(regsbuf + count,  "RDX: %016lx RSI: %016lx RDI: %016lx\n",
  regs->dx, regs->si, regs->di);
 count += sprintf(regsbuf + count,  "RBP: %016lx R08: %016lx R09: %016lx\n",
  regs->bp, regs->r8, regs->r9);
 count += sprintf(regsbuf + count,  "R10: %016lx R11: %016lx R12: %016lx\n",
  regs->r10, regs->r11, regs->r12);
 count += sprintf(regsbuf + count,  "R13: %016lx R14: %016lx R15: %016lx\n",
  regs->r13, regs->r14, regs->r15);
 
 asm("movl %%ds,%0" : "=r" (ds));
 asm("movl %%cs,%0" : "=r" (cs));
 asm("movl %%es,%0" : "=r" (es));
 asm("movl %%fs,%0" : "=r" (fsindex));
 asm("movl %%gs,%0" : "=r" (gsindex));
 
 rdmsrl(MSR_FS_BASE, fs);
 rdmsrl(MSR_GS_BASE, gs);
 rdmsrl(MSR_KERNEL_GS_BASE, shadowgs);
 
 cr0 = read_cr0();
 cr2 = read_cr2();
 cr3 = read_cr3();
 cr4 = read_cr4();
 
 count += sprintf(regsbuf + count,  "FS:  %016lx(%04x) GS:%016lx(%04x) knlGS:%016lx\n",
  fs, fsindex, gs, gsindex, shadowgs);
 count += sprintf(regsbuf + count,  "CS:  %04x DS: %04x ES: %04x CR0: %016lx\n", cs, ds,
  es, cr0);
 count += sprintf(regsbuf + count,   "CR2: %016lx CR3: %016lx CR4: %016lx\n", cr2, cr3,
  cr4);
 
 get_debugreg(d0, 0);
 get_debugreg(d1, 1);
 get_debugreg(d2, 2);
 count += sprintf(regsbuf + count,  "DR0: %016lx DR1: %016lx DR2: %016lx\n", d0, d1, d2);
 get_debugreg(d3, 3);
 get_debugreg(d6, 6);
 get_debugreg(d7, 7);
 count += sprintf(regsbuf + count,  "DR3: %016lx DR6: %016lx DR7: %016lx\n", d3, d6, d7);
 
 if (len)
  *len = count;
 
 return;
}

void save_kernel_stack(struct kfault_recorder * panic_rec, void * stackbuf, int *len)
{
 unsigned long sp0 = current->thread.sp0;
 unsigned long sp = current->thread.sp;
 int i = 0 ;
 int j = (sp0 -sp) << 3;
 int count = 0;
 int newrec = 0;
 
 count += sprintf(stackbuf + count, " kfault kernel stack : \n");
 /* every line have 4  values, every value have 8bytes, max 50 lines  */
 for(i = 0; (i < j) && (newrec < 200); i = i + 8)
 {
  count += sprintf(stackbuf + count, "  %016lx  ",  *(unsigned long *)(sp + i));
  newrec++;
  if ((newrec % 4) == 0)
   count += sprintf(stackbuf + count, "\n");
 }
 
 return ;

}

void save_call_trace(struct kfault_recorder * panic_rec, void * tracebuf, int *len)
{
 int count = 0;
 int i =  0;
 struct stack_trace trace;
 unsigned long  arr_entries[MAX_STACK_TRACE_DEPTH];
 
 unsigned long * entries = (unsigned long *)&arr_entries;
 
 count += sprintf(tracebuf + count, " kfault Call Trace : \n");
 
 trace.nr_entries    = 0;
 trace.max_entries   = MAX_STACK_TRACE_DEPTH;
 trace.entries       = entries;
 trace.skip          = 0;
 
 save_stack_trace_tsk(current, &trace);
 for (i = 0; i < trace.nr_entries; i++) {
  count += sprintf(tracebuf + count, "[<%pK>] %pS\n",
      (void *)entries[i], (void *)entries[i]);
 }
 
 if (len)
  *len = count;
 
 return ;

}

extern void output_modules(void * modbuf, int * len);

void save_kfault_module_info(struct kfault_recorder * panic_rec, void * modbuf, int *len)
{
 int count = 0;
 int modlen = 0;
 
 count += sprintf(modbuf + count, " kfault get modules info : \n");
 
 output_modules((void*)(modbuf + count), &modlen);
 
 if (len)
  *len = count + modlen;
 
 return;
}

extern int nr_irqs;
#define irq_stats(x)    (&per_cpu(irq_stat, x))

void save_kfault_interrputs(struct kfault_recorder * panic_rec, void * intrbuf, int *len)
{
 int prec = 0;
 unsigned long flags, any_count = 0;
 int i, j;
 struct irqaction *action;
 struct irq_desc *desc;
 int count = 0;
 unsigned int (*kstat_irqs_cpu)(unsigned int, int);
 unsigned long fn_addr;
 
 atomic_t *irq_err_count;
 unsigned long irq_err_count_addr;
 
 void (*x86_platform_ipi_callback)(void);
 unsigned long fn_plat_addr;

#ifdef CONFIG_X86_MCE

 unsigned long *mce_exception_count;
 unsigned long mce_exception_count_addr;
 
 unsigned long *mce_poll_count;
 unsigned long mce_poll_count_addr;

#endif

#if defined(CONFIG_X86_IO_APIC)
 atomic_t *irq_mis_count;
 unsigned long irq_mis_count_addr;
#endif

 count += sprintf(intrbuf + count, " kfault get interrupts info : \n");
 
 fn_addr = kallsyms_lookup_name("kstat_irqs_cpu");
 if (fn_addr)
 {
  kstat_irqs_cpu = (unsigned int (*)(unsigned int, int))fn_addr;
 }
 //raw_spin_lock_irqsave(&desc->lock, flags);

 for(i = 0; i < nr_irqs; i++)
 {
  if (i == 0) {
   for (prec = 3, j = 1000; prec < 10 && j <= nr_irqs; ++prec)
    j *= 10;
   
   count += sprintf(intrbuf + count,  "%*s", prec + 8, "");
   //for_each_online_cpu(j)
   j = 0;
   count += sprintf(intrbuf + count, "CPU%-8d", j);
   count += sprintf(intrbuf + count, "\n");
  }
  desc = irq_to_desc(i);
  if (!desc)
   continue;
  raw_spin_lock_irqsave(&desc->lock, flags);
  
  //for_each_online_cpu(j)
  j = 0;
  any_count |= kstat_irqs_cpu(i, j);
  action = desc->action;
  if (!action && !any_count)
  {
   raw_spin_unlock_irqrestore(&desc->lock, flags);
   continue;
  }
  count += sprintf(intrbuf + count, "%*d: ", prec, i);
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", kstat_irqs_cpu(i, j));
  
  if (desc->irq_data.chip) {
   //if (desc->irq_data.chip->irq_print_chip)
   //desc->irq_data.chip->irq_print_chip(&desc->irq_data, p);
   //else if (desc->irq_data.chip->name)
   if (desc->irq_data.chip->name)
    count += sprintf(intrbuf + count," %8s", desc->irq_data.chip->name);
   else
    count += sprintf(intrbuf + count, " %8s", "-");
  } else {
   count += sprintf(intrbuf + count, " %8s", "None");
  }
#ifdef CONFIG_GENERIC_IRQ_SHOW_LEVEL
  count += sprintf(intrbuf + count, " %-8s", 
              irqd_is_level_type(&desc->irq_data) ? "Level" : "Edge");
#endif
  if (desc->name)
   count += sprintf(intrbuf + count, "-%-8s", desc->name);
  
  if (action) {
   count += sprintf(intrbuf + count, "  %s", action->name);
   while ((action = action->next) != NULL)
    count += sprintf(intrbuf + count, ", %s", action->name);
  }
  
  count += sprintf(intrbuf + count, "\n");
  
  raw_spin_unlock_irqrestore(&desc->lock, flags);
 }
 
 if (i == nr_irqs)
 {
  count += sprintf(intrbuf + count, "%*s: ", prec, "NMI");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->__nmi_count);
  count += sprintf(intrbuf + count, "  Non-maskable interrupts\n");
#ifdef CONFIG_X86_LOCAL_APIC
  count += sprintf(intrbuf + count, "%*s: ", prec, "LOC");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->apic_timer_irqs);
  count += sprintf(intrbuf + count, "  Local timer interrupts\n");
  
  count += sprintf(intrbuf + count, "%*s: ", prec, "SPU");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->irq_spurious_count);
  count += sprintf(intrbuf + count, "  Spurious interrupts\n");
  count += sprintf(intrbuf + count, "%*s: ", prec, "PMI");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->apic_perf_irqs);
  count += sprintf(intrbuf + count, "  Performance monitoring interrupts\n");
  count += sprintf(intrbuf + count, "%*s: ", prec, "IWI");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->apic_irq_work_irqs);
  count += sprintf(intrbuf + count, "  IRQ work interrupts\n");
  count += sprintf(intrbuf + count, "%*s: ", prec, "RTR");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->icr_read_retry_count);
  count += sprintf(intrbuf + count, "  APIC ICR read retries\n");
#endif
  
  fn_plat_addr = kallsyms_lookup_name("x86_platform_ipi_callback");
  if (fn_plat_addr)
  {
   x86_platform_ipi_callback = (void (*)(void))fn_plat_addr;
   //}
   //if (x86_platform_ipi_callback) {
   count += sprintf(intrbuf + count, "%*s: ", prec, "PLT");
   //for_each_online_cpu(j)
   j = 0;
   count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->x86_platform_ipis);
   count += sprintf(intrbuf + count, "  Platform interrupts\n");
  }
  
#ifdef CONFIG_SMP
  count += sprintf(intrbuf + count, "%*s: ", prec, "RES");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->irq_resched_count);
  count += sprintf(intrbuf + count, "  Rescheduling interrupts\n");
  count += sprintf(intrbuf + count, "%*s: ", prec, "CAL");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->irq_call_count -
   irq_stats(j)->irq_tlb_count);
  count += sprintf(intrbuf + count, "  Function call interrupts\n");
  count += sprintf(intrbuf + count, "%*s: ", prec, "TLB");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->irq_tlb_count);
  count += sprintf(intrbuf + count, "  TLB shootdowns\n");
#endif
#ifdef CONFIG_X86_THERMAL_VECTOR
  count += sprintf(intrbuf + count, "%*s: ", prec, "TRM");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->irq_thermal_count);
  count += sprintf(intrbuf + count, "  Thermal event interrupts\n");
#endif
#ifdef CONFIG_X86_MCE_THRESHOLD
  count += sprintf(intrbuf + count, "%*s: ", prec, "THR");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10u ", irq_stats(j)->irq_threshold_count);
  count += sprintf(intrbuf + count, "  Threshold APIC interrupts\n");
#endif
#ifdef CONFIG_X86_MCE
  mce_exception_count_addr = kallsyms_lookup_name("mce_exception_count");
  if (mce_exception_count_addr)
  {
   mce_exception_count = (unsigned long *)mce_exception_count_addr;
  }
  mce_poll_count_addr = kallsyms_lookup_name("mce_poll_count");
  if (mce_poll_count_addr)
  {
   mce_poll_count = (unsigned long *)mce_poll_count_addr;
  }
  
  count += sprintf(intrbuf + count, "%*s: ", prec, "MCE");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10lu ", per_cpu(*mce_exception_count, j));
  count += sprintf(intrbuf + count, "  Machine check exceptions\n");
  count += sprintf(intrbuf + count, "%*s: ", prec, "MCP");
  //for_each_online_cpu(j)
  j = 0;
  count += sprintf(intrbuf + count, "%10lu ", per_cpu(*mce_poll_count, j));
  count += sprintf(intrbuf + count, "  Machine check polls\n");
#endif
  
  irq_err_count_addr = kallsyms_lookup_name("irq_err_count");
  if (irq_err_count_addr)
  {
   irq_err_count = irq_err_count_addr;
   count += sprintf(intrbuf + count, "%*s: %10u\n", prec, "ERR", atomic_read(irq_err_count));
  }
  //count += sprintf(intrbuf + count, "%*s: %10u\n", prec, "ERR", atomic_read(&irq_err_count));
#if defined(CONFIG_X86_IO_APIC)
  irq_mis_count_addr = kallsyms_lookup_name("irq_mis_count");
  if (irq_mis_count_addr)
  {
   irq_mis_count = irq_mis_count_addr;
   count += sprintf(intrbuf + count, "%*s: %10u\n", prec, "MIS", atomic_read(irq_mis_count));
  }
  
  //count += sprintf(intrbuf + count, "%*s: %10u\n", prec, "MIS", atomic_read(&irq_mis_count));
#endif
 
 }

 if (len)
  *len = count;

 return ;
}



static int kfault_panic_info_callback(struct notifier_block *nb, unsigned long val, void *data)
{
 struct timex  txc;
 int stacklen = 0;
 int tracelen = 0;
 //char * tempbuf = NULL;
 //struct rtc_time tm;
 struct kfault_recorder * panic_rec = NULL;
 unsigned long fn_addr;
 void (* kmsg_dump)(enum kmsg_dump_reason);
 char * kfault_panic_data = NULL;
 int count = 0;
 struct pt_regs * regs = NULL;
 int regslen = 0;
 int modlen = 0;
 
 struct list_head *pos, *node;
 struct kfault_store_info *f = NULL;
 int ret = 0;
 size_t text_len = 0;
 int len = 0;
 int reallen = 0;
 struct send_data sendlog;
 
 
#ifdef DEBUG
 int i = 0;
#endif


 if (g_khead->rec_count > MAX_AREA_ID)
 {
  pr_err(" no kfault area  can be saved\n ");
  return NOTIFY_DONE;
 }

 panic_rec = (struct kfault_recorder *)(kfault_log_buf + sizeof(struct kfault_header) + g_khead->rec_count * sizeof(struct kfault_recorder));
 /*  kfault recorder   */
 panic_rec->type = PANIC_EVENT;
 /* panic time  */
 do_gettimeofday(&(txc.time));
 rtc_time_to_tm(txc.time.tv_sec,&panic_rec->tm);
 panic_rec->kmsg_area_offset = KFAULT_RECORDER_HEAD_LENGTH + 
                               g_khead->rec_count * KFAULT_AREA_LENGTH;
 g_khead->rec_count++;
 kfault_panic_data = (char *) (kfault_log_buf + panic_rec->kmsg_area_offset);


 count += sprintf(kfault_panic_data + count, " CPU: %d PID: %d Comm: %.20s  %s %.*s\n",
  raw_smp_processor_id(), current->pid, current->comm,
  init_utsname()->release,
  (int)strcspn(init_utsname()->version, " "),
  init_utsname()->version);
 
 count += sprintf(kfault_panic_data + count, "task: %p ti: %p task.ti: %p\n", 
  current, current_thread_info(), task_thread_info(current));
 
 regs = current_pt_regs();
 if (regs)
 {
  count += sprintf(kfault_panic_data + count, " kfault task pt_regs : \n");
  save_mc_regs(regs, (void *)(kfault_panic_data + count), &regslen);
  count += regslen;
 }

 save_call_trace(panic_rec,  (void *)(kfault_panic_data +count), &tracelen);
 count += tracelen;
 
 save_kernel_stack(panic_rec, (void *)(kfault_panic_data +count), &stacklen);
 count += stacklen;
 
 save_kfault_module_info(panic_rec, (void *)(kfault_panic_data +count), &modlen);
 count += modlen;
 
 panic_rec->kmsg_area_size = count;
 
 /* kernel log  */
 panic_rec->kernel_log_offset = panic_rec->kmsg_area_offset + count + KMSG_KLOG_GAP;
 kfault_kernlog_data = (char *) (kfault_log_buf + panic_rec->kernel_log_offset);
 fn_addr = kallsyms_lookup_name("kmsg_dump");
 if (fn_addr)
 {
  kmsg_dump = (void (*)(enum kmsg_dump_reason))fn_addr;
 
  kmsg_dump(KFAULT_DUMP_PANIC);
 }

#ifdef DEBUG
 printk(" kfault get kernel msg  = %d\n", count);
 
 for(i = 0; i < count; i++)
  printk("%c",kfault_panic_data[i]);
 
 printk(" kfault get kernel msg  end \n");

#endif

 /* call bmc kcs interface */
 memcpy(&text_len, kfault_kernlog_data, 8);
 
 len = kfault_kernlog_data + text_len + 8 -  kfault_log_buf;
#ifdef DEBUG
 printk(" text_len = 0x%x, len = 0x%x \n",text_len, len);
#endif

 sendlog.originlen = len;
 sendlog.compresslen = len;
 sendlog.sendbuf = kfault_log_buf;
 
 g_khead->logflag = KLOG_WR_FLAG;
 list_for_each_safe(pos, node, &si_infos) 
 {
  f = list_entry(pos, struct kfault_store_info, si_info_link);
  ret = f->handlers->write_data(&sendlog, &reallen);
  if (ret != 0)
   pr_err(" write data error, f->save_mode = 0x%x \n", f->save_mode);
 }

 return NOTIFY_DONE;

}

#ifdef CONFIG_HARDLOCKUP_DETECTOR
static int kfault_harddeadlock_info_callback(struct notifier_block *nb, 
        unsigned long val, void *data)
{
 struct timex  txc;
 int stacklen = 0;
 int tracelen = 0;
 //char * tempbuf = NULL;
 //struct rtc_time tm;
 struct kfault_recorder * rlock_rec = NULL;
 unsigned long fn_addr;
 void (* kmsg_dump)(enum kmsg_dump_reason);
 char * kfault_rlock_data = NULL;
 int count = 0;
 struct pt_regs *regs = (struct pt_regs *)data;  
 int regslen = 0;
 int modlen = 0;
 int intrlen = 0;
 
 struct list_head *pos, *node;
 struct kfault_store_info *f = NULL;
 int ret = 0;
 size_t text_len = 0;
 int len = 0;
 int reallen = 0;
 struct send_data sendlog;
 
 unsigned long hard_fn_addr;
 int * hardlockup_panic;
 int hardflag = 0; 

#ifdef DEBUG
 int i = 0;
#endif

 if (g_khead->rec_count > MAX_AREA_ID)
 {
  pr_err(" no kfault area  can be saved\n ");
  return NOTIFY_DONE;
 }

 rlock_rec = (struct kfault_recorder *)(kfault_log_buf + sizeof(struct kfault_header) + g_khead->rec_count * sizeof(struct kfault_recorder));
 /*	kfault recorder */
 rlock_rec->type = HARDDEADLOCK_EVENT;
 /* panic time  */
 do_gettimeofday(&(txc.time));
 rtc_time_to_tm(txc.time.tv_sec, &rlock_rec->tm);
 rlock_rec->kmsg_area_offset = KFAULT_RECORDER_HEAD_LENGTH + 
                               g_khead->rec_count * KFAULT_AREA_LENGTH;
 g_khead->rec_count++;
 kfault_rlock_data = (char *) (kfault_log_buf + rlock_rec->kmsg_area_offset);

 count += sprintf(kfault_rlock_data + count, " CPU: %d PID: %d Comm: %.20s  %s %.*s\n",
  raw_smp_processor_id(), current->pid, current->comm,
  init_utsname()->release,
  (int)strcspn(init_utsname()->version, " "),
  init_utsname()->version);
 
 count += sprintf(kfault_rlock_data + count, "task: %p ti: %p task.ti: %p\n", 
  current, current_thread_info(), task_thread_info(current));
 
 //regs = current_pt_regs();
 if (regs)
 {
  count += sprintf(kfault_rlock_data + count, " kfault interrupt pt_regs : \n");
  save_mc_regs(regs, (void *)(kfault_rlock_data + count), &regslen);
  count += regslen;
 } else
 {
  regs = current_pt_regs();
  count += sprintf(kfault_rlock_data + count, " kfault task pt_regs : \n");
  save_mc_regs(regs, (void *)(kfault_rlock_data + count), &regslen);
  count += regslen;
 }

 save_call_trace(rlock_rec,	(void *)(kfault_rlock_data +count), &tracelen);
 count += tracelen;
 
 save_kernel_stack(rlock_rec, (void *)(kfault_rlock_data +count), &stacklen);
 count += stacklen;
 
 save_kfault_module_info(rlock_rec, (void *)(kfault_rlock_data +count), &modlen);
 count += modlen;
 
 save_kfault_interrputs(rlock_rec, (void *)(kfault_rlock_data +count), &intrlen);
 count += intrlen;
 
 rlock_rec->kmsg_area_size = count;
 
 rlock_rec->kernel_log_offset = rlock_rec->kmsg_area_offset + count + KMSG_KLOG_GAP;
 kfault_kernlog_data = (char *) (kfault_log_buf + rlock_rec->kernel_log_offset);


 hard_fn_addr = kallsyms_lookup_name("hardlockup_panic");
 if (hard_fn_addr)
 {
  hardlockup_panic = (int *)hard_fn_addr;
  hardflag = *hardlockup_panic;
 }
 else
  hardflag = 0;
 
 /* if hardlockup_panic == 1 , hardlockup invoke panic , save kernel log in panic callback */
 if (!hardflag)
 {
  //kfault_kernlog_data = (char *)(kfault_rlock_data + count + KMSG_KLOG_GAP);
  fn_addr = kallsyms_lookup_name("kmsg_dump");
  if (fn_addr)
  {
   kmsg_dump = (void (*)(enum kmsg_dump_reason))fn_addr;
  
   kmsg_dump(KFAULT_DUMP_HARDDEADLOCK);
  }
 }

#ifdef DEBUG
 printk(" kfault harddeadlock  kmsg : count = %d, intrlen = %d \n", count, intrlen);
 for(i = 0; i < count; i++)
  printk("%c",kfault_rlock_data[i]);
 
 printk(" kfault harddeadlock  kmsg  end \n");
#endif

 /* call bmc kcs interface */
 if (!hardflag)
 {
 
  memcpy(&text_len, kfault_kernlog_data, 8);
  
  len = kfault_kernlog_data + text_len + 8 - kfault_log_buf;
#ifdef DEBUG
  printk(" text_len = 0x%x, len = 0x%x \n",text_len, len);
#endif
  
  sendlog.originlen = len;
  sendlog.compresslen = len;
  sendlog.sendbuf = kfault_log_buf;
  
  g_khead->logflag = KLOG_WR_FLAG;
  list_for_each_safe(pos, node, &si_infos) 
  {
   f = list_entry(pos, struct kfault_store_info, si_info_link);
   ret = f->handlers->write_data(&sendlog, &reallen);
   if (ret != 0)
    pr_err(" write data error, f->save_mode = 0x%x \n", f->save_mode);
  }
 
 }


 return NOTIFY_DONE;
}

#endif

static int kfault_softdeadlock_info_callback(struct notifier_block *nb, 
        unsigned long val, void *data)
{
 struct timex  txc;
 int stacklen = 0;
 int tracelen = 0;
 //char * tempbuf = NULL;
 //struct rtc_time tm;
 struct kfault_recorder * rlock_rec = NULL;
 unsigned long fn_addr;
 void (* kmsg_dump)(enum kmsg_dump_reason);
 char * kfault_rlock_data = NULL;
 int count = 0;
 struct pt_regs *regs = (struct pt_regs *)data;
 int regslen = 0;
 int modlen = 0;
 int intrlen = 0;
 unsigned long soft_fn_addr;
 unsigned int * softlockup_panic;
 int softflag = 0; 
 
#ifdef DEBUG
 int i = 0;
#endif
 
 struct list_head *pos, *node;
 struct kfault_store_info *f = NULL;
 int ret = 0;
 size_t text_len = 0;
 int len = 0;
 int reallen = 0;
 struct send_data sendlog;

 if (g_khead->rec_count > MAX_AREA_ID)
 {
  pr_err(" no kfault area  can be saved\n ");
  return NOTIFY_DONE;
 }
 
 rlock_rec = (struct kfault_recorder *)(kfault_log_buf + sizeof(struct kfault_header) + g_khead->rec_count * sizeof(struct kfault_recorder));
 /* kfault recorder */
 rlock_rec->type = SOFTDEADLOCK_EVENT;
 /* panic time */
 do_gettimeofday(&(txc.time));
 rtc_time_to_tm(txc.time.tv_sec,&rlock_rec->tm);
 rlock_rec->kmsg_area_offset = KFAULT_RECORDER_HEAD_LENGTH + 
                               g_khead->rec_count * KFAULT_AREA_LENGTH;
 g_khead->rec_count++;
 kfault_rlock_data = (char *) (kfault_log_buf + rlock_rec->kmsg_area_offset);
 
 
 count += sprintf(kfault_rlock_data + count, " CPU: %d PID: %d Comm: %.20s  %s %.*s\n",
  raw_smp_processor_id(), current->pid, current->comm,
  init_utsname()->release,
  (int)strcspn(init_utsname()->version, " "),
  init_utsname()->version);
 
 count += sprintf(kfault_rlock_data + count, "task: %p ti: %p task.ti: %p\n", 
  current, current_thread_info(), task_thread_info(current));

 //regs = current_pt_regs();
 if (regs)
 {
  count += sprintf(kfault_rlock_data + count, " kfault interrupt pt_regs : \n");
  save_mc_regs(regs, (void *)(kfault_rlock_data + count), &regslen);
  count += regslen;
 } else
 {
  regs = current_pt_regs();
  count += sprintf(kfault_rlock_data + count, " kfault task pt_regs : \n");
  save_mc_regs(regs, (void *)(kfault_rlock_data + count), &regslen);
  count += regslen;
 }

 save_call_trace(rlock_rec,	(void *)(kfault_rlock_data + count), &tracelen);
 count += tracelen;
 
 save_kernel_stack(rlock_rec, (void *)(kfault_rlock_data +count), &stacklen);
 count += stacklen;
 
 save_kfault_module_info(rlock_rec, (void *)(kfault_rlock_data +count), &modlen);
 count += modlen;
 
 save_kfault_interrputs(rlock_rec, (void *)(kfault_rlock_data +count), &intrlen);
 count += intrlen;
 
 rlock_rec->kmsg_area_size = count;
 
 rlock_rec->kernel_log_offset = rlock_rec->kmsg_area_offset + count + KMSG_KLOG_GAP;
 kfault_kernlog_data = (char *) (kfault_log_buf + rlock_rec->kernel_log_offset);
 fn_addr = kallsyms_lookup_name("kmsg_dump");
 if (fn_addr)
 {
  kmsg_dump = (void (*)(enum kmsg_dump_reason))fn_addr;
  
  kmsg_dump(KFAULT_DUMP_SOFTDEADLOCK);
 }

#ifdef DEBUG
 printk(" kfault rdeadlock  kmsg : count = %d, intrlen = %d \n", count, intrlen);
 for(i = 0; i < count; i++)
  printk("%c",kfault_rlock_data[i]);
 
 printk(" kfault rdeadlock  kmsg  end \n");
#endif
 
 soft_fn_addr = kallsyms_lookup_name("softlockup_panic");
 if (soft_fn_addr)
 {
  softlockup_panic = (unsigned int *)soft_fn_addr;
  softflag = *softlockup_panic;
 }
 else
  softflag = 0;

 if (!softflag)
 {
  memcpy(&text_len, kfault_kernlog_data, 8);
  
  len = kfault_kernlog_data + text_len + 8 - kfault_log_buf;
#ifdef DEBUG
  printk(" text_len = 0x%x, len = 0x%x \n", text_len, len);
#endif
  
  sendlog.originlen = len;
  sendlog.compresslen = len;
  sendlog.sendbuf = kfault_log_buf;
  
  g_khead->logflag = KLOG_WR_FLAG;
  list_for_each_safe(pos, node, &si_infos) 
  {
   f = list_entry(pos, struct kfault_store_info, si_info_link);
   ret = f->handlers->write_data(&sendlog, &reallen);
   if (ret != 0)
    pr_err(" write data error, f->save_mode = 0x%x \n", f->save_mode);
  }

 }

	
 return NOTIFY_DONE;
}

#ifdef CONFIG_HARDLOCKUP_DETECTOR
static struct notifier_block kfault_harddeadlock_nb = {
 .notifier_call	= kfault_harddeadlock_info_callback,
};
#endif

static struct notifier_block kfault_softdeadlock_nb = {
 .notifier_call	= kfault_softdeadlock_info_callback,
};

static struct notifier_block kfault_panic_nb = {
 .notifier_call	= kfault_panic_info_callback,
};

extern int proc_klog_init(void);

static int __init kfault_init(void)   
{
 int rc;
#ifdef DEBUG
 printk(" kfault init   \n");
#endif
 
 INIT_LIST_HEAD(&si_infos);

 //if (handle_for_panic == 1)
 if (event_config & CONFIG_PANIC_MASK)
 {
  //#ifdef DEBUG
#if 1
  printk(" kfault init : kfault_panic_nb register \n");
#endif
  atomic_notifier_chain_register(&panic_notifier_list, &kfault_panic_nb);
 }

 if (event_config & CONFIG_HARDLOCK_MASK)
 {
#ifdef CONFIG_HARDLOCKUP_DETECTOR
#ifdef DEBUG
  printk(" kfault init : kfault_harddeadlock_nb register \n");
#endif
  atomic_notifier_chain_register(&kfault_Harddeadlock_decoder_chain, &kfault_harddeadlock_nb);
#endif
 }

 if (event_config & CONFIG_SOFTLOCK_MASK)
 {
 
#ifdef DEBUG
  printk(" kfault init : kfault_softdeadlock_nb register \n");
#endif
  atomic_notifier_chain_register(&kfault_Softdeadlock_decoder_chain, &kfault_softdeadlock_nb);
 
 }

 if (event_config & (CONFIG_PANIC_MASK | CONFIG_HARDLOCK_MASK | CONFIG_SOFTLOCK_MASK))
 {
  rc = kmsg_dump_register(&kfault_kmsg_dumper);
  if (rc != 0) {
   pr_err("kfault: kfault_kmsg_dumper() failed; returned %d\n", rc);
  }

 }


 g_khead = (struct kfault_header *) kfault_log_buf;
 memset(g_khead, 0, sizeof(struct kfault_header));
 
 proc_klog_init();
 
 return  0;

}

extern  int proc_klog_exit(void);

static void __exit kfault_exit(void) 
{

#ifdef DEBUG
 printk(" kfault exit  begin\n");
#endif
	
 if (event_config & CONFIG_PANIC_MASK)
 {
#ifdef DEBUG
  printk(" kfault kfault_exit :  kfault_panic_nb unregister \n");
#endif
  atomic_notifier_chain_unregister(&panic_notifier_list, &kfault_panic_nb);
 }

 if (event_config & CONFIG_HARDLOCK_MASK)
 {
#ifdef CONFIG_HARDLOCKUP_DETECTOR
  atomic_notifier_chain_unregister(&kfault_Harddeadlock_decoder_chain, &kfault_harddeadlock_nb);
#endif
 }


 if (event_config & CONFIG_SOFTLOCK_MASK)
 {
  atomic_notifier_chain_unregister(&kfault_Softdeadlock_decoder_chain, &kfault_softdeadlock_nb);
 
 }
 
 if (event_config & (CONFIG_PANIC_MASK | CONFIG_HARDLOCK_MASK | CONFIG_SOFTLOCK_MASK))
 {
 
  if (kmsg_dump_unregister(&kfault_kmsg_dumper) < 0)
   printk(KERN_WARNING "kfault: could not unregister kmsg_dumper\n");
 }
 
 proc_klog_exit();

#ifdef DEBUG
 printk(" kfault exit  end\n");
#endif

}


module_init(kfault_init);
module_exit(kfault_exit);


 
void register_store_media(struct kfault_store_info * si_media)
{
 printk("register_store_media begin si_media->save_mode = 0x%x\n", si_media->save_mode);
 list_add_tail(&si_media->si_info_link, &si_infos);
 
 printk("register_store_media end \n");
}

EXPORT_SYMBOL(register_store_media);
 
void unregister_store_media(struct kfault_store_info * si_media)
{
 struct list_head *pos, *node;
 struct kfault_store_info *f = NULL;
 
 list_for_each_safe(pos, node, &si_infos) 
 {
  f = list_entry(pos, struct kfault_store_info, si_info_link);
  if (f->save_mode == si_media->save_mode)
   list_del(pos);
 }
  
}
EXPORT_SYMBOL(unregister_store_media);
 
 
MODULE_LICENSE("GPL");   


