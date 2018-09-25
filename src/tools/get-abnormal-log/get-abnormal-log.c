#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/syscall.h>
#include "list.h"
#include "get-abnormal-log.h"
#include "cfg.h"

/*-------------------------------global--------------------------------------------*/
//static LIST_HEAD(instance_list);
static struct inspur_component comp = {
        .instance_list = LIST_HEAD_INIT(comp.instance_list),
        .application_list = LIST_HEAD_INIT(comp.application_list),
        .config = CONFIG_LOG_PATH,
};

/*---------------------------------------common func----------------------------------------*/

void getlocaltime(char *cmdret)
{
        const char *cmd = "date";
        FILE *pout;
        char line[256] = { 0 };

        pout = popen(cmd, "r");
        if (!pout) {
                pr_err("popen error\n");
                return;
        }

        if (fgets(line, sizeof(line), pout) != NULL) {
                strcpy(cmdret, line);
                if (cmdret[strlen(cmdret) - 1] == '\n')
                        cmdret[strlen(cmdret) - 1] = '\0';

                pr_debug("local time:%s\n", cmdret);

        }

        pclose(pout);
        return;

}

static FILE *open_file(char *name, int flag)
{
        FILE *fp = NULL;
        char *mode = NULL;
        //char errorinfo[256] = {0};

        //static int count = 0;

        if (flag) {
                //  if (0 == count) 
                //      mode = "w+";    
                //  else
                mode = "a+";

                //  ++count;
        } else
                mode = "r";

        fp = fopen(name, mode);
        if (fp == NULL) {
                //sprintf(errorinfo, "fopen %s", name);
                perror("fopen");
                return NULL;
        }

        return fp;

}

static int get_line(char **line, size_t * len, FILE * fp)
{
        int ret = 0;
        ret = getline(line, len, fp);
        //pr_debug("ret value:%d\n", ret);
        if (ret < 0) {
                //perror("getline");
                pr_debug("getline error or end of file\n");
        }
        //sleep(1);
        return ret;

}

static int write_line(char *buf, ssize_t len, FILE * fp)
{
        int ret = 0;
#ifdef __PARSE_HEAD__
        int i = 0;
        int j = 0;
        int n = len;

        for (i = 0; i < n; i++) {
                if ((i == 0 || buf[i - 1] == '\n') && buf[i] == '<') {
                        i++;
                        j++;
                        while (buf[i] >= '0' && buf[i] <= '9') {
                                i++;
                                j++;
                        }
                        if (buf[i] == '>') {
                                i++;
                                j++;
                        }
                }
        }
        pr_debug("i=%d,n=%d, j=%d\n", i, n, j);
        pr_debug("%s", buf + j);
        ret = fwrite(buf + j, 1, len - j, fp);
        if (ret != len - j) {
                perror("fwrite");
        }
#else

        ret = fwrite(buf, 1, len, fp);
        if (ret != len) {
                perror("fwrite");
        }
#endif
        return ret;

}

static int file_ops(struct file_component *fpc)
{
        FILE *fpr = NULL;
        FILE *fpw = NULL;
        char *line = NULL;
        size_t len = 0;
        ssize_t read = 0;
        ssize_t write = 0;
        int ret = 0;

        char buf[256] = { 0 };
        char localtime[256] = { 0 };
        getlocaltime(localtime);
        snprintf(buf, sizeof(buf), "############### %s %s###############\n", fpc->src, localtime);

        fpr = open_file(fpc->src, READ);
        if (!fpr)
                return -1;

        fpw = open_file(fpc->dst, WRITE);
        if (!fpw) {
                fclose(fpr);
                return -1;
        }
#ifdef __DEBUG__
        if (strcmp(fpc->src, "/proc/mem") == 0)
                sleep(5);
#endif

        pr_debug("...The lock:%p\n", fpc->write_lock);
        ret = pthread_mutex_lock(fpc->write_lock);
        if (ret != 0) {
                perror("pthread_mutex_lock");
                goto end;
        }

        write_line(buf, strlen(buf), fpw);
        while ((read = get_line(&line, &len, fpr)) != -1) {
                //write = fwrite(line, read, 1, fpw);
                write = write_line(line, read, fpw);
                if (write != read) {
                        pr_warn("write not over\n");
                }
                pr_debug("the write_lock:%p\n", fpc->write_lock);
                //pr_debug("Retrieved line of length %zu, writed:%d, len:%d :\n", read, write, len);
#ifdef __DEBUG__
                printf("%s", line);
#endif
        }

        ret = pthread_mutex_unlock(fpc->write_lock);
        if (ret != 0) {
                perror("pthread_mutex_lock");
                goto end;
        }

        pr_info("Dump %s to %s successed!\n", fpc->src, fpc->dst);

  end:
        if (line)
                free(line);

        fflush(fpw);
        fclose(fpw);
        fclose(fpr);

        return 0;

}

static int my_mkdir(const char *dirname, int *error_status)
{
        int ret = 0;
        ret = mkdir(dirname, 0755);
        if (ret == -1) {
                perror("mkdir");
                pr_debug("errno:%d\n", errno);

                if (EEXIST == errno) {
                        *error_status = EEXIST;
                }
        }

        return ret;

}

static int create_dir(const char *sPathName)
{
        char DirName[NAME_SIZE] = { 0 };
        int i = 0;
        int err_status = 0;
        int len = 0;

        len = strlen(sPathName);
        if (len >= NAME_SIZE) {
                pr_err("dirname is too long\n");
                return -1;
        }

        strcpy(DirName, sPathName);
        if (DirName[len - 1] != '/' && len + 1 < NAME_SIZE)
                strcat(DirName, "/");

        len = strlen(DirName);
        if (len >= NAME_SIZE) {
                pr_err("2,dirname is too long\n");
                return -1;
        }

        for (i = 1; i < len; i++) {
                if (DirName[i] == '/') {
                        DirName[i] = 0;
                        //if (access(DirName, F_OK) != 0)  { 
                        pr_info("dir:%s will be created\n", DirName);
                        //if (strcmp(DirName, "klog") == 0) {   
                        if (my_mkdir(DirName, &err_status) == -1) {
                                //pr_err("file exist\n");

                                if (EEXIST == err_status) {
                                        DirName[i] = '/';
                                        continue;
                                } else {
                                        return -1;
                                }
                                //}  
                        }
                        //}  

                        DirName[i] = '/';
                }
        }

        return 0;
}

static int check_dst_dir(const char *dst)
{
        int c;
        char chr;
        struct stat statbuf;
        int ret = 0;
        char file_name[NAME_SIZE] = { 0 };
        //char *dir_path = NULL;
        char dir_path[NAME_SIZE] = { 0 };
        char *temp_path1 = NULL;
        char *temp_path2 = NULL;

        //dir_path = strdup(dst);
        if (dst[strlen(dst)] != '\0')
                return -1;

        if (strlen(dst) < sizeof(dir_path))
                strcpy(dir_path, dst);
        else
                return -1;

        temp_path1 = strrchr(dir_path, '/');
        temp_path2 = strchr(dir_path, '/');

        if (temp_path1 != NULL) {
                if ('\0' == *(temp_path1 + 1)) {
                        pr_err("The %s is a directory!\n", dir_path);
                        return -1;
                }

                strcpy(file_name, temp_path1 + 1);

                /*judge like ./aa or /aa */
                if (temp_path1 != temp_path2) {
                        *temp_path1 = '\0';
                } else {
                        /* judge like bb/bb */
                        if (dir_path[0] == '.' || dir_path[0] == '/')
                                *(temp_path1 + 1) = '\0';
                        else
                                *temp_path1 = '\0';
                        //dir_path = "/";
                }
        } else {
                pr_info("The %s will be created in current directory\n", dir_path);
                strcpy(file_name, dir_path);
                strcpy(dir_path, "./");
                //dir_path = "./";
                //return -1;
        }

        /* first judge the directory is exist ? if dir_path is not exist,prompt create it, if exist then judge
         * is there have a file named dir_path, and if dir_path is exist, the final judge the file is exist under
         * the dir_path. and go to the next dir and do judgment。
         * */
        ret = stat(dir_path, &statbuf);
        //ret = access(dir_path, F_OK);
        pr_debug("dir:%s,file:%s, stat ret:%d\n", dir_path, dst, ret);
        if (ret) {
                pr_warn("Directory %s isn't exist, create it?(y/n) ", dir_path);
                do {
                        fflush(stdin);
                        c = getchar();

                } while (c != 'y' && c != 'Y' && c != 'n' && c != 'N');

                if (c == 'y' || c == 'Y') {
                        //ret = mkdir(dir_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                        ret = create_dir(dir_path);
                        if (ret) {
                                perror("create_dir");
                                return ret;
                        }

                } else if (c == 'n' || c == 'N') {
                        return -1;
                }
        } else {
                if (S_ISREG(statbuf.st_mode)) {
                        pr_err("%s is a file, Can not create a same name directory!\n", dir_path);
                        return -1;

                }

                /* if the dst file no exist, that ok, return */
                ret = stat(dst, &statbuf);
                if (ret) {

                        pr_debug("File %s not exist, Can create it\n", dst);
                        return 0;
                }

                if (S_ISDIR(statbuf.st_mode)) {
                        pr_err("%s is a directory, Can not create a same name file!\n", dst);
                        return -1;
                }

                /* if the directory have the same file, need to sure cover it */
                if (S_ISREG(statbuf.st_mode)) {
                        pr_warn("%s have a file named %s, ", dir_path, file_name);
                        fprintf(stderr, "Do you want to append it?(y/n) ");
                        do {
                                fflush(stdin);
                                //c = getchar();
                                //gets(&c);
                                //scanf("%[^\n]", &c);
                                scanf(" %c", &chr);
                                fflush(stdin);
                                //printf("%d\n", chr);
                                if (chr == 'y' || chr == 'Y') {
                                        return 0;
                                } else if (chr == 'n' || chr == 'N') {
                                        return -2;
                                } else {
                                        fprintf(stderr, " Please input y|Y or n|N. ");
                                        continue;
                                }
                        } while (1);
                }

                pr_info("%s isn't a file or directory\n", dst);
                return -3;
        }

        return 0;

}

static void get_rdwr_lock(struct file_component *fpc, struct top_component *tcom)
{
        struct instance *inst = tcom->inst;
        struct instance *tmp_inst = NULL;
        struct top_component *tmp_tcom = NULL;
        bool same_flag = false;

        struct inspur_platform_data *data2 = NULL;
        struct inspur_platform_data *data1 = (struct inspur_platform_data *)inst->platform_data;

        list_for_each_entry(tmp_inst, &comp.instance_list, list) {
                data2 = (struct inspur_platform_data *)tmp_inst->platform_data;
                pr_debug("%p<==>%p, %s<==>%s\n", inst, tmp_inst, data1->dst, data2->dst);
                if (tmp_inst == inst)
                        continue;

                /* if the instance dst have configed the same file, so the lock must also are the same, and there circle the list not need lock */
                if (strcmp(data1->dst, data2->dst) == 0) {
                        tmp_tcom = (struct top_component *)tmp_inst->tcom_data;
                        pr_debug("-------------------------------------\n");
                        if (tmp_tcom->fpc.write_lock != NULL) {
                                pr_debug("before ...%s write_lock:%p, fpc->write_lock:%p\n", data1->src,
                                                 &tcom->write_lock, fpc->write_lock);

                                fpc->write_lock = tmp_tcom->fpc.write_lock;
                                fpc->read_lock = tmp_tcom->fpc.read_lock;
                                //fpc->write_lock = &tmp_tcom->write_lock;
                                //fpc->read_lock = &tmp_tcom->read_lock;

                                pr_debug("after %s fpc->write_lock:%p\n", data1->src, fpc->write_lock);
                                same_flag = true;
                        }
                }
        }

        /* if the instance dst is not the same, so the new lock can be pointed */
        if (false == same_flag) {
                fpc->write_lock = &tcom->write_lock;
                fpc->read_lock = &tcom->read_lock;
        }

        pr_debug("after get_rdwr_lock %s fpc->write_lock:%p\n", data1->src, fpc->write_lock);

        return;
}

static int check_dir(struct top_component *tcom, int (*f_ops) (struct file_component *))
{

        struct instance *inst = tcom->inst;
        //struct application *dir = tcom->app;

        int pos = 0;
        int fd = 0;
        int ret = 0;
        //struct file_component *fpc; //every thread have a new fpc
        struct stat sb;
        char *buffer = NULL;
        int namesize = 0;
        int maxsize = 1 << (sizeof(short) * 8 - 1);
        int m_size = 0;

        struct linux_dirent *dent = NULL;
        struct inspur_platform_data *pd = inst->platform_data;

        char *dst = pd->dst;
        char *src = pd->src;

        //fpc.write_lock = NULL;
        //fpc.read_lock = NULL;

        /* inst->name is dirctory,  the inst->platform_data->src is a file under the dirctory */
        fd = open(inst->name, O_RDONLY | O_NONBLOCK | O_LARGEFILE | O_DIRECTORY | O_CLOEXEC);
        if (fd == -1) {
                perror("open");
                return fd;
        }

        ret = fstat(fd, &sb);
        if (ret == -1) {
                perror("fstat");
                close(fd);
                return ret;
        }

        /* if is the ram filesystem, the st_size is 0, so there give the maxsize */
        pr_debug("%s file size:%ld\n", inst->name, sb.st_size);
        if (sb.st_size > maxsize || sb.st_size == 0)
                m_size = maxsize;
        else
                m_size = sb.st_size;

        pr_debug("Will alloc size:%d\n", m_size);
        buffer = (char *)malloc(m_size);
        if (buffer == NULL) {
                perror("malloc");
                close(fd);
                return -1;
        }

        memset(buffer, 0, m_size);

        /* find the src file, use the systemcall getdents like readdir */
        for ( ; ; ) {
                ret = syscall(SYS_getdents, fd, buffer, m_size);
                if (ret == -1) {
                        perror("getdents");
                        goto f_err;
                }

                pr_debug("total length:%d\n", ret);

                /* the ret is a number of bytes read is returned.  On end of directory, 0 is returned.  On error,  -1
                 * is returned, and errno is set appropriately. */
                if (ret == 0)
                        break;

                for (pos = 0; pos < ret; ) {
                        dent = (struct linux_dirent *)(buffer + pos);
                        //pr_debug("name:%s,len:%d\n", dent->d_name, dent->d_reclen);
                        if (strcmp(dent->d_name, src) == 0) {
                                pr_debug("found %s%s\n", inst->name, src);
                                break;
                        }
                        //pr_debug("pos:%d\n", pos);
                        pos += dent->d_reclen;
                }

                /* if the circle exit，and the pos < ret, so we have found the src, then we exit the top circle */
                if (pos < ret) {
                        break;
                }

        }

        /* if the top circle is exit, and the ret == 0, then we not found the src under the directory */
        if (ret == 0) {
                pr_err("Not found %s%s\n", inst->name, src);
                ret = -1;
                goto f_err;
        }

        namesize = strlen(dst) + 1;
        tcom->fpc.dst = (char *)malloc(namesize);
        if (tcom->fpc.dst == NULL) {
                perror("malloc");
                ret = -1;
                goto f_err;
        }
        memset(tcom->fpc.dst, 0, namesize);

        if (dent == NULL) {
                goto f_err;
        }

        /* how many bytes we should malloc for the src */
        namesize = strlen(inst->name) + 1 + dent->d_reclen - 2 - offsetof(struct linux_dirent, d_name);
        //pr_debug("...inst->name len:%d, namesize:%d, offset:%d\n", strlen(inst->name), namesize, offsetof(struct linux_dirent, d_name));
        tcom->fpc.src = (char *)malloc(namesize);
        if (tcom->fpc.src == NULL) {
                perror("malloc");
                ret = -1;
                goto f_err1;
        }

        memset(tcom->fpc.src, 0, namesize);

        strcpy(tcom->fpc.src, inst->name);
        strcat(tcom->fpc.src, dent->d_name);

        strcpy(tcom->fpc.dst, dst);

        /* every fpc lock and tcom lock are in the same thread */
        //fpc.write_lock = &tcom->write_lock;
        //fpc.read_lock = &tcom->read_lock;

        get_rdwr_lock(&tcom->fpc, tcom);

        pr_debug("write file:%s, read file:%s\n", tcom->fpc.dst, tcom->fpc.src);

        /* go to the file operations */
        ret = f_ops(&tcom->fpc);
        if (ret) {
                pr_err("file ops error\n");
                goto f_err2;
        }

        free(tcom->fpc.src);
        free(tcom->fpc.dst);
        free(buffer);
        close(fd);

        return 0;

  f_err2:
        free(tcom->fpc.src);
  f_err1:
        free(tcom->fpc.dst);
  f_err:
        free(buffer);
        close(fd);

        return ret;

}

/*-------------------------------------inline func-----------------------------------------*/
#if 0
static inline struct inspur_component *app_to_comp(struct application *app)
{
        return container_of(app, struct inspur_component, app);
}

static inline struct inspur_component *inst_to_comp(struct application *inst)
{
        return container_of(app, struct inspur_component, inst);
}
#endif
/*---------------------------------probe and remove--------------------------------------*/
static int dir_probe(struct instance *inst)
{
        int ret = 0;
        struct top_component *tcom = inst->tcom_data;
        /* We should check src dir at here other than the begain of the main like check dst */
        ret = check_dir(tcom, tcom->file_op);
        if (ret) {
                pr_err("check dir error\n");
                return ret;
        }

        return ret;
}

/**
 * remove an app form the inspur component application_list when the app have not used
 * @inst: the defined inst that the app be used
 *
 * */
static int dir_remove(struct instance *inst)
{
        //struct instance *ins;
        struct application *app;
        struct application *appl = inst->app_data;
        struct list_head *pos, *tmp;

        /* circulate the app list, and delete each entry, need lock list, like unregister_instance */
        list_for_each_safe(pos, tmp, &comp.application_list) {
                app = list_entry(pos, struct application, list);
                //if (strcmp(app->name, appl->name) == 0 && !list_is_last(&app->list, &comp.application_list)) {
                //if (strcmp(app->name, appl->name) == 0 && (comp.application_list.next != &comp.application_list)) {
                if (strcmp(app->name, appl->name) == 0) {

                        pthread_spin_lock(&comp.spin_app);
                        //list_del_init(&app->list);
                        list_del(&app->list);
                        pthread_spin_unlock(&comp.spin_app);
                        app->probe = NULL;
                        app->remove = NULL;
                }
        }

        return 0;

}

/*----------------------------------thread-----------------------------------------------*/
static void *do_common(void *parameter)
{

        int ret = 0;
        struct top_component *tcom = (struct top_component *)parameter;
        struct instance *inst = tcom->inst;
        struct application *dir = tcom->app;
        pthread_mutex_init(&tcom->write_lock, NULL);
#if __DEBUG__
        struct inspur_platform_data *data = (struct inspur_platform_data *)(inst->platform_data);
        pr_debug("%s write_lock:%p\n", data->src, &tcom->write_lock);
#endif

        if (dir->probe) {
                ret = dir->probe(inst);
                if (ret) {
                        tcom->status = -1;
                        pr_err("probe error\n");

                }
        } else {

                pr_err("no probe func\n");
        }

        //sleep(5);
        pr_debug("before destroy write_lock:%p\n", &tcom->write_lock);
        pthread_mutex_destroy(&tcom->write_lock);
        pr_debug("after destroy write_lock:%p\n", &tcom->write_lock);

        pr_debug("tcom status:%d,address:%p\n", tcom->status, &tcom->status);
        return &tcom->status;
        //return NULL;
}

static int directory_match_id(const struct dir_id *id, struct instance *inst)
{
        while (id->name[0]) {
                pr_debug("..inst name:%s, id name:%s\n", inst->name, id->name);
                if (strcmp(inst->name, id->name) == 0) {
                        inst->id_entry = id;
                        /* return the id->data, We have set 0 */
                        return id->data;
                }
                id++;
        }

        return -ENODATA;
}

/**
 *      remove the defined app, when the app is not used we can remove it
 *      @dir: the defined app
 * */
int match_remove(struct application *dir)
{
        int ret = 0;
        struct instance *inst = NULL;
        struct top_component *tcom = NULL;

        list_for_each_entry(inst, &comp.instance_list, list) {
                if (directory_match_id(dir->id_table, inst) == 0) {

                        /* delete the inst's app_list form the application and recnt-- */
                        list_del_init(&inst->app_list);
                        dir->recnt--;
                        if (dir->recnt == 0 && dir->remove) {
                                pr_debug("No instance have use the applicaton, so remove it...\n");
                                ret = dir->remove(inst);
                        } else {
                                ret = -ENOENT;
                        }

                        tcom = inst->tcom_data;
                        pr_debug("Will free tcom:%p\n", tcom);
                        pr_debug("inst:%p, app:%p, write_lock:%p, read_lock:%p, fpc:%p, comp:%p, file_op:%p\n",
                                         tcom->inst, tcom->app, &tcom->write_lock, &tcom->read_lock, &tcom->fpc,
                                         tcom->comp, tcom->file_op);
                        pr_debug("fpc->write_lock:%p, fpc->read_lock:%p, %s<->%s\n", tcom->fpc.write_lock,
                                         tcom->fpc.read_lock, tcom->fpc.dst, tcom->fpc.src);
                        tcom->fpc.write_lock = NULL;
                        tcom->fpc.read_lock = NULL;
                        /* free the source, it create by the inst matched the app */
                        tcom->app = NULL;
                        tcom->inst = NULL;
                        tcom->comp = NULL;
                        tcom->file_op = NULL;
                        free(tcom);
                        tcom = NULL;
                        //pr_debug(".....inst:%p, app:%p, write_lock:%p, read_lock:%p, fpc:%p, comp:%p, file_op:%p\n", tcom->inst, tcom->app, &tcom->write_lock, &tcom->read_lock, &tcom->fpc, tcom->comp, tcom->file_op);

                        //sleep(10);

                }

        }

        return ret;

}

int match_probe(struct application *dir)
{
        int ret = 0;
        int thread_ret = 0;
        int s = 0;
        int i = 0;
        unsigned short count = 0;
        bool found = false;
        struct instance *inst = NULL;
        struct top_component *tcom = NULL;
        pthread_t pid[MAX_THREAD_COUNT] = { 0 };
        void *pthread_result;

        if (dir->id_table) {
                list_for_each_entry(inst, &comp.instance_list, list) {
                        ret = directory_match_id(dir->id_table, inst);
                        if (!ret) {
                                pr_debug("matched instance,instance name:%s\n", inst->name);
                                found = true;

                                list_add_tail(&inst->app_list, &dir->inst_list);
                                /* increase reference count */
                                dir->recnt++;

                                //memcpy(&comp.app, dir, sizeof(struct application));
                                //memcpy(&comp.inst, inst, sizeof(struct instance));

                                /* there can use the only one ptr named tcom, because when create the thread, the thread parameter point the tcom very fast， so when create the other thread, the tcom become available , but the resource must be release in the thread */
                                tcom = (struct top_component *)malloc(sizeof(struct top_component));
                                if (tcom == NULL) {
                                        perror("malloc");
                                        return -1;
                                }
                                tcom->comp = &comp;
                                tcom->inst = inst;
                                tcom->app = dir;
                                tcom->file_op = file_ops;
                                tcom->status = 0;
                                tcom->fpc.write_lock = NULL;
                                tcom->fpc.read_lock = NULL;

                                inst->tcom_data = tcom;
                                inst->app_data = dir;
                                //pr_debug("before sleep, inst name:%s:%p\n",inst->name, inst);
                                //if (count == 1) sleep(20);
                                ret = pthread_create(&pid[count], NULL, do_common, tcom);
                                if (0 != ret) {
                                        perror("pthread_create");
                                        return -1;
                                }

                                pr_debug("create the %d pthread:%ld\n", count, pid[count]);

                                count += 1;
                                if (count > MAX_THREAD_COUNT) {
                                        pr_err("thread count is limited %d\n", MAX_THREAD_COUNT);
                                        return -1;
                                }
                        } else {
                                pr_info("Do not match %s\n", inst->name);
                        }

                }

                /* if the circle exit,it will locate in the first head,and if found==false, so the app not corresponding instance */
                if (list_is_last(&inst->list, comp.instance_list.next) && found == false) {
                        pr_err("Not match instance\n");
                        return -ENODEV;
                }
        } else {
                pr_err("No name should match\n");
                return -1;
        }

        /* wait all child thread exit, then the main thread exit */
        for (i = 0; i < count; i++) {
                s = pthread_join(pid[i], &pthread_result);
                if (0 != s) {
                        perror("pthread_join");
                        return -1;
                }
                pr_debug("thread %ld exit, return address:%p,value:%d\n", pid[i], pthread_result,
                                 *(char *)pthread_result);

                //sleep(10);
                /* if anyone thread return error, then the main thread return error,but should wait all thread completed */
                if (*(char *)pthread_result == -1)
                        thread_ret = -1;
                //return -1;

                //free(pthread_result);
        }

        return thread_ret;
}

/*---------------------------------application-------------------------------------------*/

static const struct dir_id dir_ids[] = {
        {
         .name = "/proc/",
         .data = 0,
         },

        {
         .name = "/tmp/",
         .data = 0,
         },
        {},
};

static struct application dir_application = {
        .name = "inspur application",
        .probe = dir_probe,
        .remove = dir_remove,
        /* tell that what instance the application can match */
        .id_table = dir_ids,
        .inst_list = LIST_HEAD_INIT(dir_application.inst_list),
        .recnt = 0,
};

static void application_unregister(struct application *dir)
{

        pr_debug("unregister application:%s\n", dir->name);
        list_del_init(&dir->list);
        dir->probe = NULL;
        dir->remove = NULL;

        return;
}

static int application_register(struct application *dir)
{
        list_add_tail(&dir->list, &comp.application_list);

        return 0;
}

/*-----------------------------------instance----------------------------------------------*/

static struct inspur_platform_data pd_data1 = {
        //default "/var/log/klog"
        .dst = LOG_PATH,
        //.src = "klog",
        /* the LOG_RAM accept the parameter form the Makefile */
        .src = LOG_RAM,

};

static struct inspur_platform_data pd_data2 = {
        .dst = LOG_PATH,
        .src = "devices",

};

static struct instance dir_instance[] = {
        [0] = {
                   .name = "/proc/",
                   .id = 0,
                   .platform_data = (void *)&pd_data1,
                   },
        [1] = {
                   .name = "/tmp/",
                   .id = 1,
                   .platform_data = (void *)&pd_data2,
                   },
};

/* the numbers we will register into the system */
static struct instance *inspur_dir_instances[] = {
        &dir_instance[0],
        //&dir_instance[1],
};

static int instance_register(struct instance **inst)
{
        int i = 0;
        /*calculate how many the instance what we will add */
        struct instance (*in)[1] = (void *)inst;
        int num = ARRAY_SIZE(*in);
        //pr_debug("num:%d, %d/%d\n", num, sizeof(*in), sizeof(in[0][0]));
        for (i = 0; i < num; i++) {
                list_add_tail(&(inst[i]->list), &comp.instance_list);
        }

        return 0;
}

static void instance_unregister(struct instance **ins)
{
        struct instance *inst;
        /* circulate the instance list, and delete each entry, need lock list */
        while (!list_empty(&comp.instance_list)) {
                inst = list_first_entry(&comp.instance_list, struct instance, list);

                pr_debug("unregister instance:%s\n", inst->name);
                pthread_spin_lock(&comp.spin_inst);
                list_del_init(&inst->list);
                pthread_spin_unlock(&comp.spin_inst);

                inst->name = NULL;
                inst->id_entry = NULL;
                inst->app_data = NULL;
                inst->platform_data = NULL;
                inst->tcom_data = NULL;

        }

        return;
}

/*-----------------------------main----------------------------------------------------------*/
static void help(const char **argv)
{
        fprintf(stderr, "Can Usage:%s  [LOG PATH]\n Default log path is %s\n", argv[0], LOG_PATH);
        //exit(1);
}

static int read_config(char *path, int size)
{
        config_t *configfile = NULL;
        configfile = config_open(comp.config);
        if (configfile == NULL) {
                pr_debug("config file open failed\n");
                return -1;
        }

        if (config_query(configfile, "path/dst", path, size)) {
                pr_err("config file query error!\n");
                config_close(configfile);
                return -1;
        } else {
                pr_info("Read config sucessed.\n");
        }

        config_close(configfile);
        return 0;
}

static int prepare_job(int argc, const char **argv)
{
        int ret = 0;
        int c;
        char write_path[CONFIG_LINE_BUFFER_SIZE] = { 0 };
        /* read the configfile /etc/inspurlog.conf */
        ret = read_config(write_path, sizeof(write_path));

        if (argc <= 1 && ret) {
                help(argv);
                fprintf(stderr, "Do you want to use the default file?(y/n) ");
                do {
                        fflush(stdin);
                        c = getchar();

                } while (c != 'y' && c != 'Y' && c != 'n' && c != 'N');

                if (c == 'y' || c == 'Y') {
                        ret = check_dst_dir(LOG_PATH);
                        if (ret) {
                                //  help(argv);
                                return ret;
                        }

                } else if (c == 'n' || c == 'N') {
                        return -1;
                }
        } else if (argc > 1 && ret) {

                pr_info("Will use the command parameter:%s\n", argv[1]);
                ret = check_dst_dir(argv[1]);
                if (ret) {
                        help(argv);
                        return ret;
                }

                /* cover the default path */
                strcpy(pd_data1.dst, argv[1]);
                strcpy(pd_data2.dst, argv[1]);
        } else {
                /* find corresponding item from the config file */
                ret = check_dst_dir(write_path);
                if (ret) {
                        if (-1 == ret)
                                pr_err("config file path error\n");

                        return ret;
                }

                /* cover the default path */
                strcpy(pd_data1.dst, write_path);
                strcpy(pd_data2.dst, write_path);
        }

        return 0;

}

int main(int argc, const char *argv[])
{
        int ret = 0;

        /* first read the config file, if error, so have the default recommendation */
        ret = prepare_job(argc, argv);
        if (ret)
                return ret;

        /* init the instance list lock and application list lock */
        pthread_spin_init(&comp.spin_inst, 0);
        pthread_spin_init(&comp.spin_app, 0);

        /* register the instance in the system */
        instance_register(inspur_dir_instances);
        //ret = instance_register(inspur_dir_instances);    
        //if (ret) {
        //  pr_err("instance register Failed\n");
        //  return -EPERM;
        //} 

        pr_debug("instance register Successed\n");

        /* register the application in the system */
        application_register(&dir_application);
        //ret = application_register(&dir_application); 
        //if (ret) {
        //  pr_err("application register Failed\n");
        //  instance_unregister(inspur_dir_instances);
        //  return -EPERM;  
        //}

        pr_debug("application register Successed\n");

        /* after register the app, We own match the instance in the system */
        ret = match_probe(&dir_application);
        if (ret) {
                pr_err("match and probe error\n");
                application_unregister(&dir_application);
                instance_unregister(inspur_dir_instances);
                return -EPERM;
        }

        pr_debug("match and probe success\n");

        /* final retrieve all the resource */
        ret = match_remove(&dir_application);
        if (ret) {
                pr_err("match and remove error\n");
                return -EPERM;
        }

        //sleep(10);
        instance_unregister(inspur_dir_instances);

        pthread_spin_destroy(&comp.spin_inst);
        pthread_spin_destroy(&comp.spin_app);
        return 0;

}
