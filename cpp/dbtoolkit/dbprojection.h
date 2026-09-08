#pragma once
#include <QList>
#include <QMap>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "dbstorage.h"
#include "query/projection.h"
#include "query/select.h"

template <typename Row> class DbProjection
{
public:
    using Mapper = std::function<Row(const ProjectedRow&)>;

    DbProjection(DbStorage& storage, const QList<Projection>& projections, Mapper mapper)
        : m_storage(storage)
        , m_projections(projections)
        , m_mapper(mapper)
    {
    }

    std::vector<Row> findAll(const Select& shape) const
    {
        std::vector<Row> rows;
        for (const QVariantMap& row : m_storage.execute(projected(shape)))
        {
            rows.push_back(m_mapper(split(row)));
        }
        return rows;
    }

    std::optional<Row> findFirst(const Select& shape) const
    {
        Select first = shape;
        first.limit(1);

        std::vector<Row> rows = findAll(first);
        if (rows.empty())
        {
            return std::nullopt;
        }
        return std::move(rows.front());
    }

    int count(const Select& shape) const
    {
        Select counted(QStringList { "COUNT(*)" });
        counted.from(counting(shape));

        const QVector<QVariantMap> rows = m_storage.execute(counted);
        return rows.isEmpty() ? 0 : rows.first().value("COUNT(*)").toInt();
    }

    bool exists(const Select& shape) const { return count(shape) > 0; }

    Select projected(const Select& shape) const
    {
        Select select = shape;
        select.setColumns(QStringList());
        select.clearProjected();
        select.projected(selectExpressions());
        return select;
    }

    Select counting(const Select& shape) const
    {
        Select select = shape;
        select.setColumns(QStringList());
        select.clearProjected();
        select.projected(QStringList { "1" });
        select.orderBy(QString());
        return select;
    }

    QStringList selectExpressions() const
    {
        QStringList expressions;
        for (const Projection& projection : m_projections)
        {
            expressions.append(projection.selectExpressions());
        }
        return expressions;
    }

private:
    ProjectedRow split(const QVariantMap& row) const
    {
        QMap<QString, QVariantMap> groups;
        for (const Projection& projection : m_projections)
        {
            groups.insert(projection.group(), projection.extract(row));
        }
        return ProjectedRow(groups);
    }

    DbStorage& m_storage;
    QList<Projection> m_projections;
    Mapper m_mapper;
};
