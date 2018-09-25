/**
 * @defgroup 配置文件解析模块
 * @ingroup  通用模块
 *
 * config.h: 配置文件解析
 *
 * @{
 */

#ifndef __CONFIG_H_
#define __CONFIG_H_
#include <stdio.h>

#define CONFIG_LINE_BUFFER_SIZE 256

/* config_t 结构体描述一个配置文件*/
struct config {
    /*指向配置文件的文件指针 */
    FILE *fd;
};
typedef struct config config_t;

/**
    @brief 关闭配置文件
    
    根据传入的配置文件结构体，关闭相应的配置文件，并释放相关空间。

    @param[in] configfile   配置文件结构体

    @return
        关闭成功返回0，失败返回1.
*/
extern int config_close(config_t * configfile);

/**
    @brief 配置项查询

    在用户指定的一个配置文件中查询某一个配置项的信息。

    @param[in] configfile   配置文件结构体
    @param[in] item         配置项（"section/key"  or "key"）
    @param[in] buffer       用户传入的用于存放查询结果的缓冲区
    @param[in] size         缓冲区大小

    @return
        查询成功返回0，失败返回1.
*/
extern int config_query(config_t * configfile, char *item, void *buffer, size_t size);

/**
    @brief 配置项插入

    在用户指定的一个配置文件中插入某一个配置项的信息。

    @param[in] configfile   配置文件结构体
    @param[in] item         配置项（"section/key" or "key"形式。建议使用“section/key”形式，
                            这将会把键-值对插入到相应的section中；若使用“key”形式，则把键-值对
                            插入到配置文件最前端，没有section概念）
    param[in] val           配置项的值

    @return
        查询成功返回0，失败返回1.
*/
extern int config_insert(config_t * configfile, char *item, char *val);

/**
    @brief 打开配置文件 
    
    打开一个配置文件用于读取配置信息。

    @param[in] name         配置文件路径

    @return
        返回打开的config_t对象指针，返回NULL表示失败。
*/
extern config_t *config_open(char *name);

#endif

/** @} */
