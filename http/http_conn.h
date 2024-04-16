#ifndef _HTTPCONN_H
#define _HTTPCONN_H

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../CGImysql/sql_connection_pool.h"

class http_conn {
  public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD //请求的方法
    { GET = 0,
      POST,
      HEAD,
      PUT,
      DELETE,
      TRACE,
      OPTIONS,
      CONNECT,
      PATH };
    enum CHECK_STATE //主状态机的状态
    { CHECK_STATE_REQUESTLINE = 0,
      CHECK_STATE_HEADER,
      CHECK_STATE_CONTENT };
    enum HTTP_CODE //报文解析的结果
    { NO_REQUEST, //请求不完整，需要继续读取请求报文数据
      GET_REQUEST, //获得了完整的HTTP请求
      BAD_REQUEST, //HTTP请求报文有语法错误
      NO_RESOURCE,
      FORBIDDEN_REQUEST,
      FILE_REQUEST,
      INTERNAL_ERROR,
      CLOSED_CONNECTION };
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

  public:
    http_conn(){}
    ~http_conn(){}

  public:
    //初始化套接字地址
    void init(int sockfd, const sockaddr_in& addr, char*, int, int,
              std::string user, std::string passwd, std::string sqlname);
    //关闭连接
    void close_conn(bool real_close = true);
    void process();
    //读取浏览器端发送的全部数据
    bool read_once();
    //响应报文写入
    bool write();
    sockaddr_in* get_address() { return &m_address; }
    void initmysql_result(connection_pool* connPool);
    int timer_flag;
    int improv;

  private:
    void init();
    //读取报文并处理,对应的报文数据被read_once存储在m_read_buf
    HTTP_CODE process_read();
    //将响应报文写入到m_write_buf中
    bool process_write(HTTP_CODE ret);
    //分析请求行
    HTTP_CODE parse_request_line(char* text);
    //分析请求头
    HTTP_CODE parse_headers(char* text);
    //分析请求内容
    HTTP_CODE parse_content(char* text);
    //根据请求的URL来编辑一个m_real_url记录文件真实地址，再将该地址通过mmap映射到m_file_address
    HTTP_CODE do_request();

    char* get_line() { return m_read_buf + m_start_line; }

    //处理一行的数据，每次识别到一个"\r\n"就停下并返回LINE_OK
    LINE_STATUS parse_line();

    void unmap();

    //生成响应报文的格式
    //通过add_response接收可变数量的参数来达到写入write_buf的效果
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

  public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL* mysql;
    int m_state; //读为0，写为1

  private:
    int m_sockfd;
    sockaddr_in m_address;

    //存储读取出的请求报文数据
    char m_read_buf[READ_BUFFER_SIZE];
    //最后一个字节的下一个角标，用来控制遍历
    long m_read_idx;
    //存储读取到read_buf的哪一个角标
    long m_checked_idx;
    // read_buf中已经解析的字符个数
    int m_start_line;

    //存储将要发出的响应报文的数据
    char m_write_buf[WRITE_BUFFER_SIZE];
    // write_buffer的长度
    int m_write_idx;

    //主状态机的分析状态
    CHECK_STATE m_check_state;
    //请求方法
    METHOD m_method;

    //解析请求报文中对应的变量
    //存储读取文件的名称
    char m_real_file[FILENAME_LEN];
    char* m_url;
    char* m_version;
    char* m_host;
    long m_content_length;
    bool m_linger; //是否需要长连接

    //服务器中的文件地址,由m_real_file中所存储的路径通过mmap映射而来
    char* m_file_address;
    struct stat m_file_stat;
    //iovec有两个成员变量,地址iov_base 长度iov_len
    //m_iv[0]为m_write_buf与m_write_idx m_iv[1]为file_address和m_file_stat.st_size
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;             //是否启用POST
    char* m_string;      //存储POST请求中的content
    int bytes_to_send;   //剩余发送字节数
    int bytes_have_send; //已发送字节数

    //存储静态资源的路径名称
    char* doc_root;

    map<string, string> m_users;
    int m_TRIGMode; //控制ET还是LT，0为LT，1为ET
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];

public:
    int get_sockfd() { return m_sockfd; }
};
#endif