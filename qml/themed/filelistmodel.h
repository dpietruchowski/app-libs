#pragma once

#include <QAbstractListModel>
#include <QFileInfoList>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

class FileListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged)
    Q_PROPERTY(QString folderName READ folderName NOTIFY folderChanged)
    Q_PROPERTY(bool atRoot READ atRoot NOTIFY folderChanged)
    Q_PROPERTY(QStringList nameFilters READ nameFilters WRITE setNameFilters NOTIFY
                   nameFiltersChanged)

public:
    enum Role
    {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IsDirRole
    };

    explicit FileListModel(QObject* parent = nullptr);

    QString folder() const;
    void setFolder(const QString& folder);
    QString folderName() const;
    bool atRoot() const;
    QStringList nameFilters() const;
    void setNameFilters(const QStringList& filters);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void cdUp();
    Q_INVOKABLE QString homeFolder() const;

signals:
    void folderChanged();
    void nameFiltersChanged();

private:
    void reload();

    QString m_folder;
    QStringList m_nameFilters;
    QFileInfoList m_entries;
};
