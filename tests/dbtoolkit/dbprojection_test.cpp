#include <gtest/gtest.h>

#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>

#include <memory>

#include "dbtoolkit/dbprojection.h"
#include "dbtoolkit/dbstorage.h"
#include "dbtoolkit/query/case.h"
#include "dbtoolkit/query/expr.h"
#include "dbtoolkit/query/join.h"
#include "dbtoolkit/query/select.h"
#include "dbtoolkit/query/where.h"

namespace
{

const TableAlias kProgress("fp");
const TableAlias kLearning("atl");
const TableAlias kReview("atr");
const TableAlias kSchedule("rs");

const QStringList kSummaryColumns { "total_attempts", "correct_attempts", "success_rate" };

Expr accuracyOf(const TableAlias& alias)
{
    return Expr(
        QString("CAST(COALESCE(%1, 0) AS REAL) / MAX(COALESCE(%2, 0), 1)")
            .arg(alias.createColumn("correct_attempts"), alias.createColumn("total_attempts")),
        "accuracy");
}

Expr accuracy()
{
    Case branches;
    branches.when(Where(kSchedule, "fact_id").isNotNull(), accuracyOf(kReview))
        .otherwise(accuracyOf(kLearning));
    return Expr(branches, "accuracy");
}

Join summaryJoin(const TableAlias& alias, int stage)
{
    return Join("attempt_summaries")
        .as(alias)
        .on(kProgress, "fact_id")
        .equals("fact_id")
        .andColumn("stage")
        .equalsValue(stage);
}

Select shape()
{
    Select select;
    select.from("fact_progress")
        .as(kProgress)
        .leftJoin(summaryJoin(kLearning, 0))
        .leftJoin(summaryJoin(kReview, 1))
        .leftJoin(
            Join("review_schedules").as(kSchedule).on(kProgress, "fact_id").equals("fact_id"));
    return select;
}

Select sortedByAccuracy()
{
    Select select = shape();
    select.orderBy(accuracy().toSql() + " DESC, " + kProgress.createColumn("fact_id") + " ASC");
    return select;
}

struct TestRow final
{
    QString factId;
    double accuracy { 0.0 };
    int learningTotal { 0 };
    int reviewTotal { 0 };
    int level { 0 };
};

TestRow rowFrom(const ProjectedRow& row)
{
    TestRow result;
    result.factId = row.of("fp").value("fact_id").toString();
    result.accuracy = row.of("calc").value("accuracy").toDouble();
    result.learningTotal = row.of("atl").value("total_attempts").toInt();
    result.reviewTotal = row.of("atr").value("total_attempts").toInt();
    result.level = row.of("rs").value("current_level").toInt();
    return result;
}

QList<Projection> wideProjections()
{
    return { Projection(kProgress, { "fact_id", "priority" }), Projection("calc", { accuracy() }),
             Projection(kLearning, kSummaryColumns), Projection(kReview, kSummaryColumns),
             Projection(kSchedule, { "current_level", "next_review_date" }) };
}

QList<Projection> narrowProjections()
{
    return { Projection(kProgress, { "fact_id", "priority" }), Projection("calc", {}),
             Projection(kLearning, {}), Projection(kReview, {}), Projection(kSchedule, {}) };
}

QStringList idsOf(const QVector<TestRow>& rows)
{
    QStringList ids;
    for (const TestRow& row : rows)
    {
        ids.append(row.factId);
    }
    return ids;
}

}

class DbProjectionTest : public ::testing::Test
{
protected:
    QSqlDatabase database;
    std::unique_ptr<DbStorage> storage;

    void SetUp() override
    {
        database = QSqlDatabase::addDatabase("QSQLITE", "dbprojection_test");
        database.setDatabaseName(":memory:");
        ASSERT_TRUE(database.open());
        storage = std::make_unique<DbStorage>(database);

        exec("CREATE TABLE fact_progress (fact_id TEXT PRIMARY KEY, priority INTEGER NOT NULL)");
        exec("CREATE TABLE attempt_summaries (fact_id TEXT NOT NULL, stage INTEGER NOT NULL, "
             "total_attempts INTEGER NOT NULL, correct_attempts INTEGER NOT NULL, "
             "success_rate REAL NOT NULL, PRIMARY KEY (fact_id, stage))");
        exec("CREATE TABLE review_schedules (fact_id TEXT PRIMARY KEY, current_level INTEGER "
             "NOT NULL, next_review_date TEXT NOT NULL)");
        exec("CREATE TABLE loose_summaries (fact_id TEXT NOT NULL, total_attempts INTEGER NOT "
             "NULL)");

        exec("INSERT INTO fact_progress VALUES ('f1', 50), ('f2', 90), ('f3', 10), ('f4', 70), "
             "('f5', 30)");
        exec("INSERT INTO attempt_summaries VALUES ('f1', 0, 10, 9, 0.10), ('f2', 0, 4, 1, 0.99), "
             "('f3', 0, 2, 2, 0.20), ('f4', 0, 6, 3, 0.30), ('f4', 1, 4, 4, 0.40)");
        exec("INSERT INTO review_schedules VALUES ('f4', 3, '2026-10-01')");
    }

    void TearDown() override
    {
        storage.reset();
        database.close();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase("dbprojection_test");
    }

    void exec(const QString& sql)
    {
        QSqlQuery query(database);
        ASSERT_TRUE(query.exec(sql)) << query.lastError().text().toStdString();
    }

    DbProjection<TestRow> wide()
    {
        return DbProjection<TestRow>(*storage, wideProjections(), rowFrom);
    }
};

TEST_F(DbProjectionTest, SqlComputesTheValueAcrossTablesBeforeSorting)
{
    const QVector<TestRow> rows = wide().findAll(sortedByAccuracy());

    EXPECT_EQ(idsOf(rows), (QStringList { "f3", "f4", "f1", "f2", "f5" }));
    EXPECT_DOUBLE_EQ(rows[0].accuracy, 1.0);
    EXPECT_DOUBLE_EQ(rows[2].accuracy, 0.9);
    EXPECT_DOUBLE_EQ(rows[4].accuracy, 0.0);
}

TEST_F(DbProjectionTest, TheComputedValueDiffersFromTheStoredColumn)
{
    Select byStoredRate = shape();
    byStoredRate.orderBy(kLearning.createColumn("success_rate") + " DESC");
    byStoredRate.limit(1);

    EXPECT_EQ(idsOf(wide().findAll(byStoredRate)), (QStringList { "f2" }));

    Select byComputed = sortedByAccuracy();
    byComputed.limit(1);

    EXPECT_EQ(idsOf(wide().findAll(byComputed)), (QStringList { "f3" }));
}

TEST_F(DbProjectionTest, LimitCutsAfterTheComputedSort)
{
    Select limited = sortedByAccuracy();
    limited.limit(2);

    EXPECT_EQ(idsOf(wide().findAll(limited)), (QStringList { "f3", "f4" }));
}

TEST_F(DbProjectionTest, TheSameExpressionFiltersInWhere)
{
    Select filtered = sortedByAccuracy();
    filtered.where(accuracy().toSql() + " >= 0.9");

    EXPECT_EQ(idsOf(wide().findAll(filtered)), (QStringList { "f3", "f4", "f1" }));
}

TEST_F(DbProjectionTest, SortingWorksWhenTheExpressionIsNotProjected)
{
    DbProjection<TestRow> narrow(*storage, narrowProjections(), rowFrom);

    const QVector<TestRow> rows = narrow.findAll(sortedByAccuracy());

    EXPECT_EQ(idsOf(rows), (QStringList { "f3", "f4", "f1", "f2", "f5" }));
    EXPECT_DOUBLE_EQ(rows[0].accuracy, 0.0);
    EXPECT_FALSE(narrow.projected(sortedByAccuracy()).toSql().contains("calc_accuracy"));
}

TEST_F(DbProjectionTest, TheRowCarriesTheValueSqlComputed)
{
    Select onlyF4 = shape();
    onlyF4.where(Where(kProgress, "fact_id").equals("f4"));

    const std::optional<TestRow> row = wide().findFirst(onlyF4);

    ASSERT_TRUE(row.has_value());
    EXPECT_DOUBLE_EQ(row->accuracy, 1.0);
    EXPECT_EQ(row->learningTotal, 6);
    EXPECT_EQ(row->reviewTotal, 4);
    EXPECT_EQ(row->level, 3);
}

TEST_F(DbProjectionTest, FindFirstFollowsTheOrderAndOverridesTheLimit)
{
    Select shaped = sortedByAccuracy();
    shaped.limit(5);

    const std::optional<TestRow> row = wide().findFirst(shaped);

    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->factId, "f3");
}

TEST_F(DbProjectionTest, FindFirstOnAnEmptyResultYieldsNothing)
{
    Select impossible = shape();
    impossible.where(Where(kProgress, "fact_id").equals("nobody"));

    EXPECT_FALSE(wide().findFirst(impossible).has_value());
}

TEST_F(DbProjectionTest, CountKeepsTheLimitAndDropsTheOrder)
{
    Select filtered = sortedByAccuracy();
    filtered.where(accuracy().toSql() + " >= 0.9");

    EXPECT_EQ(wide().count(filtered), 3);
    EXPECT_FALSE(wide().counting(filtered).toSql().contains("ORDER BY"));

    filtered.limit(2);
    EXPECT_EQ(wide().count(filtered), 2);
}

TEST_F(DbProjectionTest, CountProjectsNothingButAConstant)
{
    const QString sql = wide().counting(sortedByAccuracy()).toSql();

    EXPECT_TRUE(sql.startsWith("SELECT 1 FROM"));
    EXPECT_FALSE(sql.contains("total_attempts"));
}

TEST_F(DbProjectionTest, ExistsFollowsCount)
{
    Select impossible = shape();
    impossible.where(Where(kProgress, "fact_id").equals("nobody"));

    EXPECT_TRUE(wide().exists(shape()));
    EXPECT_FALSE(wide().exists(impossible));
}

TEST_F(DbProjectionTest, TwoAliasedJoinsToTheSameTableDoNotCollide)
{
    const QString sql = wide().projected(shape()).toSql();

    EXPECT_TRUE(sql.contains("atl.total_attempts AS atl_total_attempts"));
    EXPECT_TRUE(sql.contains("atr.total_attempts AS atr_total_attempts"));
}

TEST_F(DbProjectionTest, AShapeThatAlreadyCarriesJoinColumnsIsNormalised)
{
    Select legacy = shape();
    legacy.leftJoin(Join("review_schedules")
                        .as(TableAlias("rs2"))
                        .on(kProgress, "fact_id")
                        .equals("fact_id")
                        .withColumns({ "current_level" })
                        .withPrefix("rs2"));

    EXPECT_FALSE(wide().projected(legacy).toSql().contains("rs2_current_level"));
    EXPECT_EQ(wide().findAll(legacy).size(), 5);
}

TEST_F(DbProjectionTest, AnUndeclaredGroupYieldsAnEmptyRowRatherThanThrowing)
{
    DbProjection<TestRow> onlyProgress(*storage, { Projection(kProgress, { "fact_id" }) }, rowFrom);

    const QVector<TestRow> rows = onlyProgress.findAll(shape());

    EXPECT_EQ(rows.size(), 5);
    EXPECT_DOUBLE_EQ(rows[0].accuracy, 0.0);
}

TEST_F(DbProjectionTest, FanOutWithoutANaturalKeyLosesRowsUnderLimit)
{
    exec("INSERT INTO loose_summaries VALUES ('f1', 1), ('f1', 2), ('f2', 3)");

    Select fannedOut;
    fannedOut.from("fact_progress")
        .as(kProgress)
        .innerJoin(
            Join("loose_summaries").as(TableAlias("ls")).on(kProgress, "fact_id").equals("fact_id"))
        .orderBy(kProgress.createColumn("fact_id") + " ASC")
        .limit(3);

    DbProjection<TestRow> plain(*storage, { Projection(kProgress, { "fact_id" }) }, rowFrom);

    const QStringList ids = idsOf(plain.findAll(fannedOut));

    EXPECT_EQ(ids.size(), 3);
    EXPECT_EQ(QSet<QString>(ids.begin(), ids.end()).size(), 2);
}
