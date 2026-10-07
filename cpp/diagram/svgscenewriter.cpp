#include "svgscenewriter.h"

#include <QLineF>
#include <QStringList>

#include <cmath>

namespace diagram
{
namespace
{
    QString num(qreal value)
    {
        QString text = QString::number(value, 'f', 2);
        if (text.contains(QLatin1Char('.')))
        {
            while (text.endsWith(QLatin1Char('0')))
                text.chop(1);
            if (text.endsWith(QLatin1Char('.')))
                text.chop(1);
        }
        if (text == QStringLiteral("-0"))
            text = QStringLiteral("0");
        return text;
    }

    bool isXmlCharacter(QChar character)
    {
        const char16_t code = character.unicode();
        if (code < 0x20)
            return code == 0x09 || code == 0x0A || code == 0x0D;
        return code != 0xFFFE && code != 0xFFFF;
    }

    QString escaped(const QString& text)
    {
        QString clean;
        clean.reserve(text.size());
        for (const QChar character : text)
        {
            if (isXmlCharacter(character))
                clean += character;
        }
        QString result = clean.toHtmlEscaped();
        result.replace(QLatin1Char('\''), QStringLiteral("&apos;"));
        return result;
    }

    QString pointList(const QList<QPointF>& points)
    {
        QStringList parts;
        for (const QPointF& point : points)
            parts.append(num(point.x()) + QLatin1Char(',') + num(point.y()));
        return parts.join(QLatin1Char(' '));
    }

    const QString kDash = QStringLiteral(" stroke-dasharray=\"4 3\"");

    class SvgBuilder
    {
    public:
        SvgBuilder(const Scene& scene, const DiagramPalette& palette)
            : m_scene(scene)
            , m_palette(palette)
        {
        }

        QString build(qreal pixelRatio)
        {
            const qreal width = m_scene.size.width();
            const qreal height = m_scene.size.height();
            m_out += QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.2\" "
                                    "baseProfile=\"tiny\" width=\"%1\" height=\"%2\" "
                                    "viewBox=\"0 0 %3 %4\">\n")
                         .arg(num(width * pixelRatio), num(height * pixelRatio), num(width),
                              num(height));
            m_out += QStringLiteral("<rect x=\"0\" y=\"0\" width=\"%1\" height=\"%2\"%3/>\n")
                         .arg(num(width), num(height), fill(ColorRole::Background));
            m_out += QStringLiteral("<g font-family=\"%1\" font-size=\"%2\" stroke-width=\"1\">\n")
                         .arg(escaped(m_scene.fontFamily), num(m_scene.fontPixelSize));
            for (const SceneItem& item : m_scene.items)
                std::visit([this](const auto& typed) { add(typed); }, item);
            m_out += QStringLiteral("</g>\n</svg>\n");
            return m_out;
        }

    private:
        QString colorAttributes(const QString& name, ColorRole role) const
        {
            const QColor color = m_palette.color(role);
            QString attributes = QStringLiteral(" %1=\"%2\"").arg(name, color.name(QColor::HexRgb));
            if (color.alpha() < 255)
                attributes += QStringLiteral(" %1-opacity=\"%2\"").arg(name, num(color.alphaF()));
            return attributes;
        }

        QString fill(ColorRole role) const { return colorAttributes(QStringLiteral("fill"), role); }

        QString stroke(ColorRole role) const
        {
            return colorAttributes(QStringLiteral("stroke"), role);
        }

        QString paint(const std::optional<ColorRole>& fillRole,
                      const std::optional<ColorRole>& strokeRole, bool dashed) const
        {
            QString attributes = fillRole ? fill(*fillRole) : QStringLiteral(" fill=\"none\"");
            if (strokeRole)
                attributes += stroke(*strokeRole);
            if (dashed)
                attributes += kDash;
            return attributes;
        }

        void add(const SceneRect& rect)
        {
            const QRectF& r = rect.rect;
            QString radius;
            if (rect.radius > 0)
                radius = QStringLiteral(" rx=\"%1\" ry=\"%1\"").arg(num(rect.radius));
            m_out += QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\"%5%6/>\n")
                         .arg(num(r.x()), num(r.y()), num(r.width()), num(r.height()), radius,
                              paint(rect.fill, rect.stroke, rect.dashed));
        }

        void add(const SceneLine& line)
        {
            if (line.points.size() < 2)
                return;
            const QString paintAttributes = paint(std::nullopt, line.color, line.dashed);
            if (line.points.size() == 2)
            {
                const QPointF& a = line.points.first();
                const QPointF& b = line.points.last();
                m_out += QStringLiteral("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%4\"%5/>\n")
                             .arg(num(a.x()), num(a.y()), num(b.x()), num(b.y()), paintAttributes);
            }
            else
            {
                m_out += QStringLiteral("<polyline points=\"%1\"%2/>\n")
                             .arg(pointList(line.points), paintAttributes);
            }
            addHead(line);
        }

        void addHead(const SceneLine& line)
        {
            if (line.head == ArrowHead::None)
                return;
            const QLineF last(line.points.at(line.points.size() - 2), line.points.last());
            if (last.length() <= 0)
                return;
            const qreal size = m_scene.arrowHeadSize;
            const QPointF direction = (last.p2() - last.p1()) / last.length();
            const QPointF normal(-direction.y(), direction.x());
            const QPointF tip = last.p2();
            const QPointF base = tip - direction * size;
            const QList<QPointF> wings { base + normal * (size * 0.45), tip,
                                         base - normal * (size * 0.45) };
            if (line.head == ArrowHead::Filled)
            {
                m_out += QStringLiteral("<polygon points=\"%1\"%2/>\n")
                             .arg(pointList(wings), fill(line.color));
                return;
            }
            m_out += QStringLiteral("<polyline points=\"%1\" fill=\"none\"%2/>\n")
                         .arg(pointList(wings), stroke(line.color));
        }

        void add(const SceneText& text)
        {
            QString anchor = QStringLiteral("middle");
            qreal x = text.bounds.center().x();
            if (text.align & Qt::AlignLeft)
            {
                anchor = QStringLiteral("start");
                x = text.bounds.left();
            }
            else if (text.align & Qt::AlignRight)
            {
                anchor = QStringLiteral("end");
                x = text.bounds.right();
            }
            for (qsizetype index = 0; index < text.lines.size(); ++index)
            {
                const qreal y
                    = text.bounds.top() + m_scene.ascent + qreal(index) * m_scene.lineHeight;
                m_out += QStringLiteral("<text x=\"%1\" y=\"%2\" text-anchor=\"%3\"%4>%5</text>\n")
                             .arg(num(x), num(y), anchor, fill(text.color),
                                  escaped(text.lines.at(index)));
            }
        }

        const Scene& m_scene;
        const DiagramPalette& m_palette;
        QString m_out;
    };
}

QString SvgSceneWriter::write(const Scene& scene, const DiagramPalette& palette, qreal pixelRatio)
{
    return SvgBuilder(scene, palette).build(pixelRatio);
}
}
