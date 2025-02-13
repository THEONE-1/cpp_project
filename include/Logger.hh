#pragma once
#include "noncopyable.hh"
#include <string>
#include <cstdio>  // Add this for snprintf

// ##__VA_ARGS__: 可变参数的宏,用于传入具体参数

// 基础日志宏,避免代码重复
#define LOG_BASE(level, LogmsgFormat, ...) \
    do \
    { \
        Logger& logger = Logger::instance(); \
        logger.setLogLevel(level); \
        char buf[1024] = {0}; \
        snprintf(buf, sizeof(buf), LogmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

// 各个日志级别的宏定义
#define LOG_INFO(LogmsgFormat, ...) \
    LOG_BASE(INFO, LogmsgFormat, ##__VA_ARGS__)

#define LOG_ERROR(LogmsgFormat, ...) \
    LOG_BASE(ERROR, LogmsgFormat, ##__VA_ARGS__)

#define LOG_FATAL(LogmsgFormat, ...) \
    LOG_BASE(FATAL, LogmsgFormat, ##__VA_ARGS__)

#define LOG_DEBUG(LogmsgFormat, ...) \
    LOG_BASE(DEBUG, LogmsgFormat, ##__VA_ARGS__)

//日志级别
enum LogLevel
{
    INFO,
    ERROR,
    FATAL,
    DEBUG
};

class Logger : public noncopyable
{
public:
//获取日志唯一实体
static Logger& instance();

//设置日志级别
void setLogLevel(int level);

//写日志
void log(std::string msg);

private:
    int logLevel_;
    Logger(){}
};