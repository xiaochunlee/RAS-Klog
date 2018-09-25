
obj-m	:= klog.o klogdriver.o
klog-objs := klogparse.o kfault_core.o 
klogdriver-objs := log_transfer.o
all:
	make -C /lib/modules/`uname -r`/build M=${PWD} modules
	
clean:
	make -C /lib/modules/`uname -r`/build M=${PWD} clean
	
install:
	@install -D -m 500 klog.ko ${PATH_PREFIX}/lib/modules/`uname -r`/kernel/klog/klog.ko
	@install -D -m 500 klogdriver.ko ${PATH_PREFIX}/lib/modules/`uname -r`/kernel/klog/klogdriver.ko
	
uninstall:
	rm -rf ${PATH_PREFIX}/lib/modules/`uname -r`/kernel/klog
