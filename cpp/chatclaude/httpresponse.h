#pragma once

#include <QByteArray>
#include <QList>

#include "chatclaude/httprequestparser.h"

namespace chat::claude
{

struct HttpResponse
{
    int status { 200 };
    QList<HttpHeader> headers;
    QByteArray body;

    QByteArray toBytes() const;
};

}
