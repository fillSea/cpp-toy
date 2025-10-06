#include "connection_pool.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#define LOG(str)                                                                     \
	std::cout << __FILE__ << ":" << __LINE__ << " " << __TIMESTAMP__ << " : " << str \
			  << std::endl;

// 单例接口
ConnectionPool* ConnectionPool::GetInstance() {
	static ConnectionPool pool;
	return &pool;
}

// 从配置文件中加载配置
bool ConnectionPool::LoadConfig() {
    std::ifstream file("mysql.ini", std::ifstream::in);
    if (!file.is_open()) {
        LOG("mysql.ini file is not exist!");
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        
        size_t idx = line.find("=");
        if (idx == std::string::npos) {
            continue;
        }
        
        std::string key = line.substr(0, idx);
        std::string value = line.substr(idx + 1);
        
        // 去除值字符串两端的空白字符（如换行符、空格等）
        value.erase(0, value.find_first_not_of(" \t\n\r"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);
        
        if (key == "ip") {
            ip_ = value;
        } else if (key == "port") {
            port_ = std::stoi(value);
        } else if (key == "username") {
            username_ = value;
        } else if (key == "password") {
            password_ = value;
        } else if (key == "dbname") {
            dbname_ = value;
        } else if (key == "init_size") {
            init_size_ = std::stoi(value);
        } else if (key == "max_size") {
            max_size_ = std::stoi(value);
        } else if (key == "max_idle_time") {
            max_idle_time_ = std::stoi(value);
        } else if (key == "connection_timeout") {
            connection_timeout_ = std::stoi(value);
        }
    }

    file.close();
    return true;
}

ConnectionPool::ConnectionPool() {
	// 加载配置项
	if (!LoadConfig()) {
		return;
	}

	// 创建初始连接
	for (int i = 0; i < init_size_; ++i) {
		Connection* conn = new Connection();
		conn->Connect(ip_, port_, username_, password_, dbname_);
		conn->RefreshAliveTime(); // 刷新开始空闲的时间点
		connection_queue_.emplace(conn);
		++current_connection_size_;
	}

	// 启动一个新的线程, 作为连接的生产者
	std::thread produce(std::bind(&ConnectionPool::ProduceConnectionTask, this));
	produce.detach();
	// 启动一个新线程, 扫描超过max_idle_time 时间的空闲连接, 进行对于连接的回收
	std::thread scanner(std::bind(&ConnectionPool::CheckIdleConnectionTask, this));
	scanner.detach();
}

// 运行在独立的线程中, 专门负责生产新连接
void ConnectionPool::ProduceConnectionTask() {
	for (;;) {
		std::unique_lock<std::mutex> lock(queue_mutex_);
		// 等待队列为空
		cond_.wait(lock, [this]() { return connection_queue_.empty(); });
		// 如果连接数量没有到达上限, 继续创建新的连接
		if (current_connection_size_ < max_size_) {
			Connection* conn = new Connection();
			conn->Connect(ip_, port_, username_, password_, dbname_);
			conn->RefreshAliveTime(); // 刷新开始空闲的时间点
			connection_queue_.emplace(conn);
			++current_connection_size_;
		}
		// 通知消费者有新连接可用
		cond_.notify_all();
	}
}

// 获取可用连接
std::shared_ptr<Connection> ConnectionPool::GetConnection() {
	std::unique_lock<std::mutex> lock(queue_mutex_);
	while (connection_queue_.empty()) {
		if (std::cv_status::timeout ==
			cond_.wait_for(lock, std::chrono::microseconds(connection_timeout_))) {
			if (connection_queue_.empty()) {
				LOG("获取空闲连接超时...获取连接失败!");
				return nullptr;
			}
		}
	}

    // 自定义智能指针的删除器, 在删除时自动添加连接到队列中
	std::shared_ptr<Connection> conn(connection_queue_.front(), [&](Connection* conn){
        // 这里是在服务器应用线程中调用, 因此, 要考虑线程安全
        std::unique_lock<std::mutex> lock(queue_mutex_);
		conn->RefreshAliveTime(); // 刷新开始空闲的时间点
        connection_queue_.emplace(conn);
    });
	connection_queue_.pop();
	cond_.notify_all();	 // 通知生产者
	return conn;
}

// 运行在独立的线程中, 专门负责检查空闲连接
void ConnectionPool::CheckIdleConnectionTask(){
	for (;;){
		// 通过 sleep 模拟定时效果
		std::this_thread::sleep_for(std::chrono::seconds(max_idle_time_));
		// 扫描整个队列, 释放多余的连接
		std::unique_lock<std::mutex> lock(queue_mutex_);
		while (current_connection_size_ > init_size_){
			Connection* conn = connection_queue_.front();
			if (conn->GetAliveTime() > max_idle_time_ * 1000){
				connection_queue_.pop();
				--current_connection_size_;
				delete conn;
			} else {
				break; // 队头的连接没有超时, 之后的也就没有超时
			}
		}
	}
}