#
# build & install & uninstall for klog
#

SUB_DIRS = service \
           kmod	   \
		   tools/get-abnormal-log

		   
define make_subdir
	@for subdir in $(SUB_DIRS);          \
	do                                   \
  		(cd $$subdir && make $1 $2)  \
	done;
endef
	
	
all:
	$(call make_subdir, all)
	
clean:
	$(call make_subdir, clean)
	
install:
	$(call make_subdir, install, PATH_PREFIX=${INSTALL_PATH})
	
uninstall:
	$(call make_subdir, uninstall, PATH_PREFIX=${INSTALL_PATH})
