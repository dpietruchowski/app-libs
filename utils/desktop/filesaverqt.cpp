#include "filesaverqt.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QStandardPaths>

namespace
{
QString uniquePath(const QString& directory, const QString& fileName)
{
    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : "." + info.suffix();

    QString candidate = QDir(directory).filePath(fileName);
    int counter = 1;
    while (QFile::exists(candidate))
    {
        candidate
            = QDir(directory).filePath(QString("%1 (%2)%3").arg(base).arg(counter).arg(suffix));
        ++counter;
    }
    return candidate;
}
}  // namespace

FileSaverQt::FileSaverQt(SavedCallback onSaved, CancelledCallback onCancelled,
                         FailedCallback onFailed)
    : m_onSaved(std::move(onSaved))
    , m_onCancelled(std::move(onCancelled))
    , m_onFailed(std::move(onFailed))
{
}

void FileSaverQt::launch(const QString& suggestedName, const QString& mimeType,
                         const QByteArray& data)
{
    Q_UNUSED(mimeType);

    const QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (directory.isEmpty() || !QDir().mkpath(directory))
    {
        m_onFailed(QObject::tr("Could not resolve a writable documents location"));
        return;
    }

    const QString path = uniquePath(directory, suggestedName);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        m_onFailed(file.errorString());
        return;
    }
    if (file.write(data) != data.size())
    {
        m_onFailed(file.errorString());
        return;
    }
    file.close();
    m_onSaved(path);
}
