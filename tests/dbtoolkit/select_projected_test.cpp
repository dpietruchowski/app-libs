#include <gtest/gtest.h>

#include <QString>

#include "dbtoolkit/query/alias.h"
#include "dbtoolkit/query/join.h"
#include "dbtoolkit/query/projection.h"
#include "dbtoolkit/query/select.h"

namespace
{

Join summaryJoin()
{
    return Join("attempt_summaries")
        .as(TableAlias("atl"))
        .on(TableAlias("fp"), "fact_id")
        .equals("fact_id");
}

Join projectingJoin() { return summaryJoin().withColumns({ "total_attempts" }).withPrefix("atl"); }

Select shape(const Join& join)
{
    Select select;
    select.from("fact_progress").as(TableAlias("fp")).leftJoin(join);
    return select;
}

}

class SelectProjectedTest : public ::testing::Test
{
};

TEST_F(SelectProjectedTest, ExpressionsAreWrittenVerbatimWithoutTheFromAlias)
{
    Select select;
    select.from("fact_progress").as(TableAlias("fp")).projected({ "atl.total_attempts AS x" });

    EXPECT_TRUE(select.toSql().startsWith("SELECT atl.total_attempts AS x FROM"));
}

TEST_F(SelectProjectedTest, PlainColumnsStillTakeTheFromAlias)
{
    Select select({ "fact_id" });
    select.from("fact_progress").as(TableAlias("fp"));

    EXPECT_TRUE(select.toSql().startsWith("SELECT fp.fact_id FROM"));
}

TEST_F(SelectProjectedTest, AJoinWithColumnsProjectsThemOnItsOwn)
{
    EXPECT_TRUE(
        shape(projectingJoin()).toSql().contains("atl.total_attempts as atl_total_attempts"));
}

TEST_F(SelectProjectedTest, AJoinWithoutColumnsProjectsNothingButStillJoins)
{
    const QString sql = shape(summaryJoin()).toSql();

    EXPECT_FALSE(sql.contains("total_attempts"));
    EXPECT_TRUE(sql.contains("LEFT JOIN attempt_summaries atl ON fp.fact_id = atl.fact_id"));
}

TEST_F(SelectProjectedTest, SetColumnsAloneCannotUnprojectAJoin)
{
    Select select = shape(projectingJoin());

    select.setColumns({ "fact_id" });

    EXPECT_TRUE(select.toSql().contains("atl_total_attempts"));
}

TEST_F(SelectProjectedTest, ClearProjectedDropsWhatTheJoinAdded)
{
    Select select = shape(projectingJoin());

    select.clearProjected();

    EXPECT_FALSE(select.toSql().contains("atl_total_attempts"));
    EXPECT_TRUE(select.toSql().contains("LEFT JOIN attempt_summaries atl"));
}

TEST_F(SelectProjectedTest, ClearingThenProjectingReplacesTheColumnList)
{
    Select select = shape(projectingJoin());

    select.setColumns(QStringList());
    select.clearProjected();
    select.projected(Projection(TableAlias("fp"), { "fact_id" }).selectExpressions());

    EXPECT_TRUE(select.toSql().startsWith("SELECT fp.fact_id AS fp_fact_id FROM"));
    EXPECT_FALSE(select.toSql().contains("atl_total_attempts"));
}

TEST_F(SelectProjectedTest, ProjectionsAccumulateInOrder)
{
    Select select;
    select.from("fact_progress").as(TableAlias("fp")).projected({ "a" }).projected({ "b", "c" });

    EXPECT_TRUE(select.toSql().startsWith("SELECT a, b, c FROM"));
}

TEST_F(SelectProjectedTest, OrderByAJoinColumnSurvivesClearingTheProjection)
{
    Select select = shape(projectingJoin());
    select.orderBy("atl.total_attempts DESC");

    select.clearProjected();

    EXPECT_FALSE(select.toSql().contains("atl_total_attempts"));
    EXPECT_TRUE(select.toSql().contains("ORDER BY atl.total_attempts DESC"));
}
