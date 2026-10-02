#pragma once

#include <string>

struct HttpEndpoint
{
    std::string host;
    unsigned short port = 80;
    bool secure = false;
};

// Разбирает "127.0.0.1:8000" или "http://127.0.0.1:8000".
bool ParseEndpoint(const std::string& url, HttpEndpoint& endpoint);

// POST c JSON-телом. Кладёт код ответа в statusCode, тело в response.
// Возвращает false, если сервер недоступен.
bool HttpPostJson(const HttpEndpoint& endpoint, const std::string& path,
                  const std::string& body, unsigned& statusCode, std::string& response);
