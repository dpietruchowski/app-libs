#include "chat/infrastructure/chatstorage.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>

#include "async/timeprovider.h"
#include "chat/infrastructure/conversationrepositorydb.h"
#include "chat/infrastructure/conversationserializer.h"
#include "chat/infrastructure/messagerepositorydb.h"
#include "chat/infrastructure/messageserializer.h"
#include "chat/infrastructure/stepserializer.h"
#include "dbtoolkit/dbstorage.h"
#include "dbtoolkit/migrationrunner.h"
#include "dbtoolkit/query/pragma.h"
#include "dbtoolkit/query/select.h"
#include "dbtoolkit/query/update.h"
#include "dbtoolkit/query/where.h"
#include "dbtoolkit/sqlcodec.h"
#include "dbtoolkit/transaction.h"

namespace chat
{

struct ChatStorage::Repositories final
{
    explicit Repositories(DbStorage& storage)
        : conversations(storage)
        , messages(storage)
    {
    }

    bool createTables() { return conversations.createTable() && messages.createTable(); }

    ConversationRepositoryDb conversations;
    MessageRepositoryDb messages;
};

namespace
{
    const QString kSchemaVersionPragma = QStringLiteral("user_version");
}

ChatStorage::ChatStorage(const QString& databasePath, const QString& appName)
    : m_databasePath(databasePath)
    , m_connectionName(QStringLiteral("chat_") + appName)
{
}

ChatStorage::~ChatStorage() { close(); }

bool ChatStorage::open()
{
    if (m_database)
    {
        qWarning() << "ChatStorage: already open";
        return false;
    }

    m_database = std::make_unique<QSqlDatabase>(
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName));
    m_database->setDatabaseName(m_databasePath);

    if (!configureConnection())
    {
        close();
        return false;
    }

    m_storage = std::make_unique<DbStorage>(*m_database);
    m_repositories = std::make_unique<Repositories>(*m_storage);

    if (!migrate())
    {
        close();
        return false;
    }

    const std::optional<QStringList> dropped = interruptRunning();
    if (!dropped)
    {
        close();
        return false;
    }
    m_droppedSessions = *dropped;
    return true;
}

void ChatStorage::close()
{
    m_droppedSessions.clear();
    m_repositories.reset();
    m_storage.reset();
    if (!m_database)
    {
        return;
    }

    m_database->close();
    m_database.reset();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool ChatStorage::isOpen() const { return m_repositories != nullptr; }

bool ChatStorage::configureConnection()
{
    if (!m_database->open())
    {
        qWarning() << "ChatStorage: cannot open" << m_databasePath << m_database->lastError();
        return false;
    }

    Pragma(QStringLiteral("foreign_keys")).set(true).execute(*m_database);
    Pragma(QStringLiteral("journal_mode")).set(QStringLiteral("WAL")).execute(*m_database);
    Pragma(QStringLiteral("busy_timeout")).set(5000).execute(*m_database);
    return true;
}

void ChatStorage::registerMigrations(MigrationRunner& runner)
{
    runner.add(1, [this](QSqlDatabase&) { return m_repositories->createTables(); });
}

bool ChatStorage::migrate()
{
    MigrationRunner runner(*m_storage);
    registerMigrations(runner);
    return runner.run() && schemaVersion() == kSchemaVersion;
}

std::optional<QStringList> ChatStorage::interruptRunning()
{
    const Where runningMessage
        = Where(MessageSerializer::kStatus).equals(sql_codec::toSql(MessageStatus::Running));

    Select interruptedConversations({ MessageSerializer::kConversationId });
    interruptedConversations.from(MessageSerializer::kTable).where(runningMessage);

    const Where interruptedConversation
        = Where(ConversationSerializer::kId).in(interruptedConversations);

    Select sessionsToDrop({ ConversationSerializer::kBackendSession });
    sessionsToDrop.from(ConversationSerializer::kTable)
        .where(Where(interruptedConversation)
                   .and_(ConversationSerializer::kBackendSession)
                   .isNotNull());

    Update clearSessions(ConversationSerializer::kTable);
    clearSessions.set(ConversationSerializer::kBackendSession, QVariant())
        .set(ConversationSerializer::kBackendSessionStart, QVariant())
        .where(interruptedConversation);

    Update interruptMessages(MessageSerializer::kTable);
    interruptMessages.set(MessageSerializer::kStatus, sql_codec::toSql(MessageStatus::Interrupted))
        .where(runningMessage);

    Update failSteps(StepSerializer::kTable);
    failSteps.set(StepSerializer::kStatus, sql_codec::toSql(StepStatus::Failed))
        .set(StepSerializer::kFinishedAt,
             sql_codec::toSql(TimeProvider::instance().currentDateTime()))
        .where(Where(StepSerializer::kStatus).equals(sql_codec::toSql(StepStatus::Running)));

    Transaction transaction(*m_storage);
    if (!transaction.isActive())
    {
        return std::nullopt;
    }

    QStringList dropped;
    for (const QVariantMap& row : m_storage->execute(sessionsToDrop))
    {
        dropped.append(row.value(ConversationSerializer::kBackendSession).toString());
    }

    if (m_storage->execute(clearSessions).toInt() < 0
        || m_storage->execute(interruptMessages).toInt() < 0
        || m_storage->execute(failSteps).toInt() < 0 || !transaction.commit())
    {
        return std::nullopt;
    }
    return dropped;
}

int ChatStorage::schemaVersion() const
{
    if (!m_database)
    {
        return 0;
    }
    const QVector<QVariantMap> rows = Pragma(kSchemaVersionPragma).query(*m_database);
    return rows.isEmpty() ? 0 : rows.first().value(kSchemaVersionPragma).toInt();
}

const QString& ChatStorage::connectionName() const { return m_connectionName; }

const QStringList& ChatStorage::droppedSessions() const { return m_droppedSessions; }

DbStorage& ChatStorage::storage() { return *m_storage; }

ConversationRepository& ChatStorage::conversations() { return m_repositories->conversations; }

MessageRepository& ChatStorage::messages() { return m_repositories->messages; }

}
