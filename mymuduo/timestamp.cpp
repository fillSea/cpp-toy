#include "timestamp.h"

#include <bits/types/struct_timeval.h>
#include <sys/time.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

Timestamp::Timestamp() : micro_seconds_since_epoch_(0) {}

Timestamp::Timestamp(int64_t micro_seconds_since_epoch)
	: micro_seconds_since_epoch_(micro_seconds_since_epoch) {}

// 获取当前时间
Timestamp Timestamp::Now() {
	return Timestamp(time(NULL));
}

// 将时间转换为 string
std::string Timestamp::ToString() const {
	char buf[128]{0};
    // 将时间戳转换为本地时间结构体
	tm *tm_time = localtime(&micro_seconds_since_epoch_);
    // 格式化时间为字符串
	snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d", tm_time->tm_year + 1900,
			 tm_time->tm_mon + 1, tm_time->tm_mday, tm_time->tm_hour, tm_time->tm_min,
			 tm_time->tm_sec);

	return buf;
}

// #include <iostream>

// int main(){
//     std::cout << Timestamp::Now().ToString() << std::endl;

//     return 0;
// }