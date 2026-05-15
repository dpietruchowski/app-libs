#include "fillincontent.h"
#include "fillintextbuilder.h"

FillInContent::FillInContent(QObject* parent)
    : QObject(parent)
{
}

QString FillInContent::content() const
{
    QString sentence = buildSentence();
    QString header = buildHeader();

    if (!header.isEmpty())
        return sentence.isEmpty() ? header : header + sentence;
    return sentence.isEmpty() ? QStringLiteral("_________") : sentence;
}

QString FillInContent::buildSentence() const
{
    if (!m_showSentence)
        return m_preview;
    if (m_entry.sentence.isEmpty())
        return {};

    FillInTextBuilder builder(QStringList { m_entry.blankedText }, m_entry.sentence);
    if (!m_preview.isEmpty())
        return builder.buildPreviewText(m_preview);
    if (!m_interactive)
        return builder.buildHighlightedText();

    if (!m_userAnswer.isEmpty())
        builder.setAttempt(0, m_userAnswer);
    return builder.buildPartialText();
}

QString FillInContent::buildHeader() const
{
    if (!m_showExpression || m_entry.expression.isEmpty())
        return {};
    return "<p><b>" + m_entry.expression + "</b></p>";
}
