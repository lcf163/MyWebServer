# MyWebServer
基于`Cpp`实现的`Web`服务器，支持静态资源的访问（图片、视频）。

## 项目描述
- 使用状态机解析`HTTP`请求报文，处理`GET`和`POST`请求
- 使用`vector`容器封装了一个自动扩容的缓冲区
- 使用IO复用技术`Epoll`，实现`Reactor`事件处理模式
- 使用`epoll_wait`实现定时功能，小根堆管理定时器
- 使用单例模式实现线程池与数据库连接池
- 使用阻塞队列实现日志功能，记录服务器的运行状态

## 开发环境
- Linux
- C++11
- MySQL 5.7

## 目录树
```
.
├── bin
│   └── server       可执行文件
├── build
│   └── Makefile
├── code             源代码
│   ├── buffer       自动扩容的缓冲区
│   ├── http         HTTP请求解析、响应
│   ├── lock         锁函数封装
│   ├── timer        小根堆管理的定时器
│   ├── server       服务器
│   ├── threadpool   线程池
│   ├── sqlconnpool  数据库连接池
│   ├── log          基于阻塞队列的异步日志模块
│   └── main.cpp     主函数
├── log              日志文件目录
├── Makefile
├── resources        静态资源
└── README.md       
```
## 项目启动
1、安装并配置数据库
```
# 创建数据库
create database webServer;
# 创建user表
USE webServer;
CREATE TABLE user(
    username char(30) NULL,
    password char(30) NULL
)ENGINE=InnoDB;
# 添加数据
INSERT INTO user(username, password) VALUES('yourName', 'yourPassword');

# webServer是数据库名，user是表名，需要在main函数中传入
```
2、编译运行
```
make
./bin/server
```
3、浏览器访问
```
127.0.0.1:8081
# 8081是在main函数中传入的服务器监听端口
```

## 参考资料
- Linux高性能服务器编程，游双著
- https://github.com/markparticle/WebServer
