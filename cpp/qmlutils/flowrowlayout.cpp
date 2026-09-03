#include "flowrowlayout.h"

#include <algorithm>
#include <cmath>

void FlowRowLayout::setLineFonts(const QList<QFont>& fonts)
{
    if (m_lineFonts == fonts)
        return;

    m_lineFonts = fonts;
    m_textWidthCache.clear();
    m_lineMetrics.clear();
    m_lineMetrics.reserve(static_cast<size_t>(fonts.size()));
    for (const QFont& font : fonts)
    {
        m_lineMetrics.emplace_back(font);
    }

    measure();
    pack();
}

const QList<QFont>& FlowRowLayout::lineFonts() const { return m_lineFonts; }

void FlowRowLayout::setSpacing(qreal spacing)
{
    if (qFuzzyCompare(m_spacing, spacing))
        return;

    m_spacing = spacing;
    pack();
}

qreal FlowRowLayout::spacing() const { return m_spacing; }

void FlowRowLayout::setRowHeight(qreal rowHeight)
{
    if (qFuzzyCompare(m_rowHeight, rowHeight))
        return;

    m_rowHeight = rowHeight;
    pack();
}

qreal FlowRowLayout::rowHeight() const { return m_rowHeight; }

void FlowRowLayout::setContainerWidth(qreal containerWidth)
{
    if (qFuzzyCompare(m_containerWidth, containerWidth))
        return;

    m_containerWidth = containerWidth;
    pack();
}

qreal FlowRowLayout::containerWidth() const { return m_containerWidth; }

void FlowRowLayout::setItems(const QList<Item>& items)
{
    m_items = items;
    measure();
    pack();
}

void FlowRowLayout::setItemWidths(const QList<qreal>& widths)
{
    m_items.clear();
    m_naturalWidths = widths;
    pack();
}

void FlowRowLayout::clear()
{
    m_items.clear();
    m_naturalWidths.clear();
    m_widths.clear();
    m_rows.clear();
}

int FlowRowLayout::itemCount() const { return static_cast<int>(m_widths.size()); }

qreal FlowRowLayout::itemWidth(int index) const
{
    if (index < 0 || index >= m_widths.size())
        return 0;
    return m_widths[index];
}

qreal FlowRowLayout::itemNaturalWidth(int index) const
{
    if (index < 0 || index >= m_naturalWidths.size())
        return 0;
    return m_naturalWidths[index];
}

qreal FlowRowLayout::itemX(int index) const
{
    const int rowIndex = rowOfItem(index);
    if (rowIndex < 0)
        return 0;

    qreal x = 0;
    for (int i = m_rows[rowIndex].firstIndex; i < index; ++i)
    {
        x += m_widths[i] + m_spacing;
    }
    return x;
}

int FlowRowLayout::rowOfItem(int index) const
{
    if (index < 0 || index >= m_widths.size() || m_rows.isEmpty())
        return -1;

    const auto it
        = std::upper_bound(m_rows.begin(), m_rows.end(), index,
                           [](int value, const Row& row) { return value < row.firstIndex; });
    return static_cast<int>(std::distance(m_rows.begin(), it)) - 1;
}

int FlowRowLayout::rowCount() const { return static_cast<int>(m_rows.size()); }

FlowRowLayout::Row FlowRowLayout::row(int index) const
{
    if (index < 0 || index >= m_rows.size())
        return Row {};
    return m_rows[index];
}

qreal FlowRowLayout::contentHeight() const
{
    if (m_rows.isEmpty())
        return 0;
    return m_rows.size() * m_rowHeight + (m_rows.size() - 1) * m_spacing;
}

void FlowRowLayout::measure()
{
    m_naturalWidths.resize(m_items.size());
    if (m_items.isEmpty())
        return;

    std::vector<QFontMetricsF> metrics = m_lineMetrics;
    if (metrics.empty())
    {
        metrics.emplace_back(QFont());
    }

    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        const Item& item = m_items[i];
        const QString key = item.lines.join(QChar(0x1f));

        const auto cached = m_textWidthCache.constFind(key);
        if (cached != m_textWidthCache.constEnd())
        {
            m_naturalWidths[i] = *cached + item.extraWidth;
            continue;
        }

        qreal widest = 0;
        for (qsizetype line = 0; line < item.lines.size(); ++line)
        {
            const size_t metricsIndex = std::min(static_cast<size_t>(line), metrics.size() - 1);
            widest = std::max(widest, metrics[metricsIndex].horizontalAdvance(item.lines[line]));
        }

        const qreal textWidth = std::ceil(widest);
        m_textWidthCache.insert(key, textWidth);
        m_naturalWidths[i] = textWidth + item.extraWidth;
    }
}

void FlowRowLayout::pack()
{
    m_rows.clear();
    m_widths.resize(m_naturalWidths.size());
    if (m_naturalWidths.isEmpty())
        return;

    const bool wraps = m_containerWidth > 0;

    qreal x = 0;
    Row current;

    for (qsizetype i = 0; i < m_naturalWidths.size(); ++i)
    {
        const qreal width
            = wraps ? std::min(m_naturalWidths[i], m_containerWidth) : m_naturalWidths[i];
        m_widths[i] = width;

        if (wraps && x > 0 && x + width > m_containerWidth)
        {
            current.width = x - m_spacing;
            m_rows.append(current);
            current = Row { static_cast<int>(i), 0, 0, 0 };
            x = 0;
        }

        ++current.count;
        x += width + m_spacing;
    }

    current.width = x - m_spacing;
    m_rows.append(current);

    for (qsizetype i = 0; i < m_rows.size(); ++i)
    {
        m_rows[i].y = i * (m_rowHeight + m_spacing);
    }
}
