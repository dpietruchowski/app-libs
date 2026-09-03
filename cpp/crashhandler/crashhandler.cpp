#include "crashhandler.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <ucontext.h>
#include <unistd.h>

#ifdef CRASH_HANDLER_STD_STACKTRACE
#include <stacktrace>
#include <string>
#elif !defined(__ANDROID__)
#include <execinfo.h>
#endif

namespace
{
constexpr size_t kAlternateStackSize = 256 * 1024;
[[maybe_unused]] constexpr int kMaxFrames = 64;

int crashLogFd = -1;
int probeFd[2] = { -1, -1 };
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
#elif defined(__ANDROID__)
    report("stack trace unavailable (no execinfo on bionic)\n");
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

[[maybe_unused]] bool readable(const void* address)
{
    if (probeFd[1] < 0 || address == nullptr)
    {
        return false;
    }
    return ::write(probeFd[1], address, sizeof(void*)) == static_cast<ssize_t>(sizeof(void*));
}

void* programCounter(const ucontext_t* context)
{
#if defined(__x86_64__)
    return reinterpret_cast<void*>(context->uc_mcontext.gregs[REG_RIP]);
#elif defined(__aarch64__)
    return reinterpret_cast<void*>(context->uc_mcontext.pc);
#elif defined(__arm__)
    return reinterpret_cast<void*>(context->uc_mcontext.arm_pc);
#else
    (void)context;
    return nullptr;
#endif
}

void* stackPointer(const ucontext_t* context)
{
#if defined(__x86_64__)
    return reinterpret_cast<void*>(context->uc_mcontext.gregs[REG_RSP]);
#elif defined(__aarch64__)
    return reinterpret_cast<void*>(context->uc_mcontext.sp);
#elif defined(__arm__)
    return reinterpret_cast<void*>(context->uc_mcontext.arm_sp);
#else
    (void)context;
    return nullptr;
#endif
}

void* returnAddress(const ucontext_t* context)
{
#if defined(__aarch64__)
    return reinterpret_cast<void*>(context->uc_mcontext.regs[30]);
#elif defined(__arm__)
    return reinterpret_cast<void*>(context->uc_mcontext.arm_lr);
#else
    void* stack = stackPointer(context);
    return readable(stack) ? *static_cast<void* const*>(stack) : nullptr;
#endif
}

void reportOrigin(void* address)
{
    Dl_info info;
    if (::dladdr(address, &info) == 0 || info.dli_fname == nullptr)
    {
        return;
    }

    char line[512];
    const auto offset = static_cast<const char*>(address) - static_cast<const char*>(info.dli_fbase);
    if (info.dli_sname != nullptr)
    {
        std::snprintf(line, sizeof(line), "     in %s (%s+0x%lx)\n", info.dli_sname, info.dli_fname,
                      static_cast<unsigned long>(offset));
    }
    else
    {
        std::snprintf(line, sizeof(line), "     in %s+0x%lx\n", info.dli_fname,
                      static_cast<unsigned long>(offset));
    }
    report(line);
}

void reportFault(const siginfo_t* info, void* context)
{
    if (info == nullptr || context == nullptr)
    {
        return;
    }

    const auto* userContext = static_cast<const ucontext_t*>(context);
    void* counter = programCounter(userContext);
    char line[256];
    std::snprintf(line, sizeof(line), "fault address %p, pc %p, sp %p\n", info->si_addr, counter,
                  stackPointer(userContext));
    report(line);

    if (counter == nullptr || info->si_addr != counter)
    {
        return;
    }

    report("jumped to an unmapped address, the stack trace below stops here\n");
    void* caller = returnAddress(userContext);
    if (caller == nullptr)
    {
        return;
    }

    std::snprintf(line, sizeof(line), "called from %p\n", caller);
    report(line);
    reportOrigin(caller);
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

void handleSignal(int signalNumber, siginfo_t* info, void* context)
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
    reportFault(info, context);
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

    if (::pipe(probeFd) != 0)
    {
        probeFd[0] = -1;
        probeFd[1] = -1;
    }
    else
    {
        ::fcntl(probeFd[1], F_SETFL, O_NONBLOCK);
    }

    stack_t alternate = {};
    alternate.ss_sp = alternateStack;
    alternate.ss_size = sizeof(alternateStack);
    ::sigaltstack(&alternate, nullptr);

    struct sigaction action = {};
    action.sa_sigaction = handleSignal;
    ::sigemptyset(&action.sa_mask);
    action.sa_flags = SA_ONSTACK | SA_RESTART | SA_SIGINFO;

    for (const int signalNumber : { SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL })
    {
        ::sigaction(signalNumber, &action, nullptr);
    }

    std::set_terminate(handleTerminate);
}
}
