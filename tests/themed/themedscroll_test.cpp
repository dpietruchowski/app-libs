#include <gtest/gtest.h>

#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace
{
const char* kImports = "import QtQuick\nimport QtQuick.Layouts\nimport Themed.Components\n";

void collect(QQuickItem* item, const QString& name, QList<QQuickItem*>& found)
{
    if (item->objectName() == name)
        found.append(item);
    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* child : children)
        collect(child, name, found);
}

class ThemedScrollTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        themeDir = new QTemporaryDir;
        const QString path = themeDir->path() + QStringLiteral("/Theme.qml");
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("pragma Singleton\nimport Themed.Theme\nDefaultTheme {}\n");
        file.close();
        qmlRegisterSingletonType(QUrl::fromLocalFile(path), "Themed.Components", 1, 0, "Theme");
    }

    static void TearDownTestSuite()
    {
        delete themeDir;
        themeDir = nullptr;
    }

    void SetUp() override { engine.addImportPath(QStringLiteral(QML_IMPORT_ROOT)); }

    void show(const QString& body)
    {
        QQmlComponent component(&engine);
        component.setData(QByteArray(kImports) + body.toUtf8(), QUrl());
        ASSERT_FALSE(component.isError()) << component.errorString().toStdString();

        scroll.reset(qobject_cast<QQuickItem*>(component.create()));
        ASSERT_TRUE(scroll);
        scroll->setWidth(200);
        scroll->setHeight(300);
        scroll->setParentItem(window.contentItem());
        window.resize(200, 300);
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(50);
    }

    QList<QQuickItem*> named(const QString& name)
    {
        QList<QQuickItem*> found;
        collect(scroll.get(), name, found);
        return found;
    }

    static QTemporaryDir* themeDir;

    QQmlEngine engine;
    QQuickWindow window;
    std::unique_ptr<QQuickItem> scroll;
};

QTemporaryDir* ThemedScrollTest::themeDir = nullptr;
}

TEST_F(ThemedScrollTest, ARepeaterInsideStacksItsItemsAsWideAsTheScroll)
{
    show(QStringLiteral("ThemedScroll { Repeater { model: 3; Rectangle { objectName: \"row\"; "
                        "Layout.fillWidth: true; Layout.preferredHeight: 50 } } }"));

    const QList<QQuickItem*> rows = named(QStringLiteral("row"));
    ASSERT_EQ(rows.size(), 3);
    for (QQuickItem* row : rows)
        EXPECT_EQ(row->width(), 200);
    EXPECT_EQ(rows[0]->y(), 0);
    EXPECT_GE(rows[1]->y(), rows[0]->y() + 50);
    EXPECT_GE(rows[2]->y(), rows[1]->y() + 50);
}

TEST_F(ThemedScrollTest, AColumnInsideFillsTheWidthOfTheScroll)
{
    show(QStringLiteral("ThemedScroll { ColumnLayout { Layout.fillWidth: true; Rectangle { "
                        "objectName: \"row\"; Layout.fillWidth: true; Layout.preferredHeight: 50 } "
                        "} }"));

    const QList<QQuickItem*> rows = named(QStringLiteral("row"));
    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0]->width(), 200);
}

TEST_F(ThemedScrollTest, TheContentScrollsWhenItIsTallerThanTheScroll)
{
    show(QStringLiteral("ThemedScroll { Repeater { model: 10; Rectangle { Layout.fillWidth: true; "
                        "Layout.preferredHeight: 50 } } }"));

    EXPECT_GE(scroll->property("contentHeight").toReal(), 500);
}
