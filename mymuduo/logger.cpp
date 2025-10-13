#include "logger.h"

#include <iostream>
#include <string>
#include "timestamp.h"

// 获取日志实例
Logger& Logger::GetInstance() {
	static Logger logger;
	return logger;
}

Logger::Logger(): log_level_(INFO){}

// 设置日志级别
void Logger::SetLogLevel(int level) { log_level_ = level; }

// 写日志
// 格式: [级别信息] time : msg
void Logger::Log(std::string msg) {
	switch (log_level_) {
		case INFO:
			std::cout << "[INFO]";
			break;
		case ERROR:
			std::cout << "[ERROR]";
			break;
		case FATAL:
			std::cout << "[FATAL]";
			break;
		case DEBUG:
			std::cout << "[DEBUG]";
			break;
		default:
			break;
	}

	// 打印时间和 msg
	std::cout << Timestamp::Now().ToString() << " : " << msg << std::endl;
}