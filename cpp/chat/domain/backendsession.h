#pragma once

#include <QString>

namespace chat
{

struct BackendSession final
{
    QString id;
    int start { 0 };

    bool operator==(const BackendSession& other) const = default;
};

}
