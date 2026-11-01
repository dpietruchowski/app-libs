#pragma once

#include <optional>

#include <QByteArray>
#include <QList>

namespace chat::claude
{

struct HttpHeader
{
    QByteArray name;
    QByteArray value;
};

struct HttpRequest
{
    QByteArray method;
    QByteArray path;
    QList<HttpHeader> headers;
    QByteArray body;

    std::optional<QByteArray> header(QByteArrayView name) const;
};

enum class HttpParseError
{
    None,
    MalformedRequestLine,
    UnsupportedVersion,
    MalformedHeader,
    InvalidContentLength,
    UnsupportedTransferEncoding,
    HeadersTooLarge,
    BodyTooLarge,
};

struct HttpParserLimits
{
    qsizetype maxHeadBytes = 16 * 1024;
    qsizetype maxBodyBytes = 1024 * 1024;
};

class HttpRequestParser
{
public:
    explicit HttpRequestParser(HttpParserLimits limits = {});

    void feed(const QByteArray& bytes);
    std::optional<HttpRequest> takeRequest();

    HttpParseError error() const;
    bool failed() const;

private:
    struct PendingRequest
    {
        HttpRequest request;
        qsizetype headLength = 0;
        qsizetype bodyLength = 0;
    };

    bool readHead();
    bool fail(HttpParseError error);

    HttpParserLimits m_limits;
    QByteArray m_buffer;
    std::optional<PendingRequest> m_pending;
    HttpParseError m_error = HttpParseError::None;
};

}
