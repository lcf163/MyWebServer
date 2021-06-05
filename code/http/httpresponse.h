#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "../buffer/buffer.h"
#include "../log/log.h"

using namespace std;

class HttpResponse
{
public:
    HttpResponse();
    ~HttpResponse();

    void init(const string& srcDir, string& path, bool isKeepAlive = false, int code = -1);
    void makeResponse(Buffer& buffer);
    void unmapFile();
    char* getFile();
    size_t getFileLen() const;
    void errorContent(Buffer& buffer, string message);
    int getCode() const;

private:
    void addState(Buffer& buffer);
    void addHeader(Buffer& buffer);
    void addContent(Buffer& buffer);

    void errorHtml();
    string getFileType();

    int code;         // 响应状态码
    bool isKeepAlive; // 是否保持连接

    string path;      // 资源的路径
    string srcDir;    // 资源的目录

    char* mmFile;     // 文件内存映射的指针
    struct stat mmFileStat; // 文件的状态信息

    static const unordered_map<string, string> SUFFIX_TYPE; // 后缀 -> 类型
    static const unordered_map<int, string> CODE_STATUS;    // 状态码 -> 描述
    static const unordered_map<int, string> CODE_PATH;      // 状态码 -> 路径
};

#endif