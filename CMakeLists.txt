cmake_minimum_required(VERSION 3.0)
project(webserver)

include_directories($(CMAKE_SOURCE_DIR))
include_directories($(CMAKE_SOURCE_DIR)/http)
include_directories($(CMAKE_SOURCE_DIR)/lock)
include_directories($(CMAKE_SOURCE_DIR)/log)
include_directories($(CMAKE_SOURCE_DIR)/CGImysql)
include_directories($(CMAKE_SOURCE_DIR)/threadpool)
include_directories($(CMAKE_SOURCE_DIR)/timer)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        main.cpp
        http/http_conn.cpp
        http/http_conn.h
        lock/locker.h
        log/block_queue.h
        log/log.cpp
        log/log.h
        CGImysql/sql_connection_pool.cpp
        CGImysql/sql_connection_pool.h
        threadpool/threadpool.h
        timer/lst_timer.cpp
        timer/lst_timer.h
        config.h
        config.cpp
        webserver.cpp
        webserver.h
        )

add_executable(webserver ${SOURCE_FILES})

target_link_libraries(webserver -lpthread -lmysqlclient)