#include <gtest/gtest.h>

#include <utility>

#include "chatclaude/httpresponse.h"

using namespace chat::claude;

class HttpResponseTest : public ::testing::Test
{
};

TEST_F(HttpResponseTest, WritesStatusLineHeadersAndContentLength)
{
    HttpResponse response;
    response.status = 200;
    response.headers.append(HttpHeader { "Content-Type", "application/json" });
    response.body = "{\"ok\":true}";

    EXPECT_EQ(response.toBytes(),
              QByteArray("HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: 11\r\n"
                         "\r\n"
                         "{\"ok\":true}"));
}

TEST_F(HttpResponseTest, WritesZeroContentLengthForAnEmptyBody)
{
    HttpResponse response;
    response.status = 202;

    EXPECT_EQ(response.toBytes(), QByteArray("HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\n\r\n"));
}

TEST_F(HttpResponseTest, KeepsHeadersInTheOrderTheyWereAdded)
{
    HttpResponse response;
    response.status = 405;
    response.headers.append(HttpHeader { "Allow", "POST" });
    response.headers.append(HttpHeader { "Connection", "close" });

    EXPECT_EQ(response.toBytes(),
              QByteArray("HTTP/1.1 405 Method Not Allowed\r\n"
                         "Allow: POST\r\n"
                         "Connection: close\r\n"
                         "Content-Length: 0\r\n"
                         "\r\n"));
}

TEST_F(HttpResponseTest, NamesEveryStatusTheBridgeAnswersWith)
{
    const std::pair<int, QByteArray> statuses[] = {
        { 200, "OK" },
        { 202, "Accepted" },
        { 400, "Bad Request" },
        { 403, "Forbidden" },
        { 404, "Not Found" },
        { 405, "Method Not Allowed" },
        { 413, "Content Too Large" },
        { 431, "Request Header Fields Too Large" },
        { 500, "Internal Server Error" },
    };

    for (const auto& [status, reason] : statuses)
    {
        HttpResponse response;
        response.status = status;

        EXPECT_TRUE(response.toBytes().startsWith("HTTP/1.1 " + QByteArray::number(status) + ' '
                                                  + reason + "\r\n"))
            << status;
    }
}

TEST_F(HttpResponseTest, LeavesTheReasonEmptyForAnUnlistedStatus)
{
    HttpResponse response;
    response.status = 599;

    EXPECT_TRUE(response.toBytes().startsWith("HTTP/1.1 599 \r\n"));
}

TEST_F(HttpResponseTest, CountsBodyBytesNotCharacters)
{
    HttpResponse response;
    response.body = QString::fromUtf8("zażółć").toUtf8();

    EXPECT_TRUE(response.toBytes().contains("Content-Length: 10\r\n"));
}
