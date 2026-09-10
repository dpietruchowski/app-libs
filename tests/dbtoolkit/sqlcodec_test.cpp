#include "dbtoolkit/sqlcodec.h"

#include <gtest/gtest.h>

#include <QTimeZone>

using namespace sql_codec;

namespace
{
enum class Shape
{
    Round = 0,
    Square = 1,
    Triangle = 2
};
}

TEST(SqlCodecTest, Uuid_RoundTripsAsRfc4122Blob)
{
    const QUuid id = QUuid::createUuid();

    const QVariant stored = toSql(id);

    EXPECT_EQ(stored.typeId(), QMetaType::QByteArray);
    EXPECT_EQ(stored.toByteArray().size(), 16);
    EXPECT_EQ(uuidFromSql(stored), id);
}

TEST(SqlCodecTest, NullUuid_IsSqlNull)
{
    EXPECT_TRUE(toSql(QUuid()).isNull());
    EXPECT_TRUE(uuidFromSql(QVariant()).isNull());
}

TEST(SqlCodecTest, UuidList_EncodesEveryIdInOrder)
{
    const std::vector<QUuid> ids { QUuid::createUuid(), QUuid::createUuid() };

    const QVariantList stored = toSqlList(ids);

    ASSERT_EQ(stored.size(), 2);
    EXPECT_EQ(uuidFromSql(stored.at(0)), ids[0]);
    EXPECT_EQ(uuidFromSql(stored.at(1)), ids[1]);
}

TEST(SqlCodecTest, EmptyUuidList_IsEmpty)
{
    EXPECT_TRUE(toSqlList({}).isEmpty());
}

TEST(SqlCodecTest, DateTime_IsStoredAsUtcIsoTextWithMilliseconds)
{
    const QDateTime local(QDate(2026, 1, 5), QTime(12, 30, 15, 250), QTimeZone(3600));

    const QVariant stored = toSql(local);

    EXPECT_EQ(stored.toString(), "2026-01-05T11:30:15.250Z");
}

TEST(SqlCodecTest, DateTime_RoundTripsToTheSameInstant)
{
    const QDateTime local(QDate(2026, 1, 5), QTime(12, 30, 15, 250), QTimeZone(3600));

    const QDateTime restored = dateTimeFromSql(toSql(local));

    EXPECT_TRUE(restored.isValid());
    EXPECT_EQ(restored, local);
    EXPECT_EQ(restored.timeSpec(), Qt::UTC);
}

TEST(SqlCodecTest, DateTime_LexicalOrderMatchesTimeOrder)
{
    const QDateTime earlier(QDate(2026, 1, 6), QTime(0, 30), QTimeZone(3600));
    const QDateTime later(QDate(2026, 1, 5), QTime(23, 0), QTimeZone(-7200));

    EXPECT_LT(earlier, later);
    EXPECT_LT(toSql(earlier).toString(), toSql(later).toString());
}

TEST(SqlCodecTest, InvalidDateTime_IsSqlNull)
{
    EXPECT_TRUE(toSql(QDateTime()).isNull());
    EXPECT_FALSE(dateTimeFromSql(QVariant()).isValid());
}

TEST(SqlCodecTest, Date_RoundTripsAsIsoText)
{
    const QDate date(2026, 3, 9);

    const QVariant stored = toSql(date);

    EXPECT_EQ(stored.toString(), "2026-03-09");
    EXPECT_EQ(dateFromSql(stored), date);
    EXPECT_TRUE(toSql(QDate()).isNull());
    EXPECT_FALSE(dateFromSql(QVariant()).isValid());
}

TEST(SqlCodecTest, Bool_IsStoredAsZeroOrOne)
{
    EXPECT_EQ(toSql(true).toInt(), 1);
    EXPECT_EQ(toSql(false).toInt(), 0);
    EXPECT_TRUE(boolFromSql(QVariant(1)));
    EXPECT_FALSE(boolFromSql(QVariant(0)));
    EXPECT_FALSE(boolFromSql(QVariant()));
}

TEST(SqlCodecTest, NullString_IsStoredAsEmptyTextNotNull)
{
    const QVariant stored = toSql(QString());

    EXPECT_FALSE(stored.isNull());
    EXPECT_EQ(stored.toString(), "");
    EXPECT_EQ(toSql(QString("abc")).toString(), "abc");
}

TEST(SqlCodecTest, StringList_RoundTripsAsJsonArray)
{
    const std::vector<QString> strings = { "kmalloc", "vmalloc, kfree", "" };

    const QVariant stored = toSql(strings);

    EXPECT_EQ(stored.toString(), R"(["kmalloc","vmalloc, kfree",""])");
    EXPECT_EQ(stringListFromSql(stored), strings);
}

TEST(SqlCodecTest, EmptyStringList_RoundTrips)
{
    const std::vector<QString> empty;

    EXPECT_EQ(toSql(empty).toString(), "[]");
    EXPECT_TRUE(stringListFromSql(toSql(empty)).empty());
    EXPECT_TRUE(stringListFromSql(QVariant()).empty());
}

TEST(SqlCodecTest, Enum_RoundTripsThroughItsIntegerValue)
{
    const QVariant stored = toSql(Shape::Triangle);

    EXPECT_EQ(stored.toInt(), 2);
    EXPECT_EQ(enumFromSql<Shape>(stored), Shape::Triangle);
}
