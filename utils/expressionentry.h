#pragma once

#include <QObject>
#include <QString>

struct ExpressionEntry
{
    Q_GADGET
    Q_PROPERTY(QString expression MEMBER expression)
    Q_PROPERTY(QString blankedText MEMBER blankedText)
    Q_PROPERTY(QString sentence MEMBER sentence)
    Q_PROPERTY(QString language MEMBER language)

public:
    QString expression;
    QString blankedText;
    QString sentence;
    QString language;

    Q_INVOKABLE QString targetText(bool showContext) const
    {
        return showContext ? blankedText : expression;
    }

    bool operator==(const ExpressionEntry& other) const = default;
};
