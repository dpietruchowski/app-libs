#include "httprequestparser.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace chat::claude
{

namespace
{

    constexpr QByteArrayView kHeadTerminator { "\r\n\r\n" };
    constexpr QByteArrayView kLineBreak { "\r\n" };
    constexpr QByteArrayView kSupportedVersion { "HTTP/1.1" };
    constexpr std::string_view kTokenSymbols { "!#$%&'*+-.^_`|~" };

    bool isTokenChar(char c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || kTokenSymbols.find(c) != std::string_view::npos;
    }

    bool isToken(QByteArrayView text)
    {
        return !text.isEmpty() && std::all_of(text.begin(), text.end(), isTokenChar);
    }

    bool isControl(char c)
    {
        const auto byte = static_cast<unsigned char>(c);
        return byte < 0x20 || byte == 0x7f;
    }

    bool isRequestTarget(QByteArrayView text)
    {
        return text.size() > 0 && text.front() == '/'
            && std::none_of(text.begin(), text.end(),
                            [](char c) { return isControl(c) || c == ' '; });
    }

    bool isFieldValue(QByteArrayView text)
    {
        return std::none_of(text.begin(), text.end(),
                            [](char c) { return c != '\t' && isControl(c); });
    }

    bool isDecimal(QByteArrayView text)
    {
        return !text.isEmpty()
            && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
    }

    bool sameName(const QByteArray& name, QByteArrayView expected)
    {
        return QByteArrayView(name).compare(expected, Qt::CaseInsensitive) == 0;
    }

    QList<QByteArray> splitLines(const QByteArray& head)
    {
        QList<QByteArray> lines;
        qsizetype start = 0;
        while (true)
        {
            const qsizetype end = head.indexOf(kLineBreak, start);
            if (end < 0)
            {
                lines.append(head.mid(start));
                return lines;
            }
            lines.append(head.mid(start, end - start));
            start = end + kLineBreak.size();
        }
    }

    HttpParseError parseRequestLine(const QByteArray& line, HttpRequest& request)
    {
        const qsizetype firstSpace = line.indexOf(' ');
        const qsizetype lastSpace = line.lastIndexOf(' ');
        if (firstSpace < 0 || firstSpace == lastSpace)
            return HttpParseError::MalformedRequestLine;

        request.method = line.left(firstSpace);
        request.path = line.mid(firstSpace + 1, lastSpace - firstSpace - 1);
        if (!isToken(request.method) || !isRequestTarget(request.path))
            return HttpParseError::MalformedRequestLine;

        if (QByteArrayView(line).sliced(lastSpace + 1) != kSupportedVersion)
            return HttpParseError::UnsupportedVersion;
        return HttpParseError::None;
    }

    HttpParseError parseHeaderLine(const QByteArray& line, HttpHeader& header)
    {
        const qsizetype colon = line.indexOf(':');
        if (colon < 0)
            return HttpParseError::MalformedHeader;

        header.name = line.left(colon);
        const QByteArray rawValue = line.mid(colon + 1);
        if (!isToken(header.name) || !isFieldValue(rawValue))
            return HttpParseError::MalformedHeader;

        header.value = rawValue.trimmed();
        return HttpParseError::None;
    }

    HttpParseError readBodyLength(const HttpRequest& request, qsizetype maxBodyBytes,
                                  qlonglong& length)
    {
        if (request.header("Transfer-Encoding"))
            return HttpParseError::UnsupportedTransferEncoding;

        length = 0;
        bool seen = false;
        for (const HttpHeader& header : request.headers)
        {
            if (!sameName(header.name, "Content-Length"))
                continue;

            bool valid = isDecimal(header.value);
            const qlonglong value = valid ? header.value.toLongLong(&valid) : 0;
            if (!valid || (seen && value != length))
                return HttpParseError::InvalidContentLength;
            length = value;
            seen = true;
        }
        return length > maxBodyBytes ? HttpParseError::BodyTooLarge : HttpParseError::None;
    }

}

std::optional<QByteArray> HttpRequest::header(QByteArrayView name) const
{
    const auto found = std::find_if(headers.begin(), headers.end(), [&name](const HttpHeader& h)
                                    { return sameName(h.name, name); });
    if (found == headers.end())
        return std::nullopt;
    return found->value;
}

HttpRequestParser::HttpRequestParser(HttpParserLimits limits)
    : m_limits(limits)
{
}

void HttpRequestParser::feed(const QByteArray& bytes)
{
    if (!failed())
        m_buffer.append(bytes);
}

std::optional<HttpRequest> HttpRequestParser::takeRequest()
{
    if (failed())
        return std::nullopt;
    if (!m_pending && !readHead())
        return std::nullopt;

    const qsizetype total = m_pending->headLength + m_pending->bodyLength;
    if (m_buffer.size() < total)
        return std::nullopt;

    HttpRequest request = std::move(m_pending->request);
    request.body = m_buffer.mid(m_pending->headLength, m_pending->bodyLength);
    m_buffer.remove(0, total);
    m_pending.reset();
    return request;
}

HttpParseError HttpRequestParser::error() const { return m_error; }

bool HttpRequestParser::failed() const { return m_error != HttpParseError::None; }

bool HttpRequestParser::readHead()
{
    const qsizetype terminator = m_buffer.indexOf(kHeadTerminator);
    if (terminator < 0)
        return m_buffer.size() > m_limits.maxHeadBytes ? fail(HttpParseError::HeadersTooLarge)
                                                       : false;

    PendingRequest pending;
    pending.headLength = terminator + kHeadTerminator.size();
    if (pending.headLength > m_limits.maxHeadBytes)
        return fail(HttpParseError::HeadersTooLarge);

    const QList<QByteArray> lines = splitLines(m_buffer.left(terminator));
    if (const HttpParseError error = parseRequestLine(lines.first(), pending.request);
        error != HttpParseError::None)
        return fail(error);

    for (qsizetype i = 1; i < lines.size(); ++i)
    {
        HttpHeader header;
        if (const HttpParseError error = parseHeaderLine(lines.at(i), header);
            error != HttpParseError::None)
            return fail(error);
        pending.request.headers.append(std::move(header));
    }

    qlonglong bodyLength = 0;
    if (const HttpParseError error
        = readBodyLength(pending.request, m_limits.maxBodyBytes, bodyLength);
        error != HttpParseError::None)
        return fail(error);

    pending.bodyLength = static_cast<qsizetype>(bodyLength);
    m_pending = std::move(pending);
    return true;
}

bool HttpRequestParser::fail(HttpParseError error)
{
    m_error = error;
    m_buffer.clear();
    return false;
}

}
