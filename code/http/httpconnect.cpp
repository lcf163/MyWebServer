#include "httpconnect.h"

using namespace std;

const char* HttpConnect::srcDir;
atomic<int> HttpConnect::userCnt;
bool HttpConnect::isET;

HttpConnect::HttpConnect()
{
    fd = -1;
    addr = {0};
    isClose = true;
}

HttpConnect::~HttpConnect()
{
    closeConnect();
}

// 连接对象初始化
void HttpConnect::init(int fd, const sockaddr_in& addr)
{
    assert(fd > 0);
    userCnt ++;
    this->addr = addr;
    this->fd = fd;
    writeBuffer.retrieveAll();
    readBuffer.retrieveAll();
    isClose = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd, getIP(), getPort(), (int)userCnt);
}

// 关闭连接
void HttpConnect::closeConnect()
{
    response.unmapFile();
    if (!isClose)
    {
        isClose = true;
        userCnt --;
        close(fd);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd, getIP(), getPort(), (int)userCnt);
    }
}

int HttpConnect::getFd() const 
{ 
    return fd; 
}

int HttpConnect::getPort() const 
{ 
    return addr.sin_port; 
}

const char* HttpConnect::getIP() const 
{ 
    return inet_ntoa(addr.sin_addr); 
}
    
sockaddr_in HttpConnect::getAddr() const 
{ 
    return addr; 
}

/* 
    读方法，ET模式将缓存读空  
    读入数据到读缓冲区
*/
ssize_t HttpConnect::read(int* saveErrno)
{
    // 最后一次读取的长度
    ssize_t len = -1;
    do
    {
        len = readBuffer.readFd(fd, saveErrno);
        if (len <= 0)
        {
            *saveErrno = errno;
            break;
        }
    } while (isET);
    return len;
}

/* 
    写方法，响应头和响应体分开传输
    写入数据到写缓冲区
*/
ssize_t HttpConnect::write(int* saveErrno)
{
    // 最后一次写入的长度
    ssize_t len = -1;
    do
    {
        len = writev(fd, iov, iovCnt);
        if (len <= 0)
        {
            *saveErrno = errno;
            break;
        }
        // 缓存为空，传输完成
        if (iov[0].iov_len + iov[1].iov_len == 0) 
        { 
            break; 
        }
        // 响应头已经传输完成
        else if (static_cast<size_t>(len) > iov[0].iov_len)
        {
            // 更新响应体的传输起点和长度
            iov[1].iov_base = (uint8_t*)iov[1].iov_base + (len - iov[0].iov_len);
            iov[1].iov_len -= (len - iov[0].iov_len);
            // 响应头不再需要传输
            if (iov[0].iov_len)
            {
                iov[0].iov_len = 0;        // 响应头保存在写缓存中，全部回收
                writeBuffer.retrieveAll(); // 写缓冲区，重置
            }
        }
        // 响应头还没传输完成
        else
        {
            // 更新响应头的传输起点和长度
            iov[0].iov_base = (uint8_t*)iov[0].iov_base + len;
            iov[0].iov_len -= len;
            writeBuffer.retrieve(len);
        }
    } while (isET || toWriteBytes() > 10240); // 一次最多传输10MB数据
    return len;
}

/* 
    处理方法（处理业务逻辑，解析 HTTP 请求）
    解析读缓存内的请求报文，判断是否完整：
        如果不完整，返回 false；
        如果完整，在写缓存中写入响应头，并且获取响应体内容（文件）
*/
bool HttpConnect::process()
{
    request.init();
    if (readBuffer.readableBytes() <= 0) 
    {
        return false;
    }

    HTTP_CODE ret = request.parse(readBuffer);
    // 请求不完整，继续读
    if (ret == HTTP_CODE::NO_REQUEST)
    {
        return false; // 返回false后，会继续监听读
    }
    // 请求完整，开始写
    else if (ret == HTTP_CODE::GET_REQUEST)
    {
        LOG_DEBUG("%s", request.getPathConst().c_str());
        response.init(srcDir, request.getPath(), request.isKeepAlive(), 200);
    }
    // 请求行错误
    else if (ret == HTTP_CODE::BAD_REQUEST)
    {
        response.init(srcDir, request.getPath(), false, 400);
    }

    response.makeResponse(writeBuffer);
    // 响应头
    iov[0].iov_base = (char*)writeBuffer.peek();
    iov[0].iov_len = writeBuffer.readableBytes();
    iovCnt = 1;
    // 响应体
    if (response.getFileLen() > 0 && response.getFile())
    {
        iov[1].iov_base = response.getFile();
        iov[1].iov_len = response.getFileLen();
        iovCnt = 2;
    }

    LOG_DEBUG("filesize:%d, iovcnt:%d, write:%d bytes", response.getFileLen(), iovCnt, toWriteBytes());
    return true;
}
