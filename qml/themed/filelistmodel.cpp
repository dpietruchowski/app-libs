#include "filelistmodel.h"

#include <QDir>
#include <QStandardPaths>

FileListModel::FileListModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_folder(homeFolder())
{
    reload();
}

QString FileListModel::folder() const { return m_folder; }

void FileListModel::setFolder(const QString& folder)
{
    if (folder.isEmpty() || folder == m_folder)
        return;

    QDir dir(folder);
    if (!dir.exists())
        return;

    m_folder = dir.absolutePath();
    reload();
    emit folderChanged();
}

QString FileListModel::folderName() const { return QDir(m_folder).dirName(); }

bool FileListModel::atRoot() const { return QDir(m_folder).isRoot(); }

QStringList FileListModel::nameFilters() const { return m_nameFilters; }

void FileListModel::setNameFilters(const QStringList& filters)
{
    if (filters == m_nameFilters)
        return;

    m_nameFilters = filters;
    reload();
    emit nameFiltersChanged();
}

void FileListModel::reload()
{
    beginResetModel();
    QDir dir(m_folder);
    dir.setSorting(QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);

    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);
    QFileInfoList files = dir.entryInfoList(m_nameFilters, QDir::Files | QDir::Readable);

    m_entries = dirs;
    m_entries.append(files);
    endResetModel();
}

int FileListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_entries.size());
}

QVariant FileListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return QVariant();

    const QFileInfo& info = m_entries[index.row()];
    switch (role)
    {
        case NameRole:
            return info.fileName();
        case PathRole:
            return info.absoluteFilePath();
        case IsDirRole:
            return info.isDir();
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[PathRole] = "path";
    roles[IsDirRole] = "isDir";
    return roles;
}

void FileListModel::cdUp()
{
    QDir dir(m_folder);
    if (dir.cdUp())
        setFolder(dir.absolutePath());
}

QString FileListModel::homeFolder() const
{
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!documents.isEmpty() && QDir(documents).exists())
        return documents;
    return QDir::homePath();
}
