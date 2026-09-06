#include "transaction.h"

#include "dbstorage.h"

Transaction::Transaction(DbStorage& storage)
    : m_storage(storage)
    , m_active(storage.beginTransaction())
{
}

Transaction::~Transaction()
{
    if (m_active)
    {
        m_storage.rollback();
    }
}

bool Transaction::isActive() const { return m_active; }

bool Transaction::commit()
{
    if (!m_active)
    {
        return false;
    }

    m_active = false;
    return m_storage.commit();
}
