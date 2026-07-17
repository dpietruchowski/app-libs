#include <gtest/gtest.h>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

#include "dbtoolkit/query/case.h"
#include "dbtoolkit/query/cast.h"
#include "dbtoolkit/query/select.h"
#include "dbtoolkit/query/update.h"
#include "dbtoolkit/query/where.h"

class CastQueryTest : public ::testing::Test
{
};

TEST_F(CastQueryTest, Cast_GeneratesCorrectSQL)
{
    Cast cast("language", "INTEGER");

    EXPECT_EQ(cast.toSql(), "CAST(language AS INTEGER)");
}

class CaseQueryTest : public ::testing::Test
{
};

TEST_F(CaseQueryTest, SingleBranch_GeneratesCorrectSQL)
{
    Case languageCase("language");
    languageCase.when(0, "und");

    EXPECT_EQ(languageCase.toSql(), "CASE language WHEN 0 THEN 'und' END");
}

TEST_F(CaseQueryTest, MultipleBranches_GeneratesCorrectSQL)
{
    Case languageCase("language");
    languageCase.when(1, "pl").when(2, "en");

    EXPECT_EQ(languageCase.toSql(), "CASE language WHEN 1 THEN 'pl' WHEN 2 THEN 'en' END");
}

TEST_F(CaseQueryTest, Otherwise_AppendsRawElseExpression)
{
    Case languageCase("language");
    languageCase.when(1, "pl").otherwise("language");

    EXPECT_EQ(languageCase.toSql(), "CASE language WHEN 1 THEN 'pl' ELSE language END");
}

TEST_F(CaseQueryTest, StringResult_IsQuoted_NumberSubjectAndMatch_AreNot)
{
    Case languageCase("language");
    languageCase.when(10, "ru");

    QString sql = languageCase.toSql();

    EXPECT_TRUE(sql.contains("WHEN 10 THEN 'ru'"));
}

TEST_F(CaseQueryTest, StringMatch_IsQuoted)
{
    Case statusCase("status");
    statusCase.when("a", "active");

    EXPECT_EQ(statusCase.toSql(), "CASE status WHEN 'a' THEN 'active' END");
}

TEST_F(CaseQueryTest, SingleQuoteInResult_IsEscaped)
{
    Case labelCase("id");
    labelCase.when(1, "it's");

    EXPECT_TRUE(labelCase.toSql().contains("THEN 'it''s'"));
}

TEST_F(CaseQueryTest, CaseOverCast_ComposesInSelect)
{
    Case languageCase(Cast("language", "INTEGER").toSql());
    languageCase.when(1, "pl").otherwise("language");

    EXPECT_EQ(languageCase.toSql(),
              "CASE CAST(language AS INTEGER) WHEN 1 THEN 'pl' ELSE language END");
}

class UpdateSetRawTest : public ::testing::Test
{
};

TEST_F(UpdateSetRawTest, SetRaw_EmitsUnboundExpression)
{
    Update update("expressions");
    update.setRaw("language", "'pl'");

    EXPECT_EQ(update.toSql(), "UPDATE expressions SET language = 'pl'");
}

TEST_F(UpdateSetRawTest, SetRaw_WithCaseAndWhere_GeneratesFullStatement)
{
    Case languageCase("language");
    languageCase.when(1, "pl").otherwise("language");

    Update update("expressions");
    update.setRaw("language", languageCase.toSql()).where(Where("language").glob("[0-9]*"));

    EXPECT_EQ(update.toSql(),
              "UPDATE expressions SET language = CASE language WHEN 1 THEN 'pl' ELSE language END "
              "WHERE language GLOB '[0-9]*'");
}

TEST_F(UpdateSetRawTest, SetRaw_HasNoBindPlaceholders)
{
    Update update("expressions");
    update.setRaw("language", "'pl'");

    EXPECT_EQ(update.toSql().count("?"), 0);
}

TEST_F(UpdateSetRawTest, SetAndSetRaw_Combine)
{
    Update update("expressions");
    update.set("name", "John").setRaw("language", "'pl'");

    QString sql = update.toSql();

    EXPECT_TRUE(sql.contains("name = ?"));
    EXPECT_TRUE(sql.contains("language = 'pl'"));
}

class WhereGlobTest : public ::testing::Test
{
};

TEST_F(WhereGlobTest, Glob_GeneratesCorrectSQL)
{
    Where where("language");
    where.glob("[0-9]*");

    EXPECT_EQ(where.build(), "language GLOB '[0-9]*'");
}

TEST_F(WhereGlobTest, Glob_ChainsWithAnd)
{
    Where where("language");
    where.glob("[0-9]*").and_("active").equals(1);

    QString sql = where.build();

    EXPECT_TRUE(sql.contains("language GLOB '[0-9]*'"));
    EXPECT_TRUE(sql.contains("AND"));
    EXPECT_TRUE(sql.contains("active = 1"));
}

class CaseUpdateExecutionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_db = QSqlDatabase::addDatabase("QSQLITE", "case_update_exec_test");
        m_db.setDatabaseName(":memory:");
        ASSERT_TRUE(m_db.open());

        QSqlQuery create(m_db);
        ASSERT_TRUE(create.exec("CREATE TABLE expressions (id INTEGER PRIMARY KEY, language TEXT)"));
    }

    void TearDown() override
    {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase("case_update_exec_test");
    }

    void insertLanguage(int id, const QString& language)
    {
        QSqlQuery query(m_db);
        query.prepare("INSERT INTO expressions (id, language) VALUES (?, ?)");
        query.addBindValue(id);
        query.addBindValue(language);
        ASSERT_TRUE(query.exec());
    }

    QString languageOf(int id)
    {
        QSqlQuery query(m_db);
        query.prepare("SELECT language FROM expressions WHERE id = ?");
        query.addBindValue(id);
        query.exec();
        query.next();
        return query.value(0).toString();
    }

    QSqlDatabase m_db;
};

TEST_F(CaseUpdateExecutionTest, MapsNumericOrdinalsToCodes)
{
    insertLanguage(1, "1");
    insertLanguage(2, "2");

    Case languageCase(Cast("language", "INTEGER").toSql());
    languageCase.when(1, "pl").when(2, "en").otherwise("language");

    Update update("expressions");
    const int affected = update.setRaw("language", languageCase.toSql())
                             .where(Where("language").glob("[0-9]*"))
                             .execute(m_db)
                             .toInt();

    EXPECT_EQ(affected, 2);
    EXPECT_EQ(languageOf(1), "pl");
    EXPECT_EQ(languageOf(2), "en");
}

TEST_F(CaseUpdateExecutionTest, GlobGuardLeavesAlreadyConvertedCodes)
{
    insertLanguage(1, "pl");
    insertLanguage(2, "2");

    Case languageCase(Cast("language", "INTEGER").toSql());
    languageCase.when(1, "pl").when(2, "en").otherwise("language");

    Update update("expressions");
    const int affected = update.setRaw("language", languageCase.toSql())
                             .where(Where("language").glob("[0-9]*"))
                             .execute(m_db)
                             .toInt();

    EXPECT_EQ(affected, 1);
    EXPECT_EQ(languageOf(1), "pl");
    EXPECT_EQ(languageOf(2), "en");
}

TEST_F(CaseUpdateExecutionTest, IsIdempotentOnSecondRun)
{
    insertLanguage(1, "1");

    Case languageCase(Cast("language", "INTEGER").toSql());
    languageCase.when(1, "pl").otherwise("language");

    auto runMigration = [&]
    {
        return Update("expressions")
            .setRaw("language", languageCase.toSql())
            .where(Where("language").glob("[0-9]*"))
            .execute(m_db)
            .toInt();
    };

    EXPECT_EQ(runMigration(), 1);
    EXPECT_EQ(runMigration(), 0);
    EXPECT_EQ(languageOf(1), "pl");
}
