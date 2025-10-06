cmake_minimum_required(VERSION 3.10)
project(connection_pool)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置MySQL头文件和库文件路径
set(MYSQL_INCLUDE_DIR "D:/MySQL/MySQL Server 8.0/include")
set(MYSQL_LIB_DIR "D:/MySQL/MySQL Server 8.0/lib")

include_directories(${MYSQL_INCLUDE_DIR})
link_directories(${MYSQL_LIB_DIR})

# 添加可执行文件，包含所有源文件
add_executable(connection_pool main.cpp connection.cpp connection_pool.cpp connection.h connection_pool.h)

# 链接MySQL库，修复Windows下corrupt .drectve警告
if(WIN32)
    # 对于Windows MinGW环境，使用完整的库文件名
    target_link_libraries(connection_pool "${MYSQL_LIB_DIR}/libmysql.lib")
else()
    # 对于其他平台
    target_link_libraries(connection_pool mysqlclient)
endif()