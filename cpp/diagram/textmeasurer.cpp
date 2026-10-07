#include "textmeasurer.h"

namespace diagram
{
FontTextMeasurer::FontTextMeasurer(const QFont& font)
    : m_metrics(font)
{
}

qreal FontTextMeasurer::width(const QString& text) const
{
    return m_metrics.horizontalAdvance(text);
}

qreal FontTextMeasurer::lineHeight() const { return m_metrics.lineSpacing(); }

qreal FontTextMeasurer::ascent() const { return m_metrics.ascent(); }
}
