#include "soundpoolandroid.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace
{

constexpr int FailedLoad = 0;
const QString ResourceDirectory = QStringLiteral("soundpool");

bool isResource(const QString& path)
{
    return path.startsWith(QStringLiteral(":/")) || path.startsWith(QStringLiteral("qrc:/"));
}

QString resourceFilePath(const QString& path)
{
    return path.startsWith(QStringLiteral("qrc:/")) ? path.mid(3) : path;
}

}  // namespace

SoundPoolAndroid::SoundPoolAndroid(int maxStreams)
    : m_pool(android::SoundPool::create(maxStreams))
{
}

SoundPoolAndroid::~SoundPoolAndroid() { m_pool.release(); }

bool SoundPoolAndroid::isAvailable() const { return m_pool.isValid(); }

int SoundPoolAndroid::load(const QString& path)
{
    if (!m_pool.isValid())
        return FailedLoad;

    const QString file = readableFile(path);
    if (file.isEmpty())
        return FailedLoad;

    return m_pool.load(file);
}

bool SoundPoolAndroid::play(int soundId, float volume, float rate)
{
    return m_pool.isValid() && soundId > 0 && m_pool.play(soundId, volume, volume, rate);
}

QString SoundPoolAndroid::readableFile(const QString& path)
{
    if (!isResource(path))
        return path;

    const QString directory = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                                  .filePath(ResourceDirectory);
    if (!QDir().mkpath(directory))
        return {};

    QString name = path;
    name.replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_'));
    const QString target = QDir(directory).filePath(name);

    QFile::remove(target);
    if (!QFile::copy(resourceFilePath(path), target))
        return {};
    return target;
}
