#include <gtest/gtest.h>

#include <QString>

#include "dbtoolkit/query/alias.h"
#include "dbtoolkit/query/case.h"
#include "dbtoolkit/query/cast.h"
#include "dbtoolkit/query/expr.h"
#include "dbtoolkit/query/where.h"

class SearchedCaseTest : public ::testing::Test
{
};

TEST_F(SearchedCaseTest, WithoutASubject_TheConditionFollowsWhenDirectly)
{
    Case branches;
    branches.when(Where(TableAlias("rs"), "fact_id").isNotNull(), Expr(TableAlias("atr"), "level"));

    EXPECT_EQ(branches.toSql(), "CASE WHEN rs.fact_id IS NOT NULL THEN atr.level END");
}

TEST_F(SearchedCaseTest, TheBranchIsAnExpressionNotAQuotedLiteral)
{
    Case branches;
    branches.when(Where("level").greaterThan(0), Expr(TableAlias("atr"), "success_rate"));

    EXPECT_TRUE(branches.toSql().contains("THEN atr.success_rate"));
    EXPECT_FALSE(branches.toSql().contains("'atr.success_rate'"));
}

TEST_F(SearchedCaseTest, OtherwiseTakesAnExpressionToo)
{
    Case branches;
    branches.when(Where(TableAlias("rs"), "fact_id").isNotNull(), Expr(TableAlias("atr"), "rate"))
        .otherwise(Expr(TableAlias("atl"), "rate"));

    EXPECT_EQ(branches.toSql(), "CASE WHEN rs.fact_id IS NOT NULL THEN atr.rate ELSE atl.rate END");
}

TEST_F(SearchedCaseTest, ManyConditionsKeepTheirOrder)
{
    Case branches;
    branches.when(Where("level").greaterThan(5), Expr("3", "weight"))
        .when(Where("level").greaterThan(2), Expr("2", "weight"))
        .otherwise(Expr("1", "weight"));

    EXPECT_EQ(branches.toSql(), "CASE WHEN level > 5 THEN 3 WHEN level > 2 THEN 2 ELSE 1 END");
}

TEST_F(SearchedCaseTest, ACaseNestsInsideAnotherCase)
{
    Case inner;
    inner.when(Where("level").greaterThan(5), Expr("3", "weight")).otherwise(Expr("1", "weight"));

    Case outer;
    outer.when(Where(TableAlias("rs"), "fact_id").isNotNull(), inner)
        .otherwise(Expr("0", "weight"));

    EXPECT_EQ(outer.toSql(),
              "CASE WHEN rs.fact_id IS NOT NULL THEN CASE WHEN level > 5 THEN 3 ELSE 1 END "
              "ELSE 0 END");
}

TEST_F(SearchedCaseTest, ASubjectCaseStillQuotesItsValues)
{
    Case languageCase("language");
    languageCase.when(1, "pl").when(2, "en").otherwise("language");

    EXPECT_EQ(languageCase.toSql(),
              "CASE language WHEN 1 THEN 'pl' WHEN 2 THEN 'en' ELSE language END");
}

TEST_F(SearchedCaseTest, ASubjectCaseAcceptsAnExpressionBranch)
{
    Case languageCase("language");
    languageCase.when(Where("language").equals(1), Cast("language", "TEXT"));

    EXPECT_EQ(languageCase.toSql(),
              "CASE language WHEN language = 1 THEN CAST(language AS TEXT) END");
}
