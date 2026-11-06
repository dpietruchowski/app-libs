#pragma once

#include <QByteArray>
#include <QtGlobal>

class EnvironmentOverride
{
public:
    EnvironmentOverride(const char* name, const QByteArray& value)
        : m_name(name)
        , m_hadValue(qEnvironmentVariableIsSet(name))
        , m_previous(qgetenv(name))
    {
        qputenv(name, value);
    }

    ~EnvironmentOverride()
    {
        if (m_hadValue)
            qputenv(m_name, m_previous);
        else
            qunsetenv(m_name);
    }

private:
    const char* m_name;
    bool m_hadValue;
    QByteArray m_previous;
};
