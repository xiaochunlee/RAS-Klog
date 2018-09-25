#
# install & uninstall for klog service
#

all:
clean:

install:
	@install -D -m 700 klog ${PATH_PREFIX}/usr/sbin/klog
	@install -D -m 700 klogload ${PATH_PREFIX}/usr/sbin/klogload
	@install -D -m 700 klogunload ${PATH_PREFIX}/usr/sbin/klogunload
	@install -D -m 600 config/config ${PATH_PREFIX}/etc/klog/config
	@install -D -m 600 config/global_conf ${PATH_PREFIX}/etc/klog/global_conf
	@install -D -m 600 klog.service ${PATH_PREFIX}/usr/lib/systemd/system/klog.service
	
uninstall:
	rm -rf ${PATH_PREFIX}/usr/sbin/klog
	rm -rf ${PATH_PREFIX}/usr/sbin/klogload
	rm -rf ${PATH_PREFIX}/usr/sbin/klogunload
	rm -rf ${PATH_PREFIX}/etc/klog
	rm -rf ${PATH_PREFIX}/usr/lib/systemd/system/klog.service