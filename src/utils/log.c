#include "log.h"
#include <stdio.h>

static LogLevel g_current_level = LOG_DEBUG;

void log_set_level(LogLevel level)
{
    g_current_level = level;
}

void log_write(LogLevel level, const char *file, int line, const char *fmt, ...)
{
//1.过滤 当传入的level低于默认level 就忽略
    if(level<g_current_level)
    {
        return;
    }

    const char *level_strings[] = {
        "DEBUG",
        "INFO ",
        "WARN ",
        "ERROR",
        "FATAL"
    };

    printf("[%s] [%s:%d] ",level_strings[level],file,line);

    va_list args;
    va_start(args, fmt);   // 初始化参数游标
    vprintf(fmt, args);     // 用 vprintf 把 fmt 和 args 展开打印
    va_end(args);

    printf("\n");

    fflush(stdout);


}