#include "fileopener.h"

#if defined(Q_OS_ANDROID)
#include "android/fileopenerandroid.h"
using FileOpenerBackend = FileOpenerAndroid;
#else
#include "desktop/fileopenerqt.h"
using FileOpenerBackend = FileOpenerQt;
#endif

class FileOpener::Impl
{
public:
    explicit Impl(FileOpener& owner)
        : backend([&owner](const QString& location, const QByteArray& data)
                  { emit owner.opened(location, data); }, [&owner]() { emit owner.cancelled(); },
                  [&owner](const QString& message) { emit owner.failed(message); })
    {
    }

    FileOpenerBackend backend;
};

FileOpener::FileOpener(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>(*this))
{
}

FileOpener::~FileOpener() = default;

void FileOpener::setQmlEngine(QQmlEngine* engine) { FileOpenerBackend::setQmlEngine(engine); }

void FileOpener::open(const QString& mimeType) { m_impl->backend.launch(mimeType); }