#include <string>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <stdarg.h>
#include "log.h"

Log::Log() {
    logCount = 0;
    is_asyn_log = false; //默认异步
}


Log::~Log() {
    if (fp != nullptr) {
        fclose(fp);
    }

    delete logQueue;
    delete[] buf;
}

bool Log::init(const char *file_name, int close_log, int max_line, int log_buffer_size, int max_queue_size) {
    if (max_queue_size >= 1) {
        is_asyn_log = true;
        logQueue = new BlockQueue<string>(max_queue_size);

        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL);//异步线程处理日志
    }

    maxLine = max_line;
    is_close_log = close_log;
    logBufferSize = log_buffer_size;
    buf = new char[log_buffer_size];
    memset(buf, '\0', log_buffer_size);

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    m_today = my_tm.tm_mday;

    const char *p = strrchr(file_name, '/');
    char loadBuf[256] = {'\0'};

    if (p == nullptr) {
        snprintf(loadBuf, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    } else {
        strcpy(logName, p + 1);
        strncpy(dirName, file_name, p - file_name + 1);
        snprintf(loadBuf, 255, "%s%d_%02d_%02d_%s", dirName, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, logName);
    }
    //printf("loadBuf:%s\n", loadBuf);

    fp = fopen(loadBuf, "a");
    if (fp == nullptr)
        return false;

    return true;
}

void Log::write_log(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL); 
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    char tc[16] = {'\0'};
    switch (level) {
        case 0:
            strcpy(tc, "[debug]");
            break;
        case 1:
            strcpy(tc, "[info]");
            break;
        case 2:
            strcpy(tc, "[warn]");
            break;
        case 3:
            strcpy(tc, "[error]");
            break;
        default:
            strcpy(tc, "[info]");
            break;
    }

    locker.lock();

    ++logCount;
    if (m_today != my_tm.tm_mday || logCount % maxLine == 0) {
        fflush(fp);
        fclose(fp);

        char newLog[255] = {'\0'};
        char timeBuf[20] = {'\0'}; 
        sprintf(timeBuf, "%d_%02d_%02d", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday) {
           snprintf(newLog, 255, "%s%s_%s", dirName, timeBuf, logName);
           m_today = my_tm.tm_mday;
           logCount = 0;
        } else {
            snprintf(newLog, 255, "%s%s_%s.%lld", dirName, timeBuf, logName, logCount / maxLine);
            logCount = 0;
        }

        fp = fopen(newLog, "a");
    }

    locker.unlock();

    string logStr;
    va_list valst;
    va_start(valst, format);

    locker.lock();

    int n = snprintf(buf, logBufferSize, "%d-%02d-%02d %02d:%02d:%02d.%06lld %s ", my_tm.tm_year + 1900, my_tm.tm_mon + 1,
                    my_tm.tm_mday, my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, tc);
    int m = vsnprintf(buf + n, logBufferSize - n - 1, format, valst);
    buf[n + m] = '\n';
    buf[n + m + 1] = '\0';
    logStr = buf;

    locker.unlock();

    if (is_asyn_log && !logQueue->full()) {
        logQueue->push(logStr);
    } else {
        locker.lock();
        fputs(logStr.c_str(), fp);
        locker.unlock();
    }

    va_end(valst);
}

void Log::flush() {
    locker.lock();
    fflush(fp);
    locker.unlock();
}