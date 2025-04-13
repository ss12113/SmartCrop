#ifndef __HTTP_CLIENT_H
#define __HTTP_CLIENT_H

#include "sys.h"
#include <stdbool.h>

typedef struct {
    char url[128];
    char headers[256];
} HttpClient;

typedef struct {
    int statusCode;
    char body[1024];
} HttpResponse;

// 初始化HTTP客户端库
void HttpClientLibInit(void);

// 使用URL初始化HTTP客户端
void HttpClientInit(HttpClient* client, const char* url);

// 设置请求头
void HttpClientSetHeader(HttpClient* client, const char* headerFormat, ...);

// 发送POST请求
bool HttpClientPost(HttpClient* client, const char* body, HttpResponse* response);

// 清理HTTP客户端使用的资源
void HttpClientCleanup(HttpClient* client);

// 解析JSON响应以提取内容
bool ParseJsonResponse(const char* jsonStr, char* output, size_t outputSize);

#endif /* __HTTP_CLIENT_H */
