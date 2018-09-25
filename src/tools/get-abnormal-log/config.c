/**
 
    config.c: 配置文件解析
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "cfg.h"

typedef struct {
    char *name;
    size_t size;
} section_t;

typedef struct {
    char *name;
    size_t size;
} keyy_t;

/**
    @brief 关闭配置文件
    
    根据传入的配置文件结构体，关闭相应的配置文件，并释放相关空间。

    @return
        关闭成功返回0，失败返回1.
*/
int config_close(config_t * configfile)
{
    //if (configfile == NULL || configfile->fd == NULL)
    //  goto error;

    fclose(configfile->fd);

    free(configfile);

    return 0;

//error:
//  return 1;
}

/**
    @brief 裁剪字符串

    裁剪字符串中左右两侧，空格、TAB字符。

    @return
        成功裁剪返回裁剪后的字符串指针，失败返回NULL。
*/
static char *trim(char *str)
{
    char *head = str;
    char *tail = str + strlen(str) - 1;
    int head_stop = 0, tail_stop = 0;

    if (str == NULL) {
        printf("the string is NULL\n");
        goto error;
    }

    if (!strcmp(str, ""))
        goto ret;

    while (head < tail) {
        if (head_stop && tail_stop)
            break;

        if (*head == ' ' || *head == '\t')
            head++;
        else
            head_stop = 1;

        if (*tail == ' ' || *tail == '\t')
            tail--;
        else
            tail_stop = 1;
    }

    if ((*head == ' ' || *head == '\t') && head == tail)
        *tail = '\0';
    else
        *(tail + 1) = '\0';

  ret:
    return head;
  error:
    return NULL;
}

/**
    @brief 查询配置实现体

    读取配置文件，并获取配置项

    @return
        读取成功返回0，失败返回1.
*/
static int config_do_query(FILE * fd, section_t * orig_section, keyy_t * orig_key,
                           void *buffer, size_t size)
{
    char buf[CONFIG_LINE_BUFFER_SIZE] = { 0 };
    char *key, *val, *section = NULL;
    char section_tmp[CONFIG_LINE_BUFFER_SIZE] = { 0 };

    if (size > CONFIG_LINE_BUFFER_SIZE) {
        goto error;
    }

    if (fd == NULL || orig_section == NULL || orig_key == NULL || buffer == NULL || size <= 0) {
        printf("pass a invalid argument\n");
        goto error;
    }

    memset(buffer, 0, size);

    if (fseek(fd, 0, SEEK_SET) == -1) {
        printf("fail to fseek\n");
        //printf("fail to fseek:%s\n", strerror(errno));
        goto error;
    }

    while (fgets(buf, sizeof(buf), fd))
        //while (fread(buf, sizeof(buf), 1,  fd))
    {
        //  if (strlen(buf) >= sizeof(buf))
        //  {
        //      printf("overflow\n");
        //      goto error;
        //  }

        if (buf[strlen(buf) - 1] == '\n' || strlen(buf) + 1 != sizeof(buf)) {
            if ((val = strstr(buf, "="))) {
                if ((orig_section->name != NULL && section != NULL &&
                     !strncmp(orig_section->name, section, orig_section->size)) ||
                    orig_section->name == NULL) {
                    *val = '\0';
                    key = buf;
                    val++;

                    if (strlen(val) + 1 + strlen(key) + 1 > CONFIG_LINE_BUFFER_SIZE) {
                        printf("buffer overflow\n");
                        goto error;
                    }
                    //if (strstr(val, "\r"))
                    //  val[strlen(val) - 2] = '\0';
                    //else if (strstr(val, "\n"))
                    //  val[strlen(val) - 1] = '\0';
                    if (val[strlen(val) - 1] == '\n')
                        val[strlen(val) - 1] = '\0';
                    else
                        goto error;

                    if (!strncmp(orig_key->name, trim(key), orig_key->size)) {
                            /**
                                若配置项未进行配置，即等号右边为空，则buffer中返回空，并提示。
                                另一种buffer为空返回的情况是，程序找不到对应配置项。给出提示信息。
                            */
                        val = trim(val);
                        if (!strcmp(val, ""))
                            printf("the value is null\n");

                        /*用户传入的buffer大小不能太小，否则无法拷贝 */
                        if (size < strlen(val) + 1) {
                            printf("the size of buffer is too small.%d bytes is recommended\n",
                                   CONFIG_LINE_BUFFER_SIZE);
                            goto error;
                        }

                        if (strlen(val) >= size)
                            goto error;

                        //strncpy(buffer, val, strlen(val) + 1);
                        strcpy(buffer, val);
                        goto ret;
                    }
                }

            } else if (strstr(buf, "[") && strstr(buf, "]")) {
                if (!orig_section->name)
                    continue;

                if (strlen(buf) >= sizeof(buf)) {
                    printf("line content is too long\n");
                    goto error;
                }
                //if (strstr(buf, "\r"))
                //  buf[strlen(buf) - 3 ] = '\0';
                //else if (strstr(buf, "\n"))
                //  buf[strlen(buf) - 2 ] = '\0';
                //else
                //  buf[strlen(buf) - 1] = '\0';
                if (buf[strlen(buf) - 1] == '\n' && buf[strlen(buf) - 2] == ']' && 
                strlen(buf) < sizeof(buf))
                    buf[strlen(buf) - 2] = '\0';
                else if ('[' == buf[strlen(buf) - 1] && strlen(buf) < sizeof(buf))
                    buf[strlen(buf) - 1] = '\0';
                else
                    goto error;

                if (sizeof(section_tmp) > strlen(trim(buf + 1)))
                    section = strcpy(section_tmp, trim(buf + 1));
                else
                    goto error;
            }
        } else
            /*如果配置项长度大于buf的大小，程序无法处理，略过此配置项的解析 */
            printf("the configuration item is too long,and skip:%s\n", buf);
    }

    /*找不到指定的配置项，返回。buffer为空 */
    printf("No corresponding configuration items\n");
    return -1;

  ret:
    return 0;

  error:
    return 1;
}

/**
    @brief 配置项查询

    在用户指定的一个配置文件中查询某一个配置项的信息。

    @return
        查询成功返回0，失败返回1.
*/
int config_query(config_t * configfile, char *item, void *buffer, size_t size)
{
    section_t *section;
    keyy_t *key;
    char *position;

    if (configfile == NULL || item == NULL || buffer == NULL) {
        printf("pass a null pointer\n");
        goto error;
    }

    section = (section_t *) malloc(sizeof(section_t));
    if (NULL == section) {
        perror("malloc");
        goto error;
    }
    key = (keyy_t *) malloc(sizeof(keyy_t));
    if (NULL == key) {
        printf("fail to malloc keyy_t struct\n");
        free(section);
        goto error;
    }

    memset(section, 0, sizeof(section));
    memset(key, 0, sizeof(key));

    if ((position = strstr(item, "/"))) {
        section->name = item;
        section->size = (size_t) (position - item);
        key->name = ++position;
        key->size = (size_t) (strlen(item) - section->size - 1);
    } else {
        key->name = item;
        key->size = strlen(item);
    }

    if (config_do_query(configfile->fd, section, key, buffer, size)) {
        printf("fail to analyze configfile\n");
        goto clean;
    }

    free(section);
    free(key);

    return 0;

  clean:
    free(section);
    free(key);

  error:
    return 1;
}

int config_insert(config_t * configfile, char *item, char *val)
{
    return 0;
}

/**
    @brief 打开配置文件 
    
    打开一个配置文件用于读取配置信息。

    @return
        返回打开的config_t对象指针，返回NULL表示失败。
*/
config_t *config_open(char *name)
{
    config_t *configfile;

    if (name == NULL) {
        printf("file name is NULL\n");
        goto error;
    }

    configfile = (config_t *) malloc(sizeof(config_t));
    if (configfile == NULL) {
        //printf("fail to malloc config_t struct：%s\n", strerror(errno));
        printf("fail to malloc config_t struct\n");
        goto error;
    }

    configfile->fd = fopen(name, "r");
    if (configfile->fd == NULL) {
        //printf("fail to open config file (%s): %s\n", name, strerror(errno));
        printf("fail to open config file (%s)\n", name);
        goto clean;
    }

    return configfile;

  clean:
    free(configfile);
  error:
    return NULL;
}
