#pragma once
#include "dbrepository.h"
#include "dbstorage.h"
#include "query/order.h"
#include "query/where.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class ModelRepositoryBase
{
};

template <typename ModelType> class ModelRepository : public ModelRepositoryBase
{
public:
    ModelRepository(DbStorage& storage, const QString& tableName, const QStringList& keys)
        : m_repo(tableName, keys, storage)
    {
    }

    virtual ~ModelRepository() = default;

    QVector<ModelType*> selectModels(const Where& condition = Where(),
                                     QObject* parent = nullptr) const
    {
        auto maps = m_repo.select(condition, Order(), -1, -1);
        return convertToModels(maps, parent);
    }

    QVector<ModelType*> selectModels(const Where& condition, const Order& order, int limit = -1,
                                     int offset = -1, QObject* parent = nullptr) const
    {
        auto maps = m_repo.select(condition, order, limit, offset);
        return convertToModels(maps, parent);
    }

    ModelType* selectFirstModel(const Where& condition = Where(), QObject* parent = nullptr) const
    {
        auto maps = m_repo.select(condition, Order(), 1);
        if (maps.isEmpty())
            return nullptr;
        return ModelType::fromVariantMap(maps.first(), parent);
    }

    int insertModel(const ModelType* model) { return m_repo.insert(model->toVariantMap()); }

    int updateModel(const ModelType* model, const Where& condition = Where())
    {
        return m_repo.update(model->toVariantMap(), condition);
    }

    int upsertModel(const ModelType* model) { return m_repo.upsert(model->toVariantMap()); }

    int upsertAllModels(const QVector<ModelType*>& models)
    {
        QVector<QVariantMap> maps;
        for (const auto* model : models)
        {
            maps.append(model->toVariantMap());
        }
        return m_repo.upsertAll(maps);
    }

    bool existsModel(const Where& condition) const
    {
        return m_repo.exists(condition);
    }

    int countModels(const Where& condition = Where()) const
    {
        return m_repo.count(condition);
    }

    int removeModels(const Where& condition)
    {
        return m_repo.remove(condition);
    }

    void clearTable()
    {
        m_repo.clearTable();
    }

protected:
    DbStorage& storage() { return m_repo.storage(); }
    const DbStorage& storage() const { return m_repo.storage(); }

    DbRepository& repository() { return m_repo; }
    const DbRepository& repository() const { return m_repo; }

private:
    DbRepository m_repo;

    QVector<ModelType*> convertToModels(const QVector<QVariantMap>& maps, QObject* parent) const
    {
        QVector<ModelType*> models;
        for (const auto& map : maps)
        {
            models.append(ModelType::fromVariantMap(map, parent));
        }
        return models;
    }
};
