#pragma once

#include <QFuture>
#include <QMetaObject>
#include <QObject>
#include <QPromise>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "utils/result.h"

template <typename T> class Task;

namespace task_detail
{
template <typename> struct is_task : std::false_type
{
};
template <typename U> struct is_task<Task<U>> : std::true_type
{
};
template <typename U> inline constexpr bool is_task_v = is_task<U>::value;

template <typename F, typename T> decltype(auto) callWith(F& fn, const Result<T>& result)
{
    if constexpr (std::is_void_v<T>)
    {
        return fn();
    }
    else
    {
        return fn(result.value());
    }
}

template <typename U> Result<U> readyFailure(const QString& error)
{
    if constexpr (std::is_void_v<U>)
    {
        return Result<void>::failure(error);
    }
    else
    {
        return Result<U>::failure(error);
    }
}
}

template <typename T> class Task final
{
public:
    using value_type = T;

    Task(QObject* worker, QFuture<Result<T>> future)
        : m_worker(worker)
        , m_future(std::move(future))
    {
    }

    template <typename Work> static Task<T> run(QObject* worker, Work work)
    {
        static_assert(std::is_same_v<std::invoke_result_t<Work>, Result<T>>,
                      "work passed to Task<T>::run must return Result<T>");

        auto promise = std::make_shared<QPromise<Result<T>>>();
        QFuture<Result<T>> future = promise->future();

        QMetaObject::invokeMethod(worker,
                                  [promise, work = std::move(work)]() mutable
                                  {
                                      promise->start();
                                      promise->addResult(work());
                                      promise->finish();
                                  });

        return Task<T>(worker, std::move(future));
    }

    template <typename F> auto then(F&& fn) { return thenOn(m_worker, std::forward<F>(fn)); }

    template <typename F> auto then(QObject* context, F&& fn)
    {
        return thenOn(context, std::forward<F>(fn));
    }

    Task& onError(QObject* context, std::function<void(const QString&)> handler)
    {
        m_future.then(context,
                      [handler = std::move(handler)](Result<T> result)
                      {
                          if (result.isFailure())
                          {
                              handler(result.error());
                          }
                          return result;
                      });
        return *this;
    }

    QObject* worker() const { return m_worker; }
    QFuture<Result<T>> future() const { return m_future; }

private:
    template <typename F> auto thenOn(QObject* context, F&& fn)
    {
        using Ret = decltype(task_detail::callWith(fn, std::declval<Result<T>>()));
        if constexpr (task_detail::is_task_v<Ret>)
        {
            return thenTaskOn(context, std::forward<F>(fn));
        }
        else
        {
            using U = Ret;
            QFuture<Result<U>> next = m_future.then(
                context,
                [fn = std::forward<F>(fn)](Result<T> result) mutable -> Result<U>
                {
                    if (result.isFailure())
                    {
                        return task_detail::readyFailure<U>(result.error());
                    }
                    if constexpr (std::is_void_v<U>)
                    {
                        task_detail::callWith(fn, result);
                        return Result<void>::success();
                    }
                    else
                    {
                        return Result<U>::success(task_detail::callWith(fn, result));
                    }
                });
            return Task<U>(m_worker, std::move(next));
        }
    }

    template <typename F> auto thenTaskOn(QObject* context, F&& fn)
    {
        using Inner = decltype(task_detail::callWith(fn, std::declval<Result<T>>()));
        using U = typename Inner::value_type;
        QFuture<QFuture<Result<U>>> nested = m_future.then(
            context,
            [fn = std::forward<F>(fn)](Result<T> result) mutable -> QFuture<Result<U>>
            {
                if (result.isFailure())
                {
                    QPromise<Result<U>> promise;
                    QFuture<Result<U>> future = promise.future();
                    promise.start();
                    promise.addResult(task_detail::readyFailure<U>(result.error()));
                    promise.finish();
                    return future;
                }
                return task_detail::callWith(fn, result).future();
            });
        return Task<U>(m_worker, nested.unwrap());
    }

    QObject* m_worker;
    QFuture<Result<T>> m_future;
};
