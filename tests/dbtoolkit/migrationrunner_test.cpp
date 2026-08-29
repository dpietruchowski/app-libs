#include <QSqlDatabase>
#include <QSqlQuery>
#include <gtest/gtest.h>

#include "dbtoolkit/dbstorage.h"
#include "dbtoolkit/migrationrunner.h"

class MigrationRunnerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSqlDatabase::removeDatabase("migration_runner_test_db");
        m_db = QSqlDatabase::addDatabase("QSQLITE", "migration_runner_test_db");
        m_db.setDatabaseName(":memory:");
        ASSERT_TRUE(m_db.open());

        m_storage = std::make_unique<DbStorage>(m_db);
    }

    void TearDown() override
    {
        m_storage.reset();
        m_db.close();
    }

    static MigrationRunner::MigrationStep noop()
    {
        return [](QSqlDatabase&) { return true; };
    }

    QSqlDatabase m_db;
    std::unique_ptr<DbStorage> m_storage;
};

TEST_F(MigrationRunnerTest, TargetVersion_WithoutMigrations_IsZero)
{
    MigrationRunner runner(*m_storage);

    EXPECT_EQ(runner.targetVersion(), 0);
}

TEST_F(MigrationRunnerTest, TargetVersion_ReturnsHighestRegisteredVersion)
{
    MigrationRunner runner(*m_storage);
    runner.add(3, noop()).add(7, noop()).add(5, noop());

    EXPECT_EQ(runner.targetVersion(), 7);
}

TEST_F(MigrationRunnerTest, TargetVersion_DoesNotChangeCurrentVersion)
{
    MigrationRunner runner(*m_storage);
    runner.add(4, noop());

    EXPECT_EQ(runner.targetVersion(), 4);
    EXPECT_EQ(runner.currentVersion(), 0);
}

TEST_F(MigrationRunnerTest, Run_LeavesCurrentVersionAtTargetVersion)
{
    MigrationRunner runner(*m_storage);
    runner.add(1, noop()).add(2, noop());

    ASSERT_TRUE(runner.run());

    EXPECT_EQ(runner.currentVersion(), runner.targetVersion());
}

TEST_F(MigrationRunnerTest, Run_FailingStep_LeavesVersionBelowTarget)
{
    MigrationRunner runner(*m_storage);
    runner.add(1, noop()).add(2, [](QSqlDatabase&) { return false; }).add(3, noop());

    EXPECT_FALSE(runner.run());
    EXPECT_EQ(runner.currentVersion(), 1);
    EXPECT_EQ(runner.targetVersion(), 3);
}
