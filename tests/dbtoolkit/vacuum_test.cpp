#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "dbtoolkit/query/vacuum.h"

class VacuumTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSqlDatabase::removeDatabase("vacuum_test_db");
        m_db = QSqlDatabase::addDatabase("QSQLITE", "vacuum_test_db");
        m_db.setDatabaseName(":memory:");
        ASSERT_TRUE(m_db.open());

        QSqlQuery setup(m_db);
        ASSERT_TRUE(setup.exec("CREATE TABLE words (id INTEGER PRIMARY KEY, text TEXT)"));
        ASSERT_TRUE(setup.exec("INSERT INTO words (text) VALUES ('kot'), ('pies')"));

        ASSERT_TRUE(m_directory.isValid());
    }

    void TearDown() override { m_db.close(); }

    QString snapshotPath() const { return m_directory.filePath("snapshot.db"); }

    QSqlDatabase m_db;
    QTemporaryDir m_directory;
};

TEST_F(VacuumTest, ToSql_WithoutTarget_GeneratesPlainVacuum)
{
    EXPECT_EQ(Vacuum().toSql(), "VACUUM");
}

TEST_F(VacuumTest, ToSql_WithTarget_GeneratesBoundPlaceholder)
{
    EXPECT_EQ(Vacuum().into("/tmp/snapshot.db").toSql(), "VACUUM INTO ?");
}

TEST_F(VacuumTest, Execute_WithoutTarget_Succeeds)
{
    EXPECT_EQ(Vacuum().execute(m_db).toInt(), 1);
}

TEST_F(VacuumTest, Execute_IntoFile_WritesReadableSnapshot)
{
    EXPECT_EQ(Vacuum().into(snapshotPath()).execute(m_db).toInt(), 1);
    ASSERT_TRUE(QFile::exists(snapshotPath()));

    QSqlDatabase::removeDatabase("vacuum_snapshot_db");
    QSqlDatabase snapshot = QSqlDatabase::addDatabase("QSQLITE", "vacuum_snapshot_db");
    snapshot.setDatabaseName(snapshotPath());
    ASSERT_TRUE(snapshot.open());

    QSqlQuery query(snapshot);
    ASSERT_TRUE(query.exec("SELECT count(*) FROM words"));
    ASSERT_TRUE(query.next());
    EXPECT_EQ(query.value(0).toInt(), 2);

    snapshot.close();
}

TEST_F(VacuumTest, Execute_IntoPathWithSpaces_WritesSnapshot)
{
    const QString path = m_directory.filePath("my backup 2026.db");

    EXPECT_EQ(Vacuum().into(path).execute(m_db).toInt(), 1);
    EXPECT_TRUE(QFile::exists(path));
}

TEST_F(VacuumTest, Execute_IntoExistingFile_Fails)
{
    ASSERT_EQ(Vacuum().into(snapshotPath()).execute(m_db).toInt(), 1);

    EXPECT_EQ(Vacuum().into(snapshotPath()).execute(m_db).toInt(), 0);
}
