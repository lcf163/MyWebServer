#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>
#include <mysql/mysql.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../sqlConnPool/sqlconnpool.h"
#include "../threadPool/threadpool.h"

using namespace std;

enum HTTP_CODE
{
    NO_REQUEST = 0,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURSE,
    FORBIDDENT_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
};

enum PARSE_STATE
{
    REQUEST_LINE = 0, // 请求行（正在解析）
    HEADERS,          // 请求头（正在解析）
    BODY,             // 请求体（正在解
    FINISH            // 解析完成
};

class HttpRequest
{
public:
    HttpRequest() { init(); }
    ~HttpRequest() = default;

    void init();
    HTTP_CODE parse(Buffer& buffer);

    string getPathConst() const { return path; }
    string& getPath() { return path; }
    string getMethod() const { return method; }
    string getVersion() const { return version; }

    bool isKeepAlive() const;

private:
    HTTP_CODE parseRequestLine(const string& line);
    HTTP_CODE parseHeader(const string& line);
    HTTP_CODE parseBody(const string& line);

    void parsePath();
    void parsePost();
    void parseFromUrlEncoded();

    static bool userVerify(const string& name, const string& pwd, bool isLogin);

    PARSE_STATE state;                    // 解析的状态
    string method, path, version, body;   // 请求方法，请求路径，协议版本，请求体
    unordered_map<string, string> header; // 请求头
    unordered_map<string, string> post;   // post 请求表单数据
    bool linger;
    size_t contentLen; 

    static const unordered_set<string> DEFAULT_HTML;          // 默认的网页
    static const unordered_map<string, int> DEFAULT_HTML_TAG;
    static int convertHex(char ch); // 转换为十六进制
};

#endif