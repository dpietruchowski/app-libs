#include <gtest/gtest.h>

#include <QString>

#include "dbtoolkit/query/alias.h"
#include "dbtoolkit/query/case.h"
#include "dbtoolkit/query/cast.h"
#include "dbtoolkit/query/expr.h"

class ExprTest : public ::testing::Test
{
};

TEST_F(ExprTest, Column_QualifiesWithTheAliasAndKeepsTheColumnAsKey)
{
    Expr expression(TableAlias("atl"), "success_rate");

    EXPECT_EQ(expression.toSql(), "atl.success_rate");
    EXPECT_EQ(expression.key(), "success_rate");
}

TEST_F(ExprTest, Column_WithoutAnAliasIsTheBareColumn)
{
    Expr expression(TableAlias(), "success_rate");

    EXPECT_EQ(expression.toSql(), "success_rate");
    EXPECT_EQ(expression.key(), "success_rate");
}

TEST_F(ExprTest, Raw_TakesTheSqlAndTheKeySeparately)
{
    Expr expression("COUNT(*)", "total");

    EXPECT_EQ(expression.toSql(), "COUNT(*)");
    EXPECT_EQ(expression.key(), "total");
}

TEST_F(ExprTest, Query_TakesItsSqlFromTheExpression)
{
    Expr expression(Cast("language", "INTEGER"), "language");

    EXPECT_EQ(expression.toSql(), "CAST(language AS INTEGER)");
    EXPECT_EQ(expression.key(), "language");
}

TEST_F(ExprTest, Query_IsReadEagerlySoTheSourceMayDie)
{
    Expr expression = []
    {
        Cast cast("language", "INTEGER");
        return Expr(cast, "language");
    }();

    EXPECT_EQ(expression.toSql(), "CAST(language AS INTEGER)");
}

TEST_F(ExprTest, Expr_IsItselfASqlQuerySoItNests)
{
    Case branches("language");
    branches.when(1, "pl").otherwise("language");

    Expr inner(branches, "language");
    Expr outer(inner, "renamed");

    EXPECT_EQ(outer.toSql(), branches.toSql());
    EXPECT_EQ(outer.key(), "renamed");
}
