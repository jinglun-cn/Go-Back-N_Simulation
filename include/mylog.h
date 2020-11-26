#pragma once

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#ifdef __cplusplus  
extern "C" { 
#endif 

#define STDERR_LEVEL_LOG

#if defined(STDERR_LEVEL_LOG)

#define LOG(foramt, ...) do{                            \
            fprintf(stderr, "[%s] [%04d] [%s] \n----> ", \
                    __FILE__, __LINE__, __FUNCTION__); \
            fprintf(stderr, foramt, ##__VA_ARGS__);                     \
            fprintf(stderr, "\n");                            \
        }while(0);

#define LOG_X do {  \
            fprintf(stderr, "[%s] [%04d] [%s] \n\n", \
                    __FILE__, __LINE__, __FUNCTION__); \
        } while (0);

#elif defined(FILE_LEVEL_LOG)

#define LOG_FILENAME "/tmp/mylog.logfile"

static int log_switch;

static FILE *log_fd;


#define LOG_INIT_PATH(path) do{                                 \
            log_switch = 1;                         \
            log_fd = NULL;                          \
            log_fd = fopen(path, "w+");             \
            if(log_fd == NULL){                     \
                log_switch = 0;                     \
            }else{                                  \
                fprintf(log_fd, "---------->> Log Starts! <<----------\n\n"); \
            }                                       \
        }while(0)

#define LOG_INIT do{                                \
            log_switch = 1;                         \
            log_fd = NULL;                          \
            log_fd = fopen(LOG_FILENAME, "w+");     \
            if(log_fd == NULL){                     \
                log_switch = 0;                     \
            }else{                                  \
                fprintf(log_fd, "---------->> Log Starts! <<----------\n\n"); \
            }                                        \
        }while(0)

#define LOG_DEST do{                            \
            if(log_switch){                     \
                fclose(log_fd);                 \
            }                                   \
        }while(0)

#define LOG(foramt, ...) do{                            \
            if(log_switch){                              \
                fprintf(log_fd, "[%s] [%04d] [%s] \n----> ", \
                     __FILE__, __LINE__, __FUNCTION__); \
                fprintf(log_fd, foramt, ##__VA_ARGS__);                     \
                fprintf(log_fd, "\n");                            \
                fflush(log_fd);                                   \
            }                                                     \
        }while(0);

#define LOG_X do {  \
            if(log_switch){ \
                fprintf(log_fd, "[%s] [%04d] [%s] \n\n", \
                     __FILE__, __LINE__, __FUNCTION__); \
                fflush(log_fd); \
            }                                   \
        } while (0);

#else

#define LOG_INIT_PATH(path) do {} while(0);
#define LOG_INIT do {} while(0);
#define LOG_DEST do {} while(0);
#define LOG(foramt, ...) do {} while(0);
#define LOG_X do {} while(0);

#endif

#ifdef __cplusplus 
} 
#endif 