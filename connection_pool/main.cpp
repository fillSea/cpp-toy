#include <iostream>

#include "connection.h"

#define LOG(str)                                                                     \
	std::cout << __FILE__ << ":" << __LINE__ << " " << __TIMESTAMP__ << " : " << str \
			  << std::endl;

int main() {
    std::cout << "begin" << std::endl;
	Connection conn;
	char sql[1024] = {0};
	sprintf(sql, "insert into user(name, age, sex) values('%s', '%d', '%s')", "zhangsan",
			20, "男");
    bool res = conn.Connect("127.0.0.1", 3306, "root", "123456", "test");

    if (!res) {
        std::cout << "连接失败" << std::endl;
        return -1;
    }

    if (!conn.Update(sql)) {
        std::cout << "更新失败" << std::endl;
        LOG("更新失败:" + std::string(sql));
    }

	return 0;
}