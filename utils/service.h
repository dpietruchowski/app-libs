#pragma once

#include <QObject>
#include <type_traits>
#include <utility>

#include "utils/task.h"

class Service
{
protected:
    explicit Service(QObject* worker)
        : m_worker(worker)
    {
    }
    ~Service() = default;

    template <typename Work> auto invoke(Work work)
    {
        using T = typename std::invoke_result_t<Work>::value_type;
        return Task<T>::run(m_worker, std::move(work));
    }

private:
    QObject* m_worker;
};
