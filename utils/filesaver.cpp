#include "filesaver.h"

#if defined(Q_OS_ANDROID)
#include "android/filesaverandroid.h"
using FileSaverBackend = FileSaverAndroid;
#else
#include "desktop/filesaverqt.h"
using FileSaverBackend = FileSaverQt;
#endif

class FileSaver::Impl
{
public:
    explicit Impl(FileSaver& owner)
        : backend([&owner](const QString& location) { emit owner.saved(location); },
                  [&owner]() { emit owner.cancelled(); },
                  [&owner](const QString& message) { emit owner.failed(message); })
    {
    }

    FileSaverBackend backend;
};

FileSaver::FileSaver(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>(*this))
{
}

FileSaver::~FileSaver() = default;

void FileSaver::save(const QString& suggestedName, const QString& mimeType, const QByteArray& data)
{
    m_impl->backend.launch(suggestedName, mimeType, data);
}
