#include <gtest/gtest.h>

#include "diagram/classlayout.h"
#include "diagram/classparser.h"
#include "faketextmeasurer.h"

#include <algorithm>

using namespace diagram;

namespace
{
constexpr qreal kEpsilon = 0.01;

const QString kKernel = QStringLiteral("title Process memory\n"
                                       "struct task_struct {\n"
                                       "  pid : int\n"
                                       "  mm : mm_struct*\n"
                                       "  files : files_struct*\n"
                                       "}\n"
                                       "struct mm_struct {\n"
                                       "  pgd : pgd_t*\n"
                                       "  mmap : vm_area_struct*\n"
                                       "}\n"
                                       "struct vm_area_struct {\n"
                                       "  vm_start : unsigned long\n"
                                       "  --\n"
                                       "  vm_next : vm_area_struct*\n"
                                       "}\n"
                                       "struct files_struct\n"
                                       "task_struct \"1\" --> \"0..1\" mm_struct : mm\n"
                                       "task_struct --> files_struct : files\n"
                                       "mm_struct \"1\" *-- \"many\" vm_area_struct : mmap\n"
                                       "vm_area_struct --> vm_area_struct : vm_next\n");

const QString kInheritance = QStringLiteral("abstract class Shape {\n"
                                            "  - id : int\n"
                                            "  + area() : double\n"
                                            "}\n"
                                            "interface Drawable {\n"
                                            "  + draw()\n"
                                            "}\n"
                                            "class Circle {\n"
                                            "  - radius : double\n"
                                            "}\n"
                                            "class Square {\n"
                                            "  - side : double\n"
                                            "}\n"
                                            "Shape <|-- Circle\n"
                                            "Square --|> Shape\n"
                                            "Drawable <|.. Circle : implements\n"
                                            "Shape \"1\" o-- \"0..*\" Drawable : draws\n");

const QString kCycle = QStringLiteral("class A\n"
                                      "class B\n"
                                      "class C\n"
                                      "class D\n"
                                      "A --> B : next\n"
                                      "B --> C : next\n"
                                      "C --> A : back\n"
                                      "A \"1\" *-- \"2\" C : owns\n"
                                      "D ..> A : uses\n"
                                      "A -- D\n");

const QString kOrders = QStringLiteral("title Orders\n"
                                       "enum Status {\n"
                                       "  NEW\n"
                                       "  PAID\n"
                                       "}\n"
                                       "class \"Order line with a very long name\" as Line {\n"
                                       "  quantity : int\n"
                                       "}\n"
                                       "class Order {\n"
                                       "  + total() : Money\n"
                                       "}\n"
                                       "Order \"1\" o-- \"*\" Line\n"
                                       "Order --> Status\n"
                                       "note left of Order : the root of the aggregate\n");

const QList<qreal> kWidths { 280, 320, 336, 360, 400, 480, 640 };

ClassDiagram parsed(const QString& text)
{
    const auto result = ClassParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() ? result.value() : ClassDiagram {};
}

Scene laidOut(const QString& text, qreal width)
{
    static const FakeTextMeasurer measurer;
    return ClassLayout::layout(parsed(text), DiagramStyle {}, width, measurer);
}

template <typename T> QList<T> itemsOf(const Scene& scene)
{
    QList<T> found;
    for (const SceneItem& item : scene.items)
    {
        if (const auto* typed = std::get_if<T>(&item))
            found.append(*typed);
    }
    return found;
}

QRectF boundsOf(const QList<QPointF>& points)
{
    qreal left = points.first().x();
    qreal right = left;
    qreal top = points.first().y();
    qreal bottom = top;
    for (const QPointF& point : points)
    {
        left = std::min(left, point.x());
        right = std::max(right, point.x());
        top = std::min(top, point.y());
        bottom = std::max(bottom, point.y());
    }
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

bool inside(const QRectF& outer, const QRectF& inner)
{
    return inner.left() >= outer.left() - kEpsilon && inner.top() >= outer.top() - kEpsilon
        && inner.right() <= outer.right() + kEpsilon && inner.bottom() <= outer.bottom() + kEpsilon;
}

bool overlaps(const QRectF& a, const QRectF& b)
{
    const QRectF common = a.intersected(b);
    return common.width() > kEpsilon && common.height() > kEpsilon;
}

QList<QRectF> classBoxes(const Scene& scene)
{
    QList<QRectF> boxes;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == ColorRole::BoxFill && rect.stroke == ColorRole::BoxStroke)
            boxes.append(rect.rect);
    }
    return boxes;
}

QList<QRectF> noteBoxes(const Scene& scene)
{
    QList<QRectF> boxes;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == ColorRole::NoteFill)
            boxes.append(rect.rect);
    }
    return boxes;
}

QList<QRectF> labelHalos(const Scene& scene)
{
    QList<QRectF> halos;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == ColorRole::Background && !rect.stroke)
            halos.append(rect.rect);
    }
    return halos;
}

bool isArrow(const SceneLine& line)
{
    return line.color == ColorRole::Line && line.points.size() == 3;
}

QList<SceneLine> relationLines(const Scene& scene)
{
    QList<SceneLine> lines;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        if (line.color == ColorRole::Line && !isArrow(line))
            lines.append(line);
    }
    return lines;
}

QList<QPointF> markerTips(const Scene& scene)
{
    QList<QPointF> tips;
    for (const ScenePolygon& polygon : itemsOf<ScenePolygon>(scene))
        tips.append(polygon.points.first());
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        if (isArrow(line))
            tips.append(line.points.at(1));
    }
    return tips;
}

QList<QRectF> markerBounds(const Scene& scene)
{
    QList<QRectF> bounds;
    for (const ScenePolygon& polygon : itemsOf<ScenePolygon>(scene))
        bounds.append(boundsOf(polygon.points));
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        if (isArrow(line))
            bounds.append(boundsOf(line.points));
    }
    return bounds;
}

QRectF textBounds(const Scene& scene, const QString& shown)
{
    for (const SceneText& text : itemsOf<SceneText>(scene))
    {
        if (text.lines.join(QLatin1Char(' ')) == shown)
            return text.bounds;
    }
    ADD_FAILURE() << "no text " << shown.toStdString();
    return QRectF();
}

QRectF boxContaining(const Scene& scene, const QString& shown)
{
    const QRectF text = textBounds(scene, shown);
    for (const QRectF& box : classBoxes(scene))
    {
        if (box.contains(text))
            return box;
    }
    ADD_FAILURE() << "no box around " << shown.toStdString();
    return QRectF();
}

bool onBorder(const QPointF& point, const QRectF& box)
{
    const bool withinX = point.x() >= box.left() - kEpsilon && point.x() <= box.right() + kEpsilon;
    const bool withinY = point.y() >= box.top() - kEpsilon && point.y() <= box.bottom() + kEpsilon;
    const bool onVertical = std::abs(point.x() - box.left()) < kEpsilon
        || std::abs(point.x() - box.right()) < kEpsilon;
    const bool onHorizontal = std::abs(point.y() - box.top()) < kEpsilon
        || std::abs(point.y() - box.bottom()) < kEpsilon;
    return withinX && withinY && (onVertical || onHorizontal);
}

void expectInvariants(const Scene& scene, qreal availableWidth)
{
    const QRectF area(QPointF(0, 0), scene.size);
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
        EXPECT_TRUE(inside(area, rect.rect)) << availableWidth;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        for (const QPointF& point : line.points)
            EXPECT_TRUE(inside(area, QRectF(point, QSizeF(0, 0)))) << availableWidth;
    }
    for (const ScenePolygon& polygon : itemsOf<ScenePolygon>(scene))
    {
        for (const QPointF& point : polygon.points)
            EXPECT_TRUE(inside(area, QRectF(point, QSizeF(0, 0)))) << availableWidth;
    }
    for (const SceneText& text : itemsOf<SceneText>(scene))
        EXPECT_TRUE(inside(area, text.bounds))
            << availableWidth << " " << text.lines.join(QLatin1Char(' ')).toStdString();

    const QList<QRectF> boxes = classBoxes(scene);
    for (qsizetype i = 0; i < boxes.size(); ++i)
    {
        for (qsizetype j = i + 1; j < boxes.size(); ++j)
            EXPECT_FALSE(overlaps(boxes[i], boxes[j])) << availableWidth;
    }

    const QList<QRectF> halos = labelHalos(scene);
    const QList<QRectF> notes = noteBoxes(scene);
    const QList<QRectF> markers = markerBounds(scene);
    for (qsizetype i = 0; i < halos.size(); ++i)
    {
        for (const QRectF& box : boxes)
            EXPECT_FALSE(overlaps(halos[i], box)) << availableWidth;
        for (const QRectF& note : notes)
            EXPECT_FALSE(overlaps(halos[i], note)) << availableWidth;
        for (const QRectF& marker : markers)
            EXPECT_FALSE(overlaps(halos[i], marker)) << availableWidth;
        for (qsizetype j = i + 1; j < halos.size(); ++j)
            EXPECT_FALSE(overlaps(halos[i], halos[j])) << availableWidth;
    }
}

void expectNoRelationThroughABox(const Scene& scene, qreal availableWidth)
{
    const QList<QRectF> boxes = classBoxes(scene);
    for (const SceneLine& line : relationLines(scene))
    {
        for (qsizetype index = 1; index < line.points.size(); ++index)
        {
            const QRectF segment = boundsOf({ line.points.at(index - 1), line.points.at(index) });
            for (const QRectF& box : boxes)
                EXPECT_FALSE(
                    overlaps(segment.adjusted(-0.4, -0.4, 0.4, 0.4), box.adjusted(1, 1, -1, -1)))
                    << availableWidth;
        }
    }
}

void expectLabelsClearOfEveryLine(const Scene& scene, qreal availableWidth)
{
    for (const QRectF& halo : labelHalos(scene))
    {
        for (const SceneLine& line : itemsOf<SceneLine>(scene))
        {
            if (line.color != ColorRole::Line)
                continue;
            for (qsizetype index = 1; index < line.points.size(); ++index)
            {
                const QRectF segment
                    = boundsOf({ line.points.at(index - 1), line.points.at(index) });
                EXPECT_FALSE(overlaps(halo, segment.adjusted(-0.5, -0.5, 0.5, 0.5)))
                    << availableWidth;
            }
        }
    }
}

void expectEveryMarkerOnABorder(const Scene& scene, qreal availableWidth)
{
    const QList<QRectF> boxes = classBoxes(scene);
    for (const QPointF& tip : markerTips(scene))
    {
        const bool found = std::any_of(boxes.cbegin(), boxes.cend(),
                                       [&tip](const QRectF& box) { return onBorder(tip, box); });
        EXPECT_TRUE(found) << availableWidth << " " << tip.x() << "," << tip.y();
    }
}

qreal rightmostBox(const Scene& scene)
{
    qreal right = 0;
    for (const QRectF& box : classBoxes(scene))
        right = std::max(right, box.right());
    return right;
}
}

TEST(ClassLayoutTest, TheKernelExampleFitsAPhoneWidth)
{
    const Scene scene = laidOut(kKernel, 336);

    EXPECT_LE(scene.size.width(), 336);
    EXPECT_EQ(classBoxes(scene).size(), 4);
    expectInvariants(scene, 336);
    expectNoRelationThroughABox(scene, 336);
    EXPECT_EQ(scene.fontPixelSize, DiagramStyle {}.fontPixelSize);
}

TEST(ClassLayoutTest, KeepsTheInvariantsAtEveryWidth)
{
    for (const QString& text : { kKernel, kInheritance, kCycle, kOrders })
    {
        for (const qreal width : kWidths)
        {
            const Scene scene = laidOut(text, width);
            expectInvariants(scene, width);
            expectNoRelationThroughABox(scene, width);
            expectLabelsClearOfEveryLine(scene, width);
            expectEveryMarkerOnABorder(scene, width);
        }
    }
}

TEST(ClassLayoutTest, DrawsEveryMarkerExactlyOnTheBorderOfItsBox)
{
    const Scene scene = laidOut(kInheritance, 336);

    EXPECT_EQ(itemsOf<ScenePolygon>(scene).size(), 4);
    expectEveryMarkerOnABorder(scene, 336);
}

TEST(ClassLayoutTest, DrawsATriangleAtTheParentAndADiamondAtTheWhole)
{
    const Scene scene = laidOut(kInheritance, 336);

    int triangles = 0;
    int hollowDiamonds = 0;
    for (const ScenePolygon& polygon : itemsOf<ScenePolygon>(scene))
    {
        triangles += polygon.points.size() == 3;
        hollowDiamonds += polygon.points.size() == 4 && polygon.fill == ColorRole::Background;
    }
    EXPECT_EQ(triangles, 3);
    EXPECT_EQ(hollowDiamonds, 1);
}

TEST(ClassLayoutTest, DrawsACompositionDiamondFilled)
{
    const Scene scene = laidOut(kKernel, 336);

    const QList<ScenePolygon> polygons = itemsOf<ScenePolygon>(scene);
    ASSERT_EQ(polygons.size(), 1);
    EXPECT_EQ(polygons.first().fill, ColorRole::Line);
}

TEST(ClassLayoutTest, DrawsARealizationDashedAndAnInheritanceSolid)
{
    const Scene scene = laidOut(kInheritance, 336);

    int dashed = 0;
    int solid = 0;
    for (const SceneLine& line : relationLines(scene))
        (line.dashed ? dashed : solid) += 1;
    EXPECT_EQ(dashed, 1);
    EXPECT_EQ(solid, 3);
}

TEST(ClassLayoutTest, PlacesAParentAboveItsChildren)
{
    const Scene scene = laidOut(kInheritance, 336);

    const QRectF shape = boxContaining(scene, QStringLiteral("Shape"));
    const QRectF circle = boxContaining(scene, QStringLiteral("Circle"));
    const QRectF square = boxContaining(scene, QStringLiteral("Square"));
    const QRectF drawable = boxContaining(scene, QStringLiteral("Drawable"));
    EXPECT_LT(shape.bottom(), circle.top());
    EXPECT_LT(shape.bottom(), square.top());
    EXPECT_LT(drawable.bottom(), circle.top());
}

TEST(ClassLayoutTest, PlacesTheWholeAboveItsPartsWhateverTheOrderOfTheStatement)
{
    const Scene scene = laidOut(QStringLiteral("Part --o Whole\nPart2 --* Whole\n"), 336);

    const QRectF whole = boxContaining(scene, QStringLiteral("Whole"));
    EXPECT_LT(whole.bottom(), boxContaining(scene, QStringLiteral("Part")).top());
    EXPECT_LT(whole.bottom(), boxContaining(scene, QStringLiteral("Part2")).top());
}

TEST(ClassLayoutTest, PlacesTheDeclaringSideAboveWhenNothingMarksTheWhole)
{
    const Scene scene = laidOut(QStringLiteral("Second --> First\n"), 336);

    EXPECT_LT(boxContaining(scene, QStringLiteral("Second")).bottom(),
              boxContaining(scene, QStringLiteral("First")).top());
}

TEST(ClassLayoutTest, RoutesABackEdgeAroundTheBoxesOnTheRight)
{
    const Scene scene = laidOut(kCycle, 336);

    expectNoRelationThroughABox(scene, 336);
    const qreal boxesRight = rightmostBox(scene);
    int outside = 0;
    for (const SceneLine& line : relationLines(scene))
    {
        if (line.points.size() == 4
            && std::abs(line.points.at(1).x() - line.points.at(2).x()) < kEpsilon
            && line.points.at(1).x() > boxesRight)
            ++outside;
    }
    EXPECT_GE(outside, 3);
}

TEST(ClassLayoutTest, KeepsNestedBackEdgesApartSoTheyDoNotCross)
{
    const Scene scene = laidOut(kCycle, 336);

    QList<SceneLine> lanes;
    for (const SceneLine& line : relationLines(scene))
    {
        if (line.points.size() == 4 && line.points.at(1).x() > rightmostBox(scene))
            lanes.append(line);
    }
    ASSERT_GE(lanes.size(), 2);
    for (qsizetype i = 0; i < lanes.size(); ++i)
    {
        for (qsizetype j = i + 1; j < lanes.size(); ++j)
        {
            const bool apart
                = std::abs(lanes.at(i).points.at(1).x() - lanes.at(j).points.at(1).x()) > kEpsilon;
            EXPECT_TRUE(apart);
        }
    }
}

TEST(ClassLayoutTest, LoopsASelfRelationOnTheRightOfItsBox)
{
    const Scene scene = laidOut(kKernel, 336);

    const QRectF box = boxContaining(scene, QStringLiteral("vm_area_struct"));
    const QRectF label = textBounds(scene, QStringLiteral("vm_next"));
    EXPECT_GT(label.left(), box.right());
    bool looped = false;
    for (const SceneLine& line : relationLines(scene))
    {
        if (line.points.size() == 4 && std::abs(line.points.first().x() - box.right()) < kEpsilon
            && std::abs(line.points.last().x() - box.right()) < kEpsilon)
            looped = true;
    }
    EXPECT_TRUE(looped);
}

TEST(ClassLayoutTest, PlacesAMultiplicityBesideTheEndItBelongsTo)
{
    const Scene scene = laidOut(kKernel, 336);

    const QRectF task = boxContaining(scene, QStringLiteral("task_struct"));
    const QRectF mm = boxContaining(scene, QStringLiteral("mm_struct"));
    const QRectF upper = textBounds(scene, QStringLiteral("1"));
    const QRectF lower = textBounds(scene, QStringLiteral("0..1"));
    EXPECT_GE(upper.top(), task.bottom());
    EXPECT_LT(upper.top() - task.bottom(), 40);
    EXPECT_LE(lower.bottom(), mm.top());
    EXPECT_LT(mm.top() - lower.bottom(), 40);
}

TEST(ClassLayoutTest, KeepsEveryLabelClearOfEveryLineAndBox)
{
    for (const QString& text : { kKernel, kInheritance, kCycle, kOrders })
    {
        const Scene scene = laidOut(text, 336);
        expectLabelsClearOfEveryLine(scene, 336);
        expectInvariants(scene, 336);
    }
}

TEST(ClassLayoutTest, ShowsTheKindAboveTheNameOfNonClasses)
{
    const Scene scene = laidOut(kInheritance, 336);

    EXPECT_LT(
        textBounds(scene, QStringLiteral("«") + QStringLiteral("abstract") + QStringLiteral("»"))
            .bottom(),
        textBounds(scene, QStringLiteral("Shape")).top() + kEpsilon);
    EXPECT_FALSE(textBounds(scene, QStringLiteral("«interface»")).isNull());
    for (const SceneText& text : itemsOf<SceneText>(scene))
        EXPECT_FALSE(text.lines.join(QLatin1Char(' ')) == QStringLiteral("«class»"));
}

TEST(ClassLayoutTest, SeparatesCompartmentsWithDividers)
{
    const Scene scene = laidOut(kKernel, 336);

    int dividers = 0;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
        dividers += line.color == ColorRole::Divider;
    EXPECT_EQ(dividers, 4);
}

TEST(ClassLayoutTest, AlignsMembersLeftInsideTheirBox)
{
    const Scene scene = laidOut(kKernel, 336);

    const QRectF box = boxContaining(scene, QStringLiteral("pid : int"));
    EXPECT_NEAR(textBounds(scene, QStringLiteral("pid : int")).left(),
                box.left() + DiagramStyle {}.padding, kEpsilon);
    EXPECT_NEAR(textBounds(scene, QStringLiteral("mm : mm_struct*")).left(),
                box.left() + DiagramStyle {}.padding, kEpsilon);
}

TEST(ClassLayoutTest, WrapsALongMemberInsideItsBox)
{
    const Scene scene = laidOut(QStringLiteral("class A {\n"
                                               "  + a_very_long_method_name(first argument, "
                                               "second argument, third argument) : result\n"
                                               "}\n"),
                                240);

    expectInvariants(scene, 240);
    EXPECT_LE(scene.size.width(), 240);
    const QRectF box = classBoxes(scene).first();
    for (const SceneText& text : itemsOf<SceneText>(scene))
        EXPECT_TRUE(box.contains(text.bounds));
}

TEST(ClassLayoutTest, PlacesNotesBesideTheirClassAndATitleOnTop)
{
    const Scene scene = laidOut(kOrders, 336);

    const QList<QRectF> notes = noteBoxes(scene);
    ASSERT_EQ(notes.size(), 1);
    const QRectF order = boxContaining(scene, QStringLiteral("Order"));
    EXPECT_LE(notes.first().right(), order.left());
    EXPECT_TRUE(notes.first().bottom() > order.top() && notes.first().top() < order.bottom());
    EXPECT_LT(textBounds(scene, QStringLiteral("Orders")).bottom(), order.top());
}

TEST(ClassLayoutTest, DrawsAnArrowEndAsAnOpenHeadOnTheBorder)
{
    const Scene scene = laidOut(QStringLiteral("A --> B\n"), 336);

    int arrows = 0;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
        arrows += isArrow(line);
    EXPECT_EQ(arrows, 1);
    EXPECT_EQ(relationLines(scene).size(), 1);
    expectEveryMarkerOnABorder(scene, 336);
}

TEST(ClassLayoutTest, LaysOutAClassWithoutRelations)
{
    const Scene scene = laidOut(QStringLiteral("class Lonely\n"), 336);

    EXPECT_EQ(classBoxes(scene).size(), 1);
    EXPECT_TRUE(relationLines(scene).isEmpty());
}
