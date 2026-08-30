#pragma once

#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <vector>

class FlowRowLayout final
{
public:
    struct Item
    {
        QStringList lines;
        qreal extraWidth = 0;
    };

    struct Row
    {
        int firstIndex = 0;
        int count = 0;
        qreal width = 0;
        qreal y = 0;
    };

    void setLineFonts(const QList<QFont>& fonts);
    const QList<QFont>& lineFonts() const;

    void setSpacing(qreal spacing);
    qreal spacing() const;

    void setRowHeight(qreal rowHeight);
    qreal rowHeight() const;

    void setContainerWidth(qreal containerWidth);
    qreal containerWidth() const;

    void setItems(const QList<Item>& items);
    void setItemWidths(const QList<qreal>& widths);
    void clear();

    int itemCount() const;
    qreal itemWidth(int index) const;
    qreal itemNaturalWidth(int index) const;
    qreal itemX(int index) const;
    int rowOfItem(int index) const;

    int rowCount() const;
    Row row(int index) const;
    qreal contentHeight() const;

private:
    void measure();
    void pack();

    QList<QFont> m_lineFonts;
    std::vector<QFontMetricsF> m_lineMetrics;
    QHash<QString, qreal> m_textWidthCache;
    QList<Item> m_items;
    QList<qreal> m_naturalWidths;
    QList<qreal> m_widths;
    QList<Row> m_rows;
    qreal m_spacing = 0;
    qreal m_rowHeight = 0;
    qreal m_containerWidth = 0;
};
