#ifndef _LOG_H
#define _LOG_H

#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <string>
#include "block_queue.h"
#include "../lock/locker.h"

class Log {
public:
    //局部变量懒汉
    static Log *getInstance() {
        static Log instance;
        return &instance;
    }

    //回调函数
    static void *flush_log_thread(void *args) {
        Log::getInstance()->asyn_write_log();

        return nullptr;
    }

    //绝对路径， 最大行数， 是否开启日志， 缓冲区， 队列长度
    bool init(const char *file_name, int close_log, int max_line = 5000000, int log_buffer_size = 8192, int max_queue_size = 0);

    void flush();

    void write_log(int level, const char *format, ...);

private:
    Log();
    ~Log(); //使用单例模式

    void *asyn_write_log() {
        string log;

        while (logQueue->pop(log)) {
            locker.lock();
            fputs(log.c_str(), fp);
            locker.unlock();
        }

        return nullptr;
    }

private:
    char dirName[128];  //目录名
    char logName[128]; //文件名
    int maxLine;    //单个日志最大行数
    int is_close_log;   //是否开启日志
    int is_asyn_log;    //是否是同步日志
    int logBufferSize;  //日志缓冲区大小
    int logCount;   //当前日志行数
    FILE *fp;   //打开的文件指针
    char *buf;  //缓冲区
    Locker locker;
    int m_today; //今天是哪天，因为要按天数区分日志
    BlockQueue<string> *logQueue; //阻塞队列
};

#define LOG_DEBUG(format, ...) if (is_close_log == 0) {Log::getInstance()->write_log(0, format, ##__VA_ARGS__);Log::getInstance()->flush();}
#define LOG_INFO(format, ...) if (is_close_log == 0) {Log::getInstance()->write_log(1, format, ##__VA_ARGS__);Log::getInstance()->flush();}
#define LOG_WARN(format, ...) if (is_close_log == 0) {Log::getInstance()->write_log(2, format, ##__VA_ARGS__);Log::getInstance()->flush();}
#define LOG_ERROR(format, ...) if (is_close_log == 0) {Log::getInstance()->write_log(3, format, ##__VA_ARGS__);Log::getInstance()->flush();}

#endif
