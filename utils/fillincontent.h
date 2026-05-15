#pragma once

#include "expressionentry.h"
#include <QObject>
#include <QString>

class FillInContent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ExpressionEntry entry MEMBER m_entry NOTIFY changed)
    Q_PROPERTY(QString preview MEMBER m_preview NOTIFY changed)
    Q_PROPERTY(QString userAnswer MEMBER m_userAnswer NOTIFY changed)
    Q_PROPERTY(bool showSentence MEMBER m_showSentence NOTIFY changed)
    Q_PROPERTY(bool showExpression MEMBER m_showExpression NOTIFY changed)
    Q_PROPERTY(bool interactive MEMBER m_interactive NOTIFY changed)
    Q_PROPERTY(QString content READ content NOTIFY changed)

public:
    explicit FillInContent(QObject* parent = nullptr);

    QString content() const;

signals:
    void changed();

private:
    QString buildSentence() const;
    QString buildHeader() const;

    ExpressionEntry m_entry;
    QString m_preview;
    QString m_userAnswer;
    bool m_showSentence = true;
    bool m_showExpression = true;
    bool m_interactive = false;
};
