#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSemaphore>
#include <QThread>
#include <atomic>
#include <functional>
#include <gtest/gtest.h>
#include <memory>

#include "async/backendworker.h"
#include "async/task.h"

namespace
{
template <typename T> void discard(T&&) { }

bool waitUntil(const std::function<bool()>& condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 5000)
    {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return condition();
}

void settle(BackendWorker& worker)
{
    worker.drain();
    for (int i = 0; i < 20; ++i)
    {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
}

class GatedTask
{
public:
    explicit GatedTask(BackendWorker& worker)
        : m_task(Task<int>::run(&worker,
                                [gate = &m_gate]
                                {
                                    gate->acquire();
                                    return Result<int>::success(7);
                                }))
    {
    }

    Task<int>& task() { return m_task; }
    void open() { m_gate.release(); }

private:
    QSemaphore m_gate;
    Task<int> m_task;
};

class EventThread
{
public:
    EventThread() { m_thread.start(); }
    ~EventThread()
    {
        m_thread.quit();
        m_thread.wait();
    }

    QThread* thread() { return &m_thread; }

private:
    QThread m_thread;
};
}

TEST(TaskThenTest, Then_WithALiveContext_RunsOnTheThreadOfTheContext)
{
    BackendWorker worker;
    QObject context;
    GatedTask gated(worker);
    std::atomic<QThread*> ranOn { nullptr };
    std::atomic<int> received { 0 };

    discard(gated.task().then(&context,
                              [&](int value)
                              {
                                  ranOn = QThread::currentThread();
                                  received = value;
                              }));
    gated.open();

    ASSERT_TRUE(waitUntil([&] { return ranOn.load() != nullptr; }));
    EXPECT_EQ(ranOn.load(), QThread::currentThread());
    EXPECT_EQ(received.load(), 7);
}

TEST(TaskThenTest, Then_WithAContextOnAnotherThread_RunsOnThatThread)
{
    BackendWorker worker;
    EventThread eventThread;
    QObject context;
    context.moveToThread(eventThread.thread());
    GatedTask gated(worker);
    std::atomic<QThread*> ranOn { nullptr };

    discard(gated.task().then(&context, [&](int) { ranOn = QThread::currentThread(); }));
    gated.open();

    ASSERT_TRUE(waitUntil([&] { return ranOn.load() != nullptr; }));
    EXPECT_EQ(ranOn.load(), eventThread.thread());
}

TEST(TaskThenTest, Then_WithoutAContext_RunsOnTheWorkerThread)
{
    BackendWorker worker;
    GatedTask gated(worker);
    std::atomic<bool> onWorkerThread { false };
    std::atomic<bool> ran { false };

    discard(gated.task().then(
        [&](int)
        {
            onWorkerThread = QThread::currentThread() == worker.thread();
            ran = true;
        }));
    gated.open();

    ASSERT_TRUE(waitUntil([&] { return ran.load(); }));
    EXPECT_TRUE(onWorkerThread.load());
}

TEST(TaskThenTest, Then_WhenTheContextDiesBeforeTheFutureFinishes_SkipsTheContinuation)
{
    BackendWorker worker;
    auto* context = new QObject;
    GatedTask gated(worker);
    bool ran = false;

    discard(gated.task().then(context,
                              [&](int)
                              {
                                  ran = true;
                                  return 1;
                              }));
    delete context;
    gated.open();
    settle(worker);

    EXPECT_FALSE(ran);
}

TEST(TaskThenTest, Then_WhenTheContextDiesBeforeTheFutureFinishes_ReturnsACancelledTask)
{
    BackendWorker worker;
    auto* context = new QObject;
    GatedTask gated(worker);

    Task<int> next = gated.task().then(context, [](int value) { return value + 1; });
    delete context;
    gated.open();
    settle(worker);

    EXPECT_TRUE(next.future().isFinished());
    EXPECT_TRUE(next.future().isCanceled());
}

TEST(TaskThenTest, Then_ChainedAfterADeadContext_SkipsEveryLaterContinuation)
{
    BackendWorker worker;
    auto* context = new QObject;
    QObject survivor;
    GatedTask gated(worker);
    bool firstRan = false;
    bool secondRan = false;

    discard(gated.task()
                .then(context,
                      [&](int value)
                      {
                          firstRan = true;
                          return value;
                      })
                .then(&survivor,
                      [&](int)
                      {
                          secondRan = true;
                          return 0;
                      }));
    delete context;
    gated.open();
    settle(worker);

    EXPECT_FALSE(firstRan);
    EXPECT_FALSE(secondRan);
}

TEST(TaskThenTest, Then_Chained_PassesTheValueThroughEveryLiveContext)
{
    BackendWorker worker;
    QObject first;
    QObject second;
    GatedTask gated(worker);
    std::atomic<int> received { 0 };

    discard(gated.task()
                .then(&first, [](int value) { return value * 2; })
                .then(&second, [&](int value) { received = value; }));
    gated.open();

    ASSERT_TRUE(waitUntil([&] { return received.load() != 0; }));
    EXPECT_EQ(received.load(), 14);
}

TEST(TaskThenTest, Then_ReturningATask_ContinuesWithTheInnerResult)
{
    BackendWorker worker;
    QObject context;
    GatedTask gated(worker);
    std::atomic<int> received { 0 };

    discard(gated.task()
                .then(&context, [&](int value)
                      { return Task<int>::ready(&worker, Result<int>::success(value + 1)); })
                .then(&context, [&](int value) { received = value; }));
    gated.open();

    ASSERT_TRUE(waitUntil([&] { return received.load() != 0; }));
    EXPECT_EQ(received.load(), 8);
}

TEST(TaskThenTest, Then_ReturningATaskWhenTheContextDiesFirst_SkipsTheContinuation)
{
    BackendWorker worker;
    auto* context = new QObject;
    GatedTask gated(worker);
    bool ran = false;

    discard(gated.task().then(context,
                              [&](int value)
                              {
                                  ran = true;
                                  return Task<int>::ready(&worker, Result<int>::success(value));
                              }));
    delete context;
    gated.open();
    settle(worker);

    EXPECT_FALSE(ran);
}

TEST(TaskThenTest, Finally_WhenTheContextDiesFirst_SkipsTheHandler)
{
    BackendWorker worker;
    auto* context = new QObject;
    GatedTask gated(worker);
    bool ran = false;

    gated.task().finally(context, [&](Result<int>) { ran = true; });
    delete context;
    gated.open();
    settle(worker);

    EXPECT_FALSE(ran);
}

TEST(TaskThenTest, OnError_WhenTheContextDiesFirst_SkipsTheHandler)
{
    BackendWorker worker;
    auto* context = new QObject;
    QSemaphore gate;
    Task<int> failing = Task<int>::run(&worker,
                                       [&gate]
                                       {
                                           gate.acquire();
                                           return Result<int>::failure(QStringLiteral("boom"));
                                       });
    bool ran = false;

    failing.onError(context, [&](const QString&) { ran = true; });
    delete context;
    gate.release();
    settle(worker);

    EXPECT_FALSE(ran);
}

TEST(TaskThenTest, OnError_WithALiveContext_ReportsTheError)
{
    BackendWorker worker;
    QObject context;
    QSemaphore gate;
    Task<int> failing = Task<int>::run(&worker,
                                       [&gate]
                                       {
                                           gate.acquire();
                                           return Result<int>::failure(QStringLiteral("boom"));
                                       });
    QString reported;

    failing.onError(&context, [&](const QString& error) { reported = error; });
    gate.release();

    ASSERT_TRUE(waitUntil([&] { return !reported.isEmpty(); }));
    EXPECT_EQ(reported, QStringLiteral("boom"));
}

TEST(TaskThenTest, Then_AfterAnEarlierThreadWasDestroyed_StillReachesTheNewThread)
{
    BackendWorker worker;
    for (int round = 0; round < 5; ++round)
    {
        auto eventThread = std::make_unique<EventThread>();
        QObject context;
        context.moveToThread(eventThread->thread());
        GatedTask gated(worker);
        std::atomic<bool> ran { false };

        discard(gated.task().then(&context, [&](int) { ran = true; }));
        gated.open();

        ASSERT_TRUE(waitUntil([&] { return ran.load(); })) << "round " << round;
    }
}
