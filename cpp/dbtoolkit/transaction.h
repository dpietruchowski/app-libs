#pragma once

class DbStorage;

class Transaction
{
public:
    explicit Transaction(DbStorage& storage);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    bool isActive() const;
    bool commit();

private:
    DbStorage& m_storage;
    bool m_active;
};
