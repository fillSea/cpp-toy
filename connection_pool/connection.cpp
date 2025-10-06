#include "connection.h"
#include <iostream>

#define LOG(str)                                                                     \
	std::cout << __FILE__ << ":" << __LINE__ << " " << __TIMESTAMP__ << " : " << str \
			  << std::endl;

// 初始化数据库连接
Connection::Connection() { conn_ = mysql_init(nullptr); }
// 释放数据库连接资源
Connection::~Connection() {
	if (conn_ != nullptr) mysql_close(conn_);
}
// 连接数据库
bool Connection::Connect(std::string ip, unsigned short port, std::string user,
						 std::string password, std::string dbname) {
	MYSQL* p = mysql_real_connect(conn_, ip.c_str(), user.c_str(), password.c_str(),
								  dbname.c_str(), port, nullptr, 0);
	return p != nullptr;
}
// 更新操作 insert、delete、update
bool Connection::Update(std::string sql) {
	if (mysql_query(conn_, sql.c_str())) {
		LOG("更新失败:" + sql);
		return false;
	}
	return true;
}
// 查询操作 select
MYSQL_RES* Connection::Query(std::string sql) {
	if (mysql_query(conn_, sql.c_str())) {
		LOG("查询失败:" + sql);
		return nullptr;
	}
	return mysql_use_result(conn_);
}