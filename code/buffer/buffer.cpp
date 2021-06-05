#include "buffer.h"

using namespace std;

/*
    内存模型：
    begin---read---write---end

    begin-read: prependable
    read-write: readable
    begin-end: writalbe
*/

Buffer::Buffer(int initBuffSize) : buffer(initBuffSize), readPos(0), writePos(0) {}

// 可读的数据的大小（写位置 - 读位置）
size_t Buffer::readableBytes() const
{
    return writePos - readPos;
}

// 可写的数据的大小（缓冲区的大小 - 写位置）
size_t Buffer::writableBytes() const
{
    return buffer.size() - writePos;
}

// 前面可用的空间（已读完）
size_t Buffer::prependableBytes() const
{
    return readPos;
}

// 开始读的位置
const char* Buffer::peek() const
{
    return beginPtr() + readPos;
}

// 移动读的位置，回收内存
void Buffer::retrieve(size_t len)
{
    assert(len <= readableBytes());
    readPos += len;
}

// 判断读的位置是否可以移动
void Buffer::retrieveUntil(const char* end)
{
    assert(peek() <= end);
    retrieve(end - peek());
}

// 重置缓冲区
void Buffer::retrieveAll()
{
    bzero(&buffer[0], buffer.size());
    readPos = 0;
    writePos = 0;
}

// 可读数据转换为字符串
string Buffer::retrieveAllToStr()
{
    string str(peek(), readableBytes());
    retrieveAll();
    return str;
}

// 开始写的位置
const char* Buffer::beginWriteConst() const
{
    return beginPtr() + writePos;
}

char* Buffer::beginWrite()
{
    return beginPtr() + writePos;
}

// 更新写的位置
void Buffer::hasWritten(size_t len)
{
    writePos += len;
}

// 临时的数据buff，追加到缓冲区
void Buffer::append(const string& str)
{
    append(str.data(), str.length());
}

void Buffer::append(const void* data, size_t len)
{
    assert(data);
    append(static_cast<const char*>(data), len);
}

void Buffer::append(const char* str, size_t len)
{
    assert(str);
    ensureWritable(len);
    copy(str, str + len, beginWrite());
    hasWritten(len);
}

void Buffer::append(const Buffer& buffer)
{
    append(buffer.peek(), buffer.readableBytes());
}

// 确保容量足够
void Buffer::ensureWritable(size_t len)
{
    if (writableBytes() < len)
    {
        makeSpace(len);
    }
    assert(writableBytes() >= len);
}

// 读入请求数据的内容（文件描述符）
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char newbuffer[65536];
    struct iovec iov[2];
    const size_t writable = writableBytes();

    // 分散读
    iov[0].iov_base = beginPtr() + writePos;
    iov[0].iov_len = writable;
    iov[1].iov_base = newbuffer;
    iov[1].iov_len = sizeof(newbuffer);

    const ssize_t len = readv(fd, iov, 2);
    if (len < 0)
    {
        *saveErrno = errno;
    }
    else if (static_cast<size_t>(len) <= writable) // 当前缓冲区的容量足够
    {
        writePos += len;
    }
    // 当前缓冲区的容量不够，加入临时数组中
    else
    {
        writePos = buffer.size();
        append(newbuffer, len - writable);
    }
    return len;
}

// 写入响应数据的内容（文件描述符）
ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    size_t readSize = readableBytes();
    ssize_t len = write(fd, peek(), readSize);
    if (len < 0)
    {
        *saveErrno = errno;
        return len;
    }
    readPos += len;
    return len;
}

char* Buffer::beginPtr()
{
    return &*buffer.begin();
}

const char* Buffer::beginPtr() const
{
    return &*buffer.begin();
}

// 容量不够时扩容
void Buffer::makeSpace(size_t len)
{
    // 可用空间不足
    if (writableBytes() + prependableBytes() < len)
    {
        buffer.resize(writePos + len + 1); // 扩容
    }
    // 可用空间足够
    else
    {
        size_t readable = readableBytes();
        // 移动可读数据到缓冲区头部
        copy(beginPtr() + readPos, beginPtr() + writePos, beginPtr());
        readPos = 0;
        writePos = readPos + readable;
        assert(readable == readableBytes());
    }
}
