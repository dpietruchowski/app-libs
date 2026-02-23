#pragma once

#include <QJniObject>
#include <QString>

namespace android
{

class Intent final
{
public:
    enum class Action
    {
        View,
        Send,
        Dial,
        Edit,
        Pick,
        Search,
        WebSearch,
    };

    enum class Extra
    {
        Text,
        Subject,
        Email,
        Stream,
        Title,
    };

    enum class MimeType
    {
        TextPlain,
        TextHtml,
        ImageJpeg,
        ImagePng,
        ImageAny,
        VideoAny,
        AudioAny,
        ApplicationPdf,
    };

    explicit Intent();
    explicit Intent(Action action);
    explicit Intent(const QString& customAction);

    Intent& setAction(Action action);
    Intent& setData(const QString& uri);
    Intent& setType(MimeType mimeType);
    Intent& setClassName(const QString& packageName, const QString& className);
    Intent& putExtra(Extra key, const QString& value);
    Intent& putExtra(const QString& key, const QString& value);
    Intent& putExtra(const QString& key, int value);

    static Intent createChooser(const Intent& target, const QString& title);

    void start() const;
    QJniObject jniObject() const;

private:
    explicit Intent(QJniObject jniObject);

    QJniObject m_intent;
};

}  // namespace android
