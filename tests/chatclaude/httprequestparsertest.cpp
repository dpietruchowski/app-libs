#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "chatclaude/httprequestparser.h"
#include "fixturefile.h"

using namespace chat::claude;

namespace
{

QList<HttpRequest> takeAll(HttpRequestParser& parser)
{
    QList<HttpRequest> requests;
    while (auto request = parser.takeRequest())
        requests.append(std::move(*request));
    return requests;
}

QByteArray post(const QByteArray& path, const QByteArray& body)
{
    return "POST " + path + " HTTP/1.1\r\nContent-Length: " + QByteArray::number(body.size())
        + "\r\n\r\n" + body;
}

HttpParseError errorOf(const QByteArray& wire, HttpParserLimits limits = {})
{
    HttpRequestParser parser(limits);
    parser.feed(wire);
    EXPECT_FALSE(parser.takeRequest().has_value());
    EXPECT_TRUE(parser.failed());
    return parser.error();
}

QList<HttpRequest> parseInChunks(const QByteArray& wire, qsizetype chunkSize)
{
    HttpRequestParser parser;
    QList<HttpRequest> requests;
    for (qsizetype offset = 0; offset < wire.size(); offset += chunkSize)
    {
        parser.feed(wire.mid(offset, chunkSize));
        requests.append(takeAll(parser));
    }
    EXPECT_FALSE(parser.failed());
    return requests;
}

QList<QJsonObject> recordedRequests(const char* fixture)
{
    QList<QJsonObject> entries;
    for (const QByteArray& line : chat::test::readFixture(fixture).split('\n'))
    {
        if (!line.trimmed().isEmpty())
            entries.append(QJsonDocument::fromJson(line).object());
    }
    return entries;
}

QByteArray wireFormat(const QJsonObject& entry)
{
    QByteArray wire = entry.value("method").toString().toUtf8() + ' '
        + entry.value("path").toString().toUtf8() + ' ' + entry.value("version").toString().toUtf8()
        + "\r\n";
    for (const QJsonValue& header : entry.value("headers").toArray())
    {
        const QJsonArray pair = header.toArray();
        wire += pair.at(0).toString().toUtf8() + ": " + pair.at(1).toString().toUtf8() + "\r\n";
    }
    wire += "\r\n";
    wire += entry.value("body").toString().toUtf8();
    return wire;
}

QByteArray wireFormatOf(const QList<QJsonObject>& entries)
{
    QByteArray wire;
    for (const QJsonObject& entry : entries)
        wire += wireFormat(entry);
    return wire;
}

void expectMatchesRecording(const HttpRequest& request, const QJsonObject& entry)
{
    EXPECT_EQ(request.method, entry.value("method").toString().toUtf8());
    EXPECT_EQ(request.path, entry.value("path").toString().toUtf8());
    EXPECT_EQ(request.body, entry.value("body").toString().toUtf8());

    const QJsonArray headers = entry.value("headers").toArray();
    ASSERT_EQ(request.headers.size(), headers.size());
    for (qsizetype i = 0; i < headers.size(); ++i)
    {
        EXPECT_EQ(request.headers.at(i).name, headers.at(i).toArray().at(0).toString().toUtf8());
        EXPECT_EQ(request.headers.at(i).value, headers.at(i).toArray().at(1).toString().toUtf8());
    }
}

}

class HttpRequestParserTest : public ::testing::Test
{
};

TEST_F(HttpRequestParserTest, ParsesGetWithoutBody)
{
    HttpRequestParser parser;
    parser.feed("GET /mcp/courses HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");

    const auto request = parser.takeRequest();

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->method, QByteArray("GET"));
    EXPECT_EQ(request->path, QByteArray("/mcp/courses"));
    EXPECT_EQ(request->header("Host"), QByteArray("127.0.0.1"));
    EXPECT_TRUE(request->body.isEmpty());
    EXPECT_FALSE(parser.takeRequest().has_value());
    EXPECT_FALSE(parser.failed());
}

TEST_F(HttpRequestParserTest, ParsesPostBodyByContentLength)
{
    HttpRequestParser parser;
    parser.feed(post("/mcp", "{\"id\":1}"));

    const auto request = parser.takeRequest();

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->method, QByteArray("POST"));
    EXPECT_EQ(request->body, QByteArray("{\"id\":1}"));
}

TEST_F(HttpRequestParserTest, ZeroContentLengthGivesEmptyBody)
{
    HttpRequestParser parser;
    parser.feed("POST /mcp HTTP/1.1\r\nContent-Length: 0\r\n\r\n");

    const auto request = parser.takeRequest();

    ASSERT_TRUE(request.has_value());
    EXPECT_TRUE(request->body.isEmpty());
}

TEST_F(HttpRequestParserTest, BodyMayContainTheHeadTerminator)
{
    HttpRequestParser parser;
    parser.feed(post("/mcp", "a\r\n\r\nb"));

    const auto request = parser.takeRequest();

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->body, QByteArray("a\r\n\r\nb"));
}

TEST_F(HttpRequestParserTest, HeaderLookupIgnoresCaseAndTrimsValue)
{
    HttpRequestParser parser;
    parser.feed("GET / HTTP/1.1\r\nauthorization:   Bearer abc \t\r\nX-Empty:\r\n\r\n");

    const auto request = parser.takeRequest();

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->header("Authorization"), QByteArray("Bearer abc"));
    EXPECT_EQ(request->header("AUTHORIZATION"), QByteArray("Bearer abc"));
    EXPECT_EQ(request->header("x-empty"), QByteArray(""));
    EXPECT_FALSE(request->header("Missing").has_value());
}

TEST_F(HttpRequestParserTest, KeepsHeaderOrderAndNameCase)
{
    HttpRequestParser parser;
    parser.feed("GET / HTTP/1.1\r\nmcp-method: x\r\nAccept: y\r\nAccept: z\r\n\r\n");

    const auto request = parser.takeRequest();

    ASSERT_TRUE(request.has_value());
    ASSERT_EQ(request->headers.size(), 3);
    EXPECT_EQ(request->headers.at(0).name, QByteArray("mcp-method"));
    EXPECT_EQ(request->headers.at(1).name, QByteArray("Accept"));
    EXPECT_EQ(request->headers.at(2).value, QByteArray("z"));
    EXPECT_EQ(request->header("accept"), QByteArray("y"));
}

TEST_F(HttpRequestParserTest, WaitsForTheRestOfTheHead)
{
    HttpRequestParser parser;
    parser.feed("GET / HTTP/1.1\r\nHost: a\r\n\r");

    EXPECT_FALSE(parser.takeRequest().has_value());
    EXPECT_FALSE(parser.failed());

    parser.feed("\n");
    EXPECT_TRUE(parser.takeRequest().has_value());
}

TEST_F(HttpRequestParserTest, WaitsForTheRestOfTheBody)
{
    const QByteArray wire = post("/mcp", "0123456789");
    HttpRequestParser parser;
    parser.feed(wire.left(wire.size() - 4));

    EXPECT_FALSE(parser.takeRequest().has_value());
    EXPECT_FALSE(parser.failed());

    parser.feed(wire.right(4));
    const auto request = parser.takeRequest();
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->body, QByteArray("0123456789"));
}

TEST_F(HttpRequestParserTest, ByteByByteDeliversTheRequestOnlyOnTheLastByte)
{
    const QByteArray wire = post("/mcp", "{\"a\":1}");
    HttpRequestParser parser;

    for (qsizetype i = 0; i < wire.size() - 1; ++i)
    {
        parser.feed(wire.mid(i, 1));
        ASSERT_FALSE(parser.takeRequest().has_value()) << "at byte " << i;
    }
    parser.feed(wire.right(1));

    const auto request = parser.takeRequest();
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->body, QByteArray("{\"a\":1}"));
    EXPECT_FALSE(parser.takeRequest().has_value());
}

TEST_F(HttpRequestParserTest, SeveralRequestsInOneChunk)
{
    HttpRequestParser parser;
    parser.feed(post("/one", "1") + "GET /two HTTP/1.1\r\n\r\n" + post("/three", "333"));

    const QList<HttpRequest> requests = takeAll(parser);

    ASSERT_EQ(requests.size(), 3);
    EXPECT_EQ(requests.at(0).path, QByteArray("/one"));
    EXPECT_EQ(requests.at(0).body, QByteArray("1"));
    EXPECT_EQ(requests.at(1).path, QByteArray("/two"));
    EXPECT_EQ(requests.at(2).body, QByteArray("333"));
}

TEST_F(HttpRequestParserTest, NextRequestStartingMidChunk)
{
    const QByteArray second = post("/second", "22");
    HttpRequestParser parser;
    parser.feed(post("/first", "1") + second.left(10));

    const QList<HttpRequest> first = takeAll(parser);
    ASSERT_EQ(first.size(), 1);
    EXPECT_EQ(first.at(0).path, QByteArray("/first"));

    parser.feed(second.mid(10));
    const QList<HttpRequest> rest = takeAll(parser);
    ASSERT_EQ(rest.size(), 1);
    EXPECT_EQ(rest.at(0).body, QByteArray("22"));
}

TEST_F(HttpRequestParserTest, AcceptsIdenticalDuplicateContentLength)
{
    HttpRequestParser parser;
    parser.feed("POST / HTTP/1.1\r\nContent-Length: 3\r\ncontent-length: 3\r\n\r\nabc");

    const auto request = parser.takeRequest();

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->body, QByteArray("abc"));
}

TEST_F(HttpRequestParserTest, RejectsMalformedRequestLine)
{
    EXPECT_EQ(errorOf("\r\n\r\n"), HttpParseError::MalformedRequestLine);
    EXPECT_EQ(errorOf("GET /\r\n\r\n"), HttpParseError::MalformedRequestLine);
    EXPECT_EQ(errorOf("GET mcp HTTP/1.1\r\n\r\n"), HttpParseError::MalformedRequestLine);
    EXPECT_EQ(errorOf("G(T / HTTP/1.1\r\n\r\n"), HttpParseError::MalformedRequestLine);
    EXPECT_EQ(errorOf(" / HTTP/1.1\r\n\r\n"), HttpParseError::MalformedRequestLine);
    EXPECT_EQ(errorOf("GET /a b HTTP/1.1\r\n\r\n"), HttpParseError::MalformedRequestLine);
    EXPECT_EQ(errorOf("GET / HTTP/1.1\nHost: x\r\n\r\n"), HttpParseError::MalformedRequestLine);
}

TEST_F(HttpRequestParserTest, RejectsOtherHttpVersions)
{
    EXPECT_EQ(errorOf("GET / HTTP/1.0\r\n\r\n"), HttpParseError::UnsupportedVersion);
    EXPECT_EQ(errorOf("GET / HTTP/2.0\r\n\r\n"), HttpParseError::UnsupportedVersion);
    EXPECT_EQ(errorOf("GET / http/1.1\r\n\r\n"), HttpParseError::UnsupportedVersion);
}

TEST_F(HttpRequestParserTest, RejectsMalformedHeaders)
{
    EXPECT_EQ(errorOf("GET / HTTP/1.1\r\nNoColon\r\n\r\n"), HttpParseError::MalformedHeader);
    EXPECT_EQ(errorOf("GET / HTTP/1.1\r\nHost : x\r\n\r\n"), HttpParseError::MalformedHeader);
    EXPECT_EQ(errorOf("GET / HTTP/1.1\r\n: x\r\n\r\n"), HttpParseError::MalformedHeader);
    EXPECT_EQ(errorOf("GET / HTTP/1.1\r\nX-A: 1\r\n  folded\r\n\r\n"),
              HttpParseError::MalformedHeader);
    EXPECT_EQ(errorOf("GET / HTTP/1.1\r\nX-A: a\x01"
                      "b\r\n\r\n"),
              HttpParseError::MalformedHeader);
    EXPECT_EQ(errorOf("GET / HTTP/1.1\r\nX-A: a\rb\r\n\r\n"), HttpParseError::MalformedHeader);
}

TEST_F(HttpRequestParserTest, RejectsInvalidContentLength)
{
    EXPECT_EQ(errorOf("POST / HTTP/1.1\r\nContent-Length: abc\r\n\r\n"),
              HttpParseError::InvalidContentLength);
    EXPECT_EQ(errorOf("POST / HTTP/1.1\r\nContent-Length: -1\r\n\r\n"),
              HttpParseError::InvalidContentLength);
    EXPECT_EQ(errorOf("POST / HTTP/1.1\r\nContent-Length: +5\r\n\r\n"),
              HttpParseError::InvalidContentLength);
    EXPECT_EQ(errorOf("POST / HTTP/1.1\r\nContent-Length: 1 2\r\n\r\n"),
              HttpParseError::InvalidContentLength);
    EXPECT_EQ(errorOf("POST / HTTP/1.1\r\nContent-Length:\r\n\r\n"),
              HttpParseError::InvalidContentLength);
    EXPECT_EQ(errorOf("POST / HTTP/1.1\r\nContent-Length: 99999999999999999999\r\n\r\n"),
              HttpParseError::InvalidContentLength);
}

TEST_F(HttpRequestParserTest, RejectsConflictingContentLength)
{
    EXPECT_EQ(errorOf("POST / HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\nabcde"),
              HttpParseError::InvalidContentLength);
}

TEST_F(HttpRequestParserTest, RejectsTransferEncoding)
{
    EXPECT_EQ(errorOf("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n"),
              HttpParseError::UnsupportedTransferEncoding);
    EXPECT_EQ(
        errorOf("POST / HTTP/1.1\r\ntransfer-encoding: chunked\r\nContent-Length: 1\r\n\r\n0"),
        HttpParseError::UnsupportedTransferEncoding);
}

TEST_F(HttpRequestParserTest, BodyAtTheLimitIsAcceptedAndOneOverIsRejectedBeforeItArrives)
{
    HttpParserLimits limits;
    limits.maxBodyBytes = 16;

    HttpRequestParser accepting(limits);
    accepting.feed(post("/", QByteArray(16, 'x')));
    EXPECT_TRUE(accepting.takeRequest().has_value());

    EXPECT_EQ(errorOf("POST / HTTP/1.1\r\nContent-Length: 17\r\n\r\n", limits),
              HttpParseError::BodyTooLarge);
}

TEST_F(HttpRequestParserTest, HeadAtTheLimitIsAcceptedAndOneOverIsRejected)
{
    const QByteArray head = "GET / HTTP/1.1\r\nX: ";
    HttpParserLimits limits;
    limits.maxHeadBytes = 64;

    HttpRequestParser accepting(limits);
    accepting.feed(head + QByteArray(64 - head.size() - 4, 'v') + "\r\n\r\n");
    EXPECT_TRUE(accepting.takeRequest().has_value());

    EXPECT_EQ(errorOf(head + QByteArray(64 - head.size() - 3, 'v') + "\r\n\r\n", limits),
              HttpParseError::HeadersTooLarge);
}

TEST_F(HttpRequestParserTest, EndlessHeadWithoutTerminatorFailsWithoutWaiting)
{
    HttpParserLimits limits;
    limits.maxHeadBytes = 64;

    EXPECT_EQ(errorOf(QByteArray(65, 'a'), limits), HttpParseError::HeadersTooLarge);

    HttpRequestParser waiting(limits);
    waiting.feed(QByteArray(64, 'a'));
    EXPECT_FALSE(waiting.takeRequest().has_value());
    EXPECT_FALSE(waiting.failed());
}

TEST_F(HttpRequestParserTest, DeliversRequestsCompletedBeforeTheMalformedOne)
{
    HttpRequestParser parser;
    parser.feed(post("/good", "ok") + "GET / HTTP/9\r\n\r\n");

    const auto good = parser.takeRequest();
    ASSERT_TRUE(good.has_value());
    EXPECT_EQ(good->path, QByteArray("/good"));
    EXPECT_FALSE(parser.failed());

    EXPECT_FALSE(parser.takeRequest().has_value());
    EXPECT_EQ(parser.error(), HttpParseError::UnsupportedVersion);
}

TEST_F(HttpRequestParserTest, FailureIsFinal)
{
    HttpRequestParser parser;
    parser.feed("garbage\r\n\r\n");
    EXPECT_FALSE(parser.takeRequest().has_value());
    ASSERT_TRUE(parser.failed());

    parser.feed(post("/mcp", "{}"));

    EXPECT_FALSE(parser.takeRequest().has_value());
    EXPECT_EQ(parser.error(), HttpParseError::MalformedRequestLine);
}

TEST_F(HttpRequestParserTest, ErrorIsNoneBeforeAnyFailure)
{
    HttpRequestParser parser;

    EXPECT_FALSE(parser.failed());
    EXPECT_EQ(parser.error(), HttpParseError::None);
}

TEST_F(HttpRequestParserTest, RecordedCliSequenceParsesAsSent)
{
    const QList<QJsonObject> entries = recordedRequests("sync-mcp-requests.log");
    ASSERT_EQ(entries.size(), 6);

    HttpRequestParser parser;
    parser.feed(wireFormatOf(entries));
    const QList<HttpRequest> requests = takeAll(parser);

    EXPECT_FALSE(parser.failed());
    ASSERT_EQ(requests.size(), entries.size());
    for (qsizetype i = 0; i < entries.size(); ++i)
        expectMatchesRecording(requests.at(i), entries.at(i));
}

TEST_F(HttpRequestParserTest, RecordedCliSequenceSurvivesAnyChunking)
{
    const QList<QJsonObject> entries = recordedRequests("sync-mcp-requests.log");
    const QByteArray wire = wireFormatOf(entries);

    for (const qsizetype chunkSize : { 1, 2, 7, 64, 1000 })
    {
        const QList<HttpRequest> requests = parseInChunks(wire, chunkSize);

        ASSERT_EQ(requests.size(), entries.size()) << "chunk size " << chunkSize;
        for (qsizetype i = 0; i < entries.size(); ++i)
            expectMatchesRecording(requests.at(i), entries.at(i));
    }
}

TEST_F(HttpRequestParserTest, RecordedAsyncSequenceParsesAsSent)
{
    const QList<QJsonObject> entries = recordedRequests("mcp-requests.log");
    ASSERT_FALSE(entries.isEmpty());

    HttpRequestParser parser;
    parser.feed(wireFormatOf(entries));
    const QList<HttpRequest> requests = takeAll(parser);

    EXPECT_FALSE(parser.failed());
    ASSERT_EQ(requests.size(), entries.size());
    for (qsizetype i = 0; i < entries.size(); ++i)
        expectMatchesRecording(requests.at(i), entries.at(i));
}

TEST_F(HttpRequestParserTest, RecordedRejectedSequenceParsesAsSent)
{
    const QList<QJsonObject> entries = recordedRequests("mcp-403-requests.log");
    ASSERT_EQ(entries.size(), 2);

    HttpRequestParser parser;
    parser.feed(wireFormatOf(entries));
    const QList<HttpRequest> requests = takeAll(parser);

    ASSERT_EQ(requests.size(), 2);
    EXPECT_EQ(requests.at(0).header("Authorization"), QByteArray("Bearer wrong-token"));
    EXPECT_EQ(requests.at(1).header("Authorization"), QByteArray("Bearer wrong-token"));
}

TEST_F(HttpRequestParserTest, RecordedBodiesMatchTheirContentLength)
{
    for (const char* fixture :
         { "sync-mcp-requests.log", "mcp-requests.log", "mcp-403-requests.log" })
    {
        const QList<QJsonObject> entries = recordedRequests(fixture);
        ASSERT_FALSE(entries.isEmpty()) << fixture;

        HttpRequestParser parser;
        parser.feed(wireFormatOf(entries));
        for (const HttpRequest& request : takeAll(parser))
        {
            const auto length = request.header("Content-Length");
            if (length)
                EXPECT_EQ(length->toLongLong(), request.body.size()) << fixture;
        }
    }
}

TEST_F(HttpRequestParserTest, RecordedCliSequenceHasTheFactsTheBridgeRelaysOn)
{
    const QList<QJsonObject> entries = recordedRequests("sync-mcp-requests.log");
    HttpRequestParser parser;
    parser.feed(wireFormatOf(entries));
    const QList<HttpRequest> requests = takeAll(parser);
    ASSERT_EQ(requests.size(), 6);

    EXPECT_EQ(requests.at(0).header("mcp-method"), QByteArray("server/discover"));
    EXPECT_EQ(requests.at(3).method, QByteArray("GET"));
    EXPECT_EQ(requests.at(3).header("Accept"), QByteArray("text/event-stream"));
    EXPECT_TRUE(requests.at(3).body.isEmpty());
    EXPECT_TRUE(requests.at(5).body.contains("claudecode/toolUseId"));
    for (const HttpRequest& request : requests)
    {
        EXPECT_EQ(request.path, QByteArray("/mcp"));
        EXPECT_EQ(request.header("Authorization"), QByteArray("Bearer test-token"));
    }
}
