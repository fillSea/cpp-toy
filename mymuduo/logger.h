#pragma once

#include <string>

#include "noncopyable.h"

// 定义日志级别
enum LogLevel {
	INFO,	// 普通信息
	ERROR,	// 错误信息
	FATAL,	// 崩溃信息
	DEBUG,	// 调试信息
};

// 日志单例类
class Logger : Noncopyable {
public:
	// 获取日志实例
	static Logger& GetInstance();
	// 设置日志级别
	void SetLogLevel(int level);
	// 写日志
	void Log(std::string msg);

private:
	Logger();

private:
	int log_level_;	 // 日志级别
};


// 定义宏方便用户使用
/*
#define LOG_INFO(log_msg_format, ...) \
    do{ \
        Logger &logger = Logger::GetInstance(); \
        logger.SetLogLevel(INFO); \
        char buf[1024]{0}; \
        snprintf(buf, 1024, log_msg_format, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_ERROR(log_msg_format, ...) \
    do{ \
        Logger &logger = Logger::GetInstance(); \
        logger.SetLogLevel(ERROR); \
        char buf[1024]{0}; \
        snprintf(buf, 1024, log_msg_format, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_FATAL(log_msg_format, ...) \
    do{ \
        Logger &logger = Logger::GetInstance(); \
        logger.SetLogLevel(FATAL); \
        char buf[1024]{0}; \
        snprintf(buf, 1024, log_msg_format, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_DEBUG(log_msg_format, ...) \
    do{ \
        Logger &logger = Logger::GetInstance(); \
        logger.SetLogLevel(DEBUG); \
        char buf[1024]{0}; \
        snprintf(buf, 1024, log_msg_format, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)
*/

// 使用格式: LOG_INFO("%s %d", arg1, arg2)
// 统一处理方法
#define LOG(log_level, log_msg_format, ...) \
    do{ \
        Logger &logger = Logger::GetInstance(); \
        logger.SetLogLevel(log_level); \
        char buf[1024]{0}; \
        snprintf(buf, 1024, log_msg_format, ##__VA_ARGS__); \
        logger.Log(buf); \
    } while(0)

#define LOG_INFO(log_msg_format, ...) LOG(INFO, log_msg_format, ##__VA_ARGS__)
#define LOG_ERROR(log_msg_format, ...) LOG(ERROR, log_msg_format, ##__VA_ARGS__)
#define LOG_FATAL(log_msg_format, ...) \
    LOG(FATAL, log_msg_format, ##__VA_ARGS__); \
    exit(0);

#define LOG_DEBUG(log_msg_format, ...) LOG(DEBUG, log_msg_format, ##__VA_ARGS__)