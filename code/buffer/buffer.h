#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include <vector>
#include <atomic>
#include <cassert>

using namespace std;

class Buffer
{
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    size_t writableBytes() const;
    size_t readableBytes() const;
    size_t prependableBytes() const;

    const char* peek() const;
    void ensureWritable(size_t len);
    void hasWritten(size_t len);

    void retrieve(size_t len);
    void retrieveUntil(const char* end);

    void retrieveAll();
    string retrieveAllToStr();

    const char* beginWriteConst() const;
    char* beginWrite();

    void append(const string& str);
    void append(const char* str, size_t len);
    void append(const void* data, size_t len);
    void append(const Buffer& buffer);

    ssize_t readFd(int fd, int* Errno);
    ssize_t writeFd(int fd, int* Errno);

private:
    char* beginPtr();             // 获取内存起始位置
    const char* beginPtr() const; // 获取内存起始位置
    void makeSpace(size_t len);   // 创建空间

    vector<char> buffer;     // 存放数据的vector
    atomic<size_t> readPos;  // 读的位置
    atomic<size_t> writePos; // 写的位置
};

#endif