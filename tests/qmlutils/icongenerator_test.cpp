#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <gtest/gtest.h>

#include "qmlutils/icongenerator.h"

namespace
{
bool isWellFormed(const QString& svg)
{
    QXmlStreamReader reader(svg);
    while (!reader.atEnd())
        reader.readNext();
    return !reader.hasError();
}
}

class IconGeneratorTest : public ::testing::Test
{
protected:
    QTemporaryDir dir;
    QString error;
};

TEST_F(IconGeneratorTest, TheSameSeedGivesTheSameIcon)
{
    EXPECT_EQ(IconGenerator::svg(QStringLiteral("notes")),
              IconGenerator::svg(QStringLiteral("notes")));
    EXPECT_NE(IconGenerator::svg(QStringLiteral("notes")),
              IconGenerator::svg(QStringLiteral("timer")));
}

TEST_F(IconGeneratorTest, EveryIconIsWellFormedAndCarriesTheSharedHeader)
{
    for (quint32 seed = 0; seed < 500; ++seed)
    {
        const QString svg = IconGenerator::svg(seed);
        ASSERT_TRUE(isWellFormed(svg)) << seed;
        EXPECT_TRUE(svg.contains(QStringLiteral("viewBox=\"0 0 24 24\"")));
        EXPECT_TRUE(svg.contains(QStringLiteral("stroke=\"currentColor\"")));
        EXPECT_FALSE(svg.contains(QStringLiteral("nan")));
    }
}

TEST_F(IconGeneratorTest, SeedsSpreadOverManyDistinctIcons)
{
    QSet<QString> icons;
    for (quint32 seed = 0; seed < 200; ++seed)
        icons.insert(IconGenerator::svg(seed));

    EXPECT_GT(icons.size(), 150);
}

TEST_F(IconGeneratorTest, WritesTheIconAndCreatesMissingDirectories)
{
    const QString path = dir.path() + QStringLiteral("/notes/icon.svg");

    EXPECT_TRUE(IconGenerator::writeTo(path, QStringLiteral("notes"), &error));
    EXPECT_TRUE(error.isEmpty());

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    EXPECT_EQ(QString::fromUtf8(file.readAll()), IconGenerator::svg(QStringLiteral("notes")));
}

TEST_F(IconGeneratorTest, ReportsAnUnwritablePath)
{
    EXPECT_FALSE(IconGenerator::writeTo(QStringLiteral("/proc/one/two/icon.svg"),
                                        QStringLiteral("notes"), &error));
    EXPECT_FALSE(error.isEmpty());
}
