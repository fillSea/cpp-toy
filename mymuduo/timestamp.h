#pragma once


#include <cstdint>
#include <string>

// 时间操作
class Timestamp {
public:
    Timestamp();
    explicit Timestamp(int64_t micro_seconds_since_epoch);
    // 获取当前时间
    static Timestamp Now();
    // 将时间转换为 string
    std::string ToString() const;
private:
	int64_t micro_seconds_since_epoch_;
};