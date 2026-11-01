#include "chatclaude/httpresponse.h"

#include <algorithm>
#include <iterator>

namespace chat::claude
{

namespace
{

    struct StatusLine
    {
        int status;
        QByteArrayView reason;
    };

    constexpr StatusLine kStatusLines[] = {
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

    QByteArrayView reasonPhrase(int status)
    {
        const auto* found
            = std::find_if(std::begin(kStatusLines), std::end(kStatusLines),
                           [status](const StatusLine& line) { return line.status == status; });
        return found == std::end(kStatusLines) ? QByteArrayView() : found->reason;
    }

}

QByteArray HttpResponse::toBytes() const
{
    QByteArray bytes = "HTTP/1.1 " + QByteArray::number(status) + ' '
        + reasonPhrase(status).toByteArray() + "\r\n";
    for (const HttpHeader& header : headers)
        bytes += header.name + ": " + header.value + "\r\n";
    bytes += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
    bytes += body;
    return bytes;
}

}
