#pragma once

#include <QFont>
#include <QFontMetricsF>
#include <QString>

namespace diagram
{
class TextMeasurer
{
public:
    virtual ~TextMeasurer() = default;

    virtual qreal width(const QString& text) const = 0;
    virtual qreal lineHeight() const = 0;
    virtual qreal ascent() const = 0;
};

class FontTextMeasurer final : public TextMeasurer
{
public:
    explicit FontTextMeasurer(const QFont& font);

    qreal width(const QString& text) const override;
    qreal lineHeight() const override;
    qreal ascent() const override;

private:
    QFontMetricsF m_metrics;
};
}
