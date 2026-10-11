#pragma once

#include "diagram/diagramstyle.h"
#include "diagram/scene.h"
#include "diagram/textmeasurer.h"

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace diagram::layouting
{
struct WrappedText
{
    QStringList lines;
    qreal width = 0;
    qreal height = 0;
};

qreal wholePixels(qreal value);

QList<qreal> fairWidths(const QList<qreal>& minimal, qreal usable);

class TextWrapper
{
public:
    TextWrapper(const DiagramStyle& style, const TextMeasurer& measurer);

    qreal measured(const QString& text) const;
    qreal widestWord(const QString& text) const;
    qreal minimumWrapWidth() const;
    WrappedText wrap(const QString& text, qreal maxWidth) const;

private:
    void wrapParagraph(const QString& paragraph, qreal limit, QStringList& lines) const;
    QString breakLongWord(const QString& word, qreal limit, QStringList& lines) const;

    const DiagramStyle& m_style;
    const TextMeasurer& m_measurer;
};

void appendShifted(QList<SceneItem>& target, const QList<SceneItem>& source, qreal dx, qreal dy);

QRectF placeFrameTab(QList<SceneItem>& items, const WrappedText& label, const DiagramStyle& style,
                     const QPointF& topLeft);
}
