#include <gtest/gtest.h>

#include "dbtoolkit/query/expr.h"
#include "dbtoolkit/query/func.h"

class FuncTest : public ::testing::Test
{
};

TEST_F(FuncTest, Coalesce_TakesTheExpressionAndItsFallback)
{
    EXPECT_EQ(Func::coalesce("ls.total_attempts", "0").toSql(), "COALESCE(ls.total_attempts, 0)");
}

TEST_F(FuncTest, Max_JoinsEveryArgument)
{
    EXPECT_EQ(Func::max({ "ls.last_attempt_time", "rs.last_attempt_time" }).toSql(),
              "MAX(ls.last_attempt_time, rs.last_attempt_time)");
}

TEST_F(FuncTest, Max_WithOneArgumentIsTheAggregate)
{
    EXPECT_EQ(Func::max({ "attempt_time" }).toSql(), "MAX(attempt_time)");
}

TEST_F(FuncTest, Min_JoinsEveryArgument)
{
    EXPECT_EQ(Func::min({ "a", "b" }).toSql(), "MIN(a, b)");
}

TEST_F(FuncTest, Count_WrapsTheExpression) { EXPECT_EQ(Func::count("*").toSql(), "COUNT(*)"); }

TEST_F(FuncTest, CountDistinct_MarksTheArgument)
{
    EXPECT_EQ(Func::countDistinct("tag").toSql(), "COUNT(DISTINCT tag)");
}

TEST_F(FuncTest, AnyNamedFunctionCanBeBuilt)
{
    EXPECT_EQ(Func("IFNULL", { "a", "b" }).toSql(), "IFNULL(a, b)");
}

TEST_F(FuncTest, Func_IsASqlQuerySoItNestsInAnExpr)
{
    const Expr expression(Func::coalesce("sch.current_level", "0"), "review_level");

    EXPECT_EQ(expression.toSql(), "COALESCE(sch.current_level, 0)");
    EXPECT_EQ(expression.key(), "review_level");
}
