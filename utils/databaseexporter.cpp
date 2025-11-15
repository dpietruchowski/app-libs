#include "databaseexporter.h"
#include "storage/fillindbstorage.h"

#include <QDebug>
#include <QDir>

DatabaseExporter::DatabaseExporter(FillInDbStorage* storage, QObject* parent)
    : QObject(parent)
    , m_storage(storage)
{
}

void DatabaseExporter::exportDatabase()
{
    if (!m_storage)
    {
        emit exportFailed("Storage not initialized");
        return;
    }

    if (!m_storage->isOpen())
    {
        emit exportFailed("Database is not open");
        return;
    }

    m_dbPath = m_storage->databasePath();
    if (m_dbPath.isEmpty() || !QFile::exists(m_dbPath))
    {
        emit exportFailed("Database file not found: " + m_dbPath);
        return;
    }

#ifdef Q_OS_ANDROID
    if (exportViaMediaStore())
    {
        emit exportSuccess("/sdcard/Download/fillin_export_*.db");
    }
    else
    {
        emit exportFailed("Failed to export via MediaStore");
    }
#else
    QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadsPath.isEmpty())
    {
        emit exportFailed("Downloads folder not available");
        return;
    }

    QDir().mkpath(downloadsPath);

    QString exportPath = downloadsPath + "/fillin_export_"
        + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".db";

    if (QFile::exists(exportPath))
        QFile::remove(exportPath);

    if (QFile::copy(m_dbPath, exportPath))
        emit exportSuccess(exportPath);
    else
        emit exportFailed("Failed to copy database to Downloads");
#endif
}

#ifdef Q_OS_ANDROID
bool DatabaseExporter::exportViaMediaStore()
{
    QFile dbFile(m_dbPath);
    if (!dbFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Cannot open database file for reading";
        return false;
    }

    QByteArray fileData = dbFile.readAll();
    dbFile.close();

    if (fileData.isEmpty())
    {
        qWarning() << "Database file is empty";
        return false;
    }

    QString fileName = "fillin_export_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".db";

    QJniEnvironment env;

    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        "()Landroid/app/Activity;"
    );

    if (!activity.isValid())
    {
        qWarning() << "Cannot get Android activity";
        return false;
    }

    QJniObject contentResolver = activity.callObjectMethod(
        "getContentResolver",
        "()Landroid/content/ContentResolver;"
    );

    QJniObject contentValues("android/content/ContentValues");
    contentValues.callMethod<void>(
        "put",
        "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString("_display_name").object(),
        QJniObject::fromString(fileName).object()
    );

    contentValues.callMethod<void>(
        "put",
        "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString("mime_type").object(),
        QJniObject::fromString("application/octet-stream").object()
    );

    QJniObject relativePathValue = QJniObject::fromString("Download/");
    contentValues.callMethod<void>(
        "put",
        "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString("relative_path").object(),
        relativePathValue.object()
    );

    QJniObject externalContentUri = QJniObject::getStaticObjectField(
        "android/provider/MediaStore$Downloads",
        "EXTERNAL_CONTENT_URI",
        "Landroid/net/Uri;"
    );

    if (!externalContentUri.isValid())
    {
        qWarning() << "Cannot get MediaStore Downloads URI";
        return false;
    }

    QJniObject uri = contentResolver.callObjectMethod(
        "insert",
        "(Landroid/net/Uri;Landroid/content/ContentValues;)Landroid/net/Uri;",
        externalContentUri.object(),
        contentValues.object()
    );

    if (!uri.isValid())
    {
        qWarning() << "Failed to insert into MediaStore";
        return false;
    }

    QJniObject outputStream = contentResolver.callObjectMethod(
        "openOutputStream",
        "(Landroid/net/Uri;)Ljava/io/OutputStream;",
        uri.object()
    );

    if (!outputStream.isValid())
    {
        qWarning() << "Cannot open output stream";
        return false;
    }

    jbyteArray javaByteArray = env->NewByteArray(fileData.size());
    env->SetByteArrayRegion(javaByteArray, 0, fileData.size(),
                            reinterpret_cast<const jbyte*>(fileData.constData()));

    outputStream.callMethod<void>("write", "([B)V", javaByteArray);
    outputStream.callMethod<void>("flush", "()V");
    outputStream.callMethod<void>("close", "()V");

    env->DeleteLocalRef(javaByteArray);

    qDebug() << "Database exported successfully to public Downloads folder";
    return true;
}
#endif
