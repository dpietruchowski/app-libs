#include "crashhandler.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#ifdef CRASH_HANDLER_STD_STACKTRACE
#include <stacktrace>
#include <string>
#else
#include <execinfo.h>
#endif

namespace
{
constexpr size_t kAlternateStackSize = 256 * 1024;
constexpr int kMaxFrames = 64;

int crashLogFd = -1;
std::atomic_flag crashInProgress = ATOMIC_FLAG_INIT;
char alternateStack[kAlternateStackSize];

void writeToFd(int fd, const char* text, size_t length)
{
    while (length > 0)
    {
        const ssize_t written = ::write(fd, text, length);
        if (written <= 0)
        {
            return;
        }
        text += written;
        length -= static_cast<size_t>(written);
    }
}

void report(const char* text)
{
    const size_t length = std::strlen(text);
    writeToFd(STDERR_FILENO, text, length);
    if (crashLogFd >= 0)
    {
        writeToFd(crashLogFd, text, length);
    }
}

void reportStackTrace()
{
#ifdef CRASH_HANDLER_STD_STACKTRACE
    report(std::to_string(std::stacktrace::current(2)).c_str());
    report("\n");
#else
    void* frames[kMaxFrames];
    const int count = ::backtrace(frames, kMaxFrames);
    ::backtrace_symbols_fd(frames, count, STDERR_FILENO);
    if (crashLogFd >= 0)
    {
        ::backtrace_symbols_fd(frames, count, crashLogFd);
    }
#endif
}

const char* signalName(int signalNumber)
{
    switch (signalNumber)
    {
        case SIGSEGV:
            return "SIGSEGV";
        case SIGABRT:
            return "SIGABRT";
        case SIGBUS:
            return "SIGBUS";
        case SIGFPE:
            return "SIGFPE";
        case SIGILL:
            return "SIGILL";
        default:
            return "signal";
    }
}

void reportHeader(const char* reason, const char* detail)
{
    char threadName[32] = {};
    ::pthread_getname_np(::pthread_self(), threadName, sizeof(threadName));

    char header[256];
    std::snprintf(header, sizeof(header), "\n=== CRASH: %s (%s) in thread \"%s\" ===\n", reason,
                  detail, threadName);
    report(header);
}

void handleSignal(int signalNumber)
{
    if (crashInProgress.test_and_set())
    {
        std::signal(signalNumber, SIG_DFL);
        std::raise(signalNumber);
        return;
    }

    char detail[32];
    std::snprintf(detail, sizeof(detail), "signal %d", signalNumber);
    reportHeader(signalName(signalNumber), detail);
    reportStackTrace();

    std::signal(signalNumber, SIG_DFL);
    std::raise(signalNumber);
}

void handleTerminate()
{
    if (!crashInProgress.test_and_set())
    {
        const char* detail = "no active exception";
        if (std::current_exception())
        {
            detail = "unknown exception";
            try
            {
                std::rethrow_exception(std::current_exception());
            }
            catch (const std::exception& exception)
            {
                detail = exception.what();
            }
            catch (...)
            {
            }
        }

        reportHeader("std::terminate", detail);
        reportStackTrace();
    }

    std::signal(SIGABRT, SIG_DFL);
    std::abort();
}
}

namespace crash_handler
{
void install(const QString& logPath)
{
    if (!logPath.isEmpty())
    {
        const QByteArray path = logPath.toLocal8Bit();
        crashLogFd = ::open(path.constData(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    }

    stack_t alternate = {};
    alternate.ss_sp = alternateStack;
    alternate.ss_size = sizeof(alternateStack);
    ::sigaltstack(&alternate, nullptr);

    struct sigaction action = {};
    action.sa_handler = handleSignal;
    ::sigemptyset(&action.sa_mask);
    action.sa_flags = SA_ONSTACK | SA_RESTART;

    for (const int signalNumber : { SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL })
    {
        ::sigaction(signalNumber, &action, nullptr);
    }

    std::set_terminate(handleTerminate);
}
}
