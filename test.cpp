#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "mysql_cpool.h"

mysql_cpool_t pool;

void * foo(void *arg) 
{
    int tid = (int)arg;

    char file_name[128];
    char sql[128];
    sprintf(file_name, "./output/%d.txt", tid);
    FILE *fp = fopen(file_name, "w");

    for (int i = 0; i < 10; i++) {
        printf("thread: %d loop: %d\n", tid, i + 1);
        int id = rand() % 2000 + 1;
        //fprintf(fp, "query id: %d\n", id);
        
        sprintf(sql, "SELECT id FROM test_table WHERE id = %d LIMIT 1", id);

        mysql_conn_t *conn = mysql_cpool_alloc(pool);
        if (conn == NULL) {
            printf("thread %d: get connection failed!\n", tid);
            continue;
        }

        MYSQL *mysql = conn->db;

        if (mysql_query(mysql, sql) != 0) { 
            LOG_ERROR("mysql_query: %s", mysql_error(mysql));

            mysql_ping(mysql);

            if (mysql_query(mysql, sql) != 0) { 
                LOG_ERROR("mysql_query: %s", mysql_error(mysql));
                return NULL;
            }
        }

        MYSQL_RES *res = mysql_store_result(mysql);
        if (res == NULL) { 
            if (mysql_errno(mysql) != 0) { 
                LOG_WARN("mysql_store_result: %s", mysql_error(mysql));
            }       
            continue;  
        } 

        MYSQL_ROW row = mysql_fetch_row(res);
        if (row != NULL) {
            //fprintf(fp, "obtain id: %s\n\n", row[0]);
        } else {
            //fprintf(fp, "obtain id: NULL\n\n");
            printf("obtain id: NULL\n\n");
        }

        mysql_free_result(res);

        mysql_cpool_free(pool, conn);
    }
    
    fclose(fp);
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    mysql_cpool_init(&pool, 10, "localhost", "root", "", "test", 3306, 
            "/var/run/mysqld/mysqld.sock", 0);

    int thread_num = 8;
    pthread_t tid[thread_num];
    for (int i = 0; i < thread_num; i++) {
        pthread_create(tid + i, NULL, foo, (void *)(i + 1));
    }

    for (int i = 0 ; i < thread_num; i++) {
        pthread_join(tid[i], NULL);
    }

    mysql_cpool_destroy(pool, 1);

    return 0;
}
