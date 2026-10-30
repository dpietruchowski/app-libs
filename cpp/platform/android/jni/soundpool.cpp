#include "soundpool.h"

namespace
{

constexpr int UsageGame = 14;
constexpr int ContentTypeSonification = 4;
constexpr int Priority = 1;
constexpr int NoLoop = 0;

}  // namespace

namespace android
{

SoundPool SoundPool::create(int maxStreams)
{
    QJniObject attributesBuilder("android/media/AudioAttributes$Builder");
    attributesBuilder.callObjectMethod("setUsage", "(I)Landroid/media/AudioAttributes$Builder;",
                                       static_cast<jint>(UsageGame));
    attributesBuilder.callObjectMethod("setContentType",
                                       "(I)Landroid/media/AudioAttributes$Builder;",
                                       static_cast<jint>(ContentTypeSonification));
    const QJniObject attributes
        = attributesBuilder.callObjectMethod("build", "()Landroid/media/AudioAttributes;");

    QJniObject poolBuilder("android/media/SoundPool$Builder");
    poolBuilder.callObjectMethod("setMaxStreams", "(I)Landroid/media/SoundPool$Builder;",
                                 static_cast<jint>(maxStreams));
    poolBuilder.callObjectMethod(
        "setAudioAttributes", "(Landroid/media/AudioAttributes;)Landroid/media/SoundPool$Builder;",
        attributes.object());
    return SoundPool(poolBuilder.callObjectMethod("build", "()Landroid/media/SoundPool;"));
}

SoundPool::SoundPool(QJniObject jni)
    : m_pool(std::move(jni))
{
}

bool SoundPool::isValid() const { return m_pool.isValid(); }

int SoundPool::load(const QString& filePath) const
{
    return m_pool.callMethod<jint>("load", "(Ljava/lang/String;I)I",
                                   QJniObject::fromString(filePath).object<jstring>(),
                                   static_cast<jint>(Priority));
}

bool SoundPool::play(int soundId, float leftVolume, float rightVolume, float rate) const
{
    const jint streamId
        = m_pool.callMethod<jint>("play", "(IFFIIF)I", static_cast<jint>(soundId), leftVolume,
                                  rightVolume, static_cast<jint>(Priority),
                                  static_cast<jint>(NoLoop), rate);
    return streamId != 0;
}

void SoundPool::release()
{
    if (m_pool.isValid())
        m_pool.callMethod<void>("release");
}

}  // namespace android
