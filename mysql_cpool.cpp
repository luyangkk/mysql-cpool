#include "mysql_cpool.h"

int mysql_cpool_init(mysql_cpool_t *poolp, int max_conn_num, 
        const char *host, const char *user, const char *password, 
        const char *db_name, unsigned int port, const char *socket,
        unsigned long flag)
{
    int i, ret;
    mysql_cpool_t pool;
    MYSQL *db;

    /* allocate a pool data structure */
    if ((pool = (mysql_cpool_t)malloc(sizeof(struct mysql_cpool))) == NULL) {
        LOG_ERROR("allocate a pool data structure failed!");
        return -1;
    }

    /* initialize the fields */
    pool->max_conn_num = max_conn_num;
    pool->cur_avail_conn = max_conn_num;
    pool->queue_closed = 0;
    pool->shutdown = 0;
    
    /* allocate connection data structrue */
    if ((pool->connections = (mysql_conn_t *)malloc(sizeof(mysql_conn_t) * 
            max_conn_num)) == NULL) {
        LOG_ERROR("allocate connections failed!");
        return -1;
    }
    
    /* initialize mysql library */
    if (mysql_library_init(0, NULL, NULL) != 0) {
        LOG_ERROR("mysql_library_init failed!");
        return -1;
    }

    /* initialize connections */
    for (i = 0; i < max_conn_num; i++) {
        if (!(db = mysql_init(NULL))) {
            LOG_ERROR("mysql_init failed: %s", mysql_error(db));
            if (db) mysql_close(db);
            return -1;
        }

        if (!mysql_real_connect(db, host, user, password, db_name, 
                    port, socket, flag)) {
            LOG_ERROR("[ERROR] mysql_real_connect failed: %s", 
                    mysql_error(db));
            if (db) mysql_close(db);
            return -1;
        }

        (pool->connections[i]).cid = i;
        (pool->connections[i]).db = db;
        (pool->connections[i]).next = pool->connections + i + 1;
    }
    (pool->connections[i - 1]).next = NULL;

    pool->queue_head = pool->connections;
    pool->queue_tail = pool->connections + i - 1;

    /* initialize  synchronization data structure */
    if ((ret = pthread_mutex_init(&(pool->queue_lock), NULL)) != 0) { 
        LOG_ERROR("init queue lock failed (%d)!", ret);
        return -1;
    }

    if ((ret = pthread_cond_init(&(pool->queue_not_empty), NULL)) != 0) {
        LOG_ERROR("pthread_cond_init failed (%d)!", ret);
        return -1;
    }

    if ((ret = pthread_cond_init(&(pool->queue_full), NULL)) != 0) {
        LOG_ERROR("pthread_cond_init failed (%d)!", ret);
        return -1;
    }

    *poolp = pool;

    LOG_INFO("mysql_cpool_init success!");

    return 0;
}

mysql_conn_t * mysql_cpool_alloc(mysql_cpool_t pool)
{
    int ret;
    mysql_conn_t *conn;

    if ((ret = pthread_mutex_lock(&(pool->queue_lock))) != 0) {
        LOG_ERROR("pthread_mutex_lock failed(%d)!", ret);
        return NULL;
    }

    while ((pool->cur_avail_conn == 0) && 
            !(pool->queue_closed || pool->shutdown)) {
        if ((ret = pthread_cond_wait(&(pool->queue_not_empty), 
                &(pool->queue_lock))) != 0) {
            LOG_ERROR("pthread_cond_wait failed (%d)!", ret);
            return NULL;
        }
    }

    /* the pool is in the process of being destroyed */
    if (pool->shutdown || pool->queue_closed) {
        LOG_WARN("connection pool is shutdown!");
        if ((ret = pthread_mutex_unlock(&(pool->queue_lock))) != 0) {
            LOG_ERROR("pthread_mutex_unlock failed (%d)!", ret);
        }
        return NULL;
    }

    /* allocate a connection from queue */
    conn = pool->queue_head;
    pool->cur_avail_conn--;
    if (pool->cur_avail_conn == 0) {
        pool->queue_head = pool->queue_tail = NULL;
    } else {
        pool->queue_head = conn->next;
    }
    conn->next = NULL;
    
    if ((ret = pthread_mutex_unlock(&(pool->queue_lock))) != 0) {
        LOG_ERROR("pthread_mutex_unlock failed!");
        return NULL;
    }

    return conn;
}

int mysql_cpool_free(mysql_cpool_t pool, mysql_conn_t* conn)
{
    int ret;

    if ((ret = pthread_mutex_lock(&(pool->queue_lock))) != 0) {
        LOG_ERROR("pthread_mutex_lock failed(%d)!", ret);
        return -1;
    }

    if (pool->cur_avail_conn == 0) {
        pool->queue_head = pool->queue_tail = conn;
    } else {
        pool->queue_tail->next = conn;
        pool->queue_tail = conn;
    }
    pool->cur_avail_conn += 1;

    if ((ret = pthread_mutex_unlock(&(pool->queue_lock))) != 0) {
        LOG_ERROR("pthread_mutex_unlock failed (%d)!", ret);
        return -1;
    }

    /* handle waiting connection threads */
    if (pool->cur_avail_conn != 0) {
        if ((ret = pthread_cond_broadcast(&(pool->queue_not_empty))) != 0) {
            LOG_ERROR("pthread_cond_broadcast failed (%d)!", ret);
            return -1;
        }
    }

    /* handle waiting destroyer thread */
    if (pool->cur_avail_conn == pool->max_conn_num) {
        if ((ret = pthread_cond_broadcast(&(pool->queue_full))) != 0) {
            LOG_ERROR("pthread_cond_broadcast failed (%d)!", ret);
            return -1;
        }
    }

    return 0;
}

int mysql_cpool_destroy(mysql_cpool_t pool, int finish)
{
    int i, ret;

    if ((ret = pthread_mutex_lock(&(pool->queue_lock))) != 0) {
        LOG_ERROR("pthread_mutex_lock failed(%d)!", ret);
        return -1;
    }

    /* Is a shutdown already in progress? */
    if (pool->queue_closed || pool->shutdown) {
        if ((ret = pthread_mutex_unlock(&(pool->queue_lock))) != 0) {
            LOG_ERROR("pthread_mutex_unlock failed (%d)!", ret);
            return -1;
        }
    }

    pool->queue_closed = 1;

    /* if finish flag is set, wait for all connections are free */
    if (finish == 1) {
        while (pool->cur_avail_conn != pool->max_conn_num) {
            if (pthread_cond_wait(&(pool->queue_full), 
                        &(pool->queue_lock))) {
                LOG_ERROR("pthread_cond_wait failed (%d)!", ret);
                return -1;
            }
        }
    }

    pool->shutdown = 1;

    if ((ret = pthread_mutex_unlock(&(pool->queue_lock))) != 0) {
        LOG_ERROR("pthread_mutex_unlock failed (%d)!", ret);
        return -1;
    }

    /* wake up any threads so they recheck shutdown flag */
    if ((ret = pthread_cond_broadcast(&(pool->queue_not_empty))) != 0) {
        LOG_ERROR("pthread_cond_broadcast failed (%d)!", ret);
        return -1;
    }

    /* close all connections */
    for (i = 0; i < pool->max_conn_num; i++) {
        mysql_close((pool->connections[i]).db);
    }
    LOG_INFO("close all mysql connetions!");

    /* free pool */
    pthread_mutex_destroy(&(pool->queue_lock));
    pthread_cond_destroy(&(pool->queue_not_empty));
    pthread_cond_destroy(&(pool->queue_full));

    free(pool->connections);
    free(pool);

    return 0;
}
