#ifndef _MYSQL_CPOOL_H_
#define _MYSQL_CPOOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <mysql.h>

#define LOG_INFO(fmt, args...) do { \
    time_t t; time(&t); \
    struct tm *p = localtime(&t); \
    fprintf(stdout, "[%d-%02d-%02d %02d:%02d:%02d] INFO (%s: %d) - ", \
            1900 + p->tm_year, p->tm_mon + 1, p->tm_mday, p->tm_hour, \
            p->tm_min, p->tm_sec, __func__, __LINE__); \
    fprintf(stdout, fmt, ##args); \
    putc('\n', stdout); \
} while (0);

#define LOG_DEBUG(fmt, args...) do { \
    time_t t; time(&t); \
    struct tm *p = localtime(&t); \
    fprintf(stdout, "[%d-%02d-%02d %02d:%02d:%02d] DEBUG (%s: %d) - ", \
            1900 + p->tm_year, p->tm_mon + 1, p->tm_mday, p->tm_hour, \
            p->tm_min, p->tm_sec, __func__, __LINE__); \
    fprintf(stdout, fmt, ##args); \
    putc('\n', stdout); \
} while (0);

#define LOG_WARN(fmt, args...) do { \
    time_t t; time(&t); \
    struct tm *p = localtime(&t); \
    fprintf(stdout, "[%d-%02d-%02d %02d:%02d:%02d] WARN (%s: %d) - ", \
            1900 + p->tm_year, p->tm_mon + 1, p->tm_mday, p->tm_hour, \
            p->tm_min, p->tm_sec, __func__, __LINE__); \
    fprintf(stderr, fmt, ##args); \
    putc('\n', stderr); \
} while (0);

#define LOG_ERROR(fmt, args...) do { \
    time_t t; struct tm *p = localtime(&t); \
    fprintf(stdout, "[%d-%02d-%02d %02d:%02d:%02d] ERROR (%s: %d) - ", \
            1900 + p->tm_year, p->tm_mon + 1, p->tm_mday, p->tm_hour, \
            p->tm_min, p->tm_sec, __func__, __LINE__); \
    fprintf(stderr, fmt, ##args); \
    putc('\n', stderr); \
} while (0);

typedef struct mysql_conn
{
    int    cid;
    MYSQL *db;
    struct mysql_conn *next;
} mysql_conn_t;

typedef struct mysql_cpool
{
    /* pool configuration */
    int max_conn_num;

    /* pool state */
    int cur_avail_conn;
    mysql_conn_t *connections;
    mysql_conn_t *queue_head;
    mysql_conn_t *queue_tail;
    int queue_closed;
    int shutdown;
    
    /* pool synchronization */
    pthread_mutex_t queue_lock;
    pthread_cond_t  queue_not_empty;
    pthread_cond_t  queue_full;
} *mysql_cpool_t;

int mysql_cpool_init(mysql_cpool_t *poolp, int max_conn_num, 
        const char *host, const char *user, const char *password, 
        const char *db_name, unsigned int port, const char *socket,
        unsigned long flag);

mysql_conn_t * mysql_cpool_alloc(mysql_cpool_t pool);

int mysql_cpool_free(mysql_cpool_t pool, mysql_conn_t* conn);

int mysql_cpool_destroy(mysql_cpool_t pool, int finish);

#endif  /* mysql_cpool.h */
