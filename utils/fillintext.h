#pragma once

#include <QObject>
#include <QString>

class FillInText : public QObject
{
    Q_OBJECT

public:
    explicit FillInText(QObject* parent = nullptr);

    Q_INVOKABLE QString partial(const QString& sentence, const QString& expression,
                                const QString& userAttempt = QString()) const;
    Q_INVOKABLE QString preview(const QString& sentence, const QString& expression,
                                const QString& previewText) const;
    Q_INVOKABLE QString highlighted(const QString& sentence, const QString& expression) const;
};
