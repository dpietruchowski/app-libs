#pragma once

#include <QString>
#include <QStringList>
#include <memory>
#include <optional>

class DbStorage;
class MigrationRunner;
class QSqlDatabase;

namespace chat
{

class ConversationRepository;
class MessageRepository;

class ChatStorage final
{
public:
    static constexpr int kSchemaVersion = 1;

    ChatStorage(const QString& databasePath, const QString& appName);
    ~ChatStorage();

    ChatStorage(const ChatStorage&) = delete;
    ChatStorage& operator=(const ChatStorage&) = delete;

    bool open();
    void close();
    bool isOpen() const;

    int schemaVersion() const;
    const QString& connectionName() const;
    const QStringList& droppedSessions() const;

    DbStorage& storage();
    ConversationRepository& conversations();
    MessageRepository& messages();

private:
    struct Repositories;

    bool configureConnection();
    void registerMigrations(MigrationRunner& runner);
    bool migrate();
    std::optional<QStringList> interruptRunning();

    QString m_databasePath;
    QString m_connectionName;
    QStringList m_droppedSessions;
    std::unique_ptr<QSqlDatabase> m_database;
    std::unique_ptr<DbStorage> m_storage;
    std::unique_ptr<Repositories> m_repositories;
};

}
