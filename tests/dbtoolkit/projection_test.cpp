#include <gtest/gtest.h>

#include <QMap>
#include <QVariantMap>

#include "dbtoolkit/query/alias.h"
#include "dbtoolkit/query/case.h"
#include "dbtoolkit/query/expr.h"
#include "dbtoolkit/query/projection.h"
#include "dbtoolkit/query/where.h"

namespace
{

const QStringList kSummaryColumns { "total_attempts", "correct_attempts", "success_rate" };

}

class ProjectionTest : public ::testing::Test
{
};

TEST_F(ProjectionTest, Columns_AreQualifiedInSqlAndPrefixedInTheResult)
{
    const Projection projection(TableAlias("atl"), kSummaryColumns);

    EXPECT_EQ(projection.group(), "atl");
    EXPECT_EQ(projection.selectExpressions(),
              (QStringList { "atl.total_attempts AS atl_total_attempts",
                             "atl.correct_attempts AS atl_correct_attempts",
                             "atl.success_rate AS atl_success_rate" }));
}

TEST_F(ProjectionTest, Extract_StripsThePrefixAndKeepsTheColumnNames)
{
    const Projection projection(TableAlias("atl"), kSummaryColumns);

    QVariantMap row;
    row["atl_total_attempts"] = 6;
    row["atl_correct_attempts"] = 3;
    row["atl_success_rate"] = 0.3;
    row["fp_fact_id"] = "f4";

    const QVariantMap values = projection.extract(row);

    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values.value("total_attempts").toInt(), 6);
    EXPECT_EQ(values.value("correct_attempts").toInt(), 3);
    EXPECT_DOUBLE_EQ(values.value("success_rate").toDouble(), 0.3);
}

TEST_F(ProjectionTest, TheSelectNamesAreExactlyTheNamesExtractionLooksFor)
{
    const Projection projection(TableAlias("atl"), kSummaryColumns);

    QVariantMap row;
    for (const QString& expression : projection.selectExpressions())
    {
        row[expression.section(" AS ", 1, 1)] = 7;
    }

    EXPECT_EQ(projection.extract(row).size(), kSummaryColumns.size());
}

TEST_F(ProjectionTest, AMissingColumnIsAbsentRatherThanNull)
{
    const Projection projection(TableAlias("atl"), kSummaryColumns);

    QVariantMap row;
    row["atl_total_attempts"] = 6;

    const QVariantMap values = projection.extract(row);

    EXPECT_EQ(values.size(), 1);
    EXPECT_FALSE(values.contains("success_rate"));
}

TEST_F(ProjectionTest, WithoutAnAliasTheColumnsStayBare)
{
    const Projection projection(TableAlias(), { "fact_id" });

    EXPECT_EQ(projection.selectExpressions(), (QStringList { "fact_id AS fact_id" }));

    QVariantMap row;
    row["fact_id"] = "f1";

    EXPECT_EQ(projection.extract(row).value("fact_id").toString(), "f1");
}

TEST_F(ProjectionTest, AComputedGroupCarriesExpressionsUnderItsOwnName)
{
    Case branches;
    branches.when(Where(TableAlias("rs"), "fact_id").isNotNull(), Expr(TableAlias("atr"), "rate"))
        .otherwise(Expr(TableAlias("atl"), "rate"));

    const Projection projection("calc", { Expr(branches, "accuracy") });

    EXPECT_EQ(projection.selectExpressions(),
              (QStringList { "CASE WHEN rs.fact_id IS NOT NULL THEN atr.rate ELSE atl.rate END "
                             "AS calc_accuracy" }));

    QVariantMap row;
    row["calc_accuracy"] = 0.75;

    EXPECT_DOUBLE_EQ(projection.extract(row).value("accuracy").toDouble(), 0.75);
}

TEST_F(ProjectionTest, AnEmptyProjectionSelectsAndExtractsNothing)
{
    const Projection projection(TableAlias("atl"), {});

    EXPECT_TRUE(projection.selectExpressions().isEmpty());

    QVariantMap row;
    row["atl_total_attempts"] = 6;

    EXPECT_TRUE(projection.extract(row).isEmpty());
}

TEST_F(ProjectionTest, TwoAliasesOverTheSameColumnsDoNotCollide)
{
    const Projection learning(TableAlias("atl"), { "total_attempts" });
    const Projection review(TableAlias("atr"), { "total_attempts" });

    QVariantMap row;
    row["atl_total_attempts"] = 6;
    row["atr_total_attempts"] = 4;

    EXPECT_EQ(learning.extract(row).value("total_attempts").toInt(), 6);
    EXPECT_EQ(review.extract(row).value("total_attempts").toInt(), 4);
}

class ProjectedRowTest : public ::testing::Test
{
};

TEST_F(ProjectedRowTest, GroupsAreReadBackByName)
{
    QMap<QString, QVariantMap> groups;
    groups["fp"] = QVariantMap { { "fact_id", "f1" } };
    groups["atl"] = QVariantMap { { "total_attempts", 6 } };

    const ProjectedRow row(groups);

    EXPECT_EQ(row.of("fp").value("fact_id").toString(), "f1");
    EXPECT_EQ(row.of("atl").value("total_attempts").toInt(), 6);
}

TEST_F(ProjectedRowTest, AnUnknownGroupWarnsAndYieldsNothing)
{
    QMap<QString, QVariantMap> groups;
    groups["fp"] = QVariantMap { { "fact_id", "f1" } };

    const ProjectedRow row(groups);

    EXPECT_FALSE(row.contains("atl"));
    EXPECT_TRUE(row.of("atl").isEmpty());
}

TEST_F(ProjectedRowTest, ADefaultRowHasNoGroups)
{
    const ProjectedRow row;

    EXPECT_FALSE(row.contains("fp"));
    EXPECT_TRUE(row.of("fp").isEmpty());
}
