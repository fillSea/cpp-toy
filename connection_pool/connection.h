#include <mysql.h>

#include <ctime>
#include <string>


// 数据库操作类
class Connection {
public:
	// 初始化数据库连接
	Connection();
	// 释放数据库连接资源
	~Connection();
	// 连接数据库
	bool Connect(std::string ip, unsigned short port, std::string user,
				 std::string password, std::string dbname);
	// 更新操作 insert、delete、update
	bool Update(std::string sql);
	// 查询操作 select
	MYSQL_RES *Query(std::string sql);
	// 刷新一下连接的起始的空闲时间点
	void RefreshAliveTime(){
		alive_time_ = clock();
	}
	// 返回存活时间
	clock_t GetAliveTime() const {
		return clock() - alive_time_;
	}
private:
	MYSQL *conn_;  // 表示和MySQL Server的一条连接
	clock_t alive_time_; // 记录进入空闲状态后的存活时间
};