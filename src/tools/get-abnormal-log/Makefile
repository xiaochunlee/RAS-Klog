DEBUG=
PATH_FLAGS=
PARSE_FLAGS=
LOG_RAM	= $(interface)

ifeq ($(DEBUG),y)
	DEBUG_FLAGS += -D__DEBUG__ -O -g 
else
	DEBUG_FLAGS +=
endif

ifeq ($(parse),y)
	PARSE_FLAGS += -D__PARSE__ 
else
	PARSE_FLAGS +=
endif

ifeq ($(LOG_RAM),$(""))
	PATH_FLAGS += -DLOG_RAM_PARA=\"klog\"
else
	PATH_FLAGS += -DLOG_RAM_PARA=\"$(LOG_RAM)\" 
endif


CC		=gcc
LD		=ld 
AS		=as
AR		=AR

TARGET		= getlog
OBJS		+= get-abnormal-log.o config.o
LIB_FLAG	= -lpthread
DEPENDS 	= -Wall 
ADD_INCLUDE 	+=

DEPENDS		+= $(DEBUG_FLAGS)
DEPENDS		+=$(PATH_FLAGS)
DEPENDS		+=$(PARSE_FLAGS)

all:$(TARGET)

$(TARGET):$(OBJS)
	$(CC) -o $@ $(OBJS) $(LIB_FLAG) 

.SUFFIXES: .c .o .c  
.c.o:  
	$(CC) $(DEPENDS) -c $*.c -o $*.o 

.PHONE:clean
clean:
	@echo "Removing linked and compiled files......"
	rm $(OBJS) $(TARGET) -f
	
install:
	@install -D -m 700 getlog ${PATH_PREFIX}/usr/sbin/getlog
	@install -D -m 600 getlog_config ${PATH_PREFIX}/etc/klog/getlog_config
	
uninstall:
	rm -rf ${PATH_PREFIX}/usr/sbin/getlog
	rm -rf ${PATH_PREFIX}/etc/klog/getlog_config
