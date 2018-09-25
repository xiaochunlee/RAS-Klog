#ifndef __GET_ABNORNAL__
#define __GET_ABNORNAL__
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

//extern "C++" // templates cannot be declared to have 'C' linkage  
//template <typename T, size_t N>  
//char (*RtlpNumberOf( UNALIGNED T (&)[N] ))[N];  
//#define RTL_NUMBER_OF_V2(A) (sizeof(*RtlpNumberOf(A))) 
#define ARRAYSIZE(A)    RTL_NUMBER_OF_V2(A)

//#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + sizeof(typeof(int[1 - 2*!!__builtin_types_compatible_p(typeof(arr),typeof(&arr[0]))]))*0) 
//#define ARRAY_SIZE(x) (sizeof(x) / sizeof((((typeof(x)){})[0]))

//#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0] + sizeof(struct { int:-!!(__builtin_types_compatible_p(typeof(arr), typeof(&(arr)[0]))); })))
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                      \
                const typeof(((type *)0)->member) *__mptr = (ptr);    \
                (type *)((char *)__mptr - offsetof(type,member));})

#define pr_info(fmt, args...) \
        fprintf(stdout, "INSPUR INFO: " fmt, ## args)

#define pr_err(fmt, ...) \
        fprintf(stderr, "INSPUR ERROR: " fmt, ##__VA_ARGS__)

#define pr_warn(fmt, ...) \
        fprintf(stderr, "INSPUR WARN: " fmt, ##__VA_ARGS__)

//#define __DEBUG__ 1
#ifdef __PARSE__
#define __PARSE_HEAD__ 1
#endif

//#define LOG_RAM_PARA  "kmsg"

#ifdef __DEBUG__
#define pr_debug(fmt, ...) \
        fprintf(stdout, "INSPUR DEBUG: " fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...)
#endif

#define NAME_SIZE 256
#define MAX_THREAD_COUNT        256
#define LOG_PATH        "/var/log/klog"
#define CONFIG_LOG_PATH "/etc/klog/getlog_config"
#define LOG_RAM         LOG_RAM_PARA

struct linux_dirent {
        long d_ino;
        off_t d_off;
        unsigned short d_reclen;
        char d_name[];
};

enum operation {
        READ,
        WRITE,
};

struct inspur_platform_data {
        char dst[NAME_SIZE];
        char *src;
};

struct dir_id {
        char name[NAME_SIZE];
        uint32_t data;
};

struct instance {
        char *name;
        int id;
        struct list_head list;          //hang to the struct inspur_component
        struct list_head app_list;      //hang to the struct application
        const struct dir_id *id_entry;
        void *app_data;                         //which app is in the instance
        void *platform_data;            //what characteristic about self
        void *tcom_data;                        //point to the top component 

};

struct application {
        char *name;
        const struct dir_id *id_table;  //what inst can match the app
        struct list_head list;          //hang to the struct inspur_component
        struct list_head inst_list;     //for the struct instance hanging 
        int (*probe) (struct instance *);
        int (*remove) (struct instance *);
        int recnt;                                      //record the application have uesed
};

struct inspur_component {
        struct list_head instance_list;
        struct list_head application_list;
        pthread_spinlock_t spin_inst;   //mutex when operate the instance_list
        pthread_spinlock_t spin_app;    //mutex when operate the application_list
        char *config;

};

struct file_component {
        char *src;
        char *dst;
        pthread_mutex_t *write_lock;
        pthread_mutex_t *read_lock;
};

struct top_component {
        struct instance *inst;
        struct application *app;
        pthread_mutex_t write_lock;
        pthread_mutex_t read_lock;
        struct file_component fpc;
        struct inspur_component *comp;
        int (*file_op) (struct file_component *);       //point to the defined file operation
        char status;
};

#endif
