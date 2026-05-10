#include "fillintext.h"
#include "fillintextbuilder.h"

FillInText::FillInText(QObject* parent)
    : QObject(parent)
{
}

QString FillInText::partial(const QString& sentence, const QString& expression,
                            const QString& userAttempt) const
{
    FillInTextBuilder builder(QStringList { expression }, sentence);
    if (!userAttempt.isEmpty())
        builder.setAttempt(0, userAttempt);
    return builder.buildPartialText();
}

QString FillInText::preview(const QString& sentence, const QString& expression,
                            const QString& previewText) const
{
    FillInTextBuilder builder(QStringList { expression }, sentence);
    return builder.buildPreviewText(previewText);
}

QString FillInText::highlighted(const QString& sentence, const QString& expression) const
{
    FillInTextBuilder builder(QStringList { expression }, sentence);
    return builder.buildHighlightedText();
}
