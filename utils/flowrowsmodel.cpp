#include "flowrowsmodel.h"

#include <QFont>

FlowRowsModel::FlowRowsModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int FlowRowsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_layout.rowCount();
}

QVariant FlowRowsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_layout.rowCount())
        return QVariant();

    const FlowRowLayout::Row row = m_layout.row(index.row());

    switch (role)
    {
        case FirstIndexRole:
            return row.firstIndex;
        case ChipCountRole:
            return row.count;
        case RowWidthRole:
            return row.width;
        case ChipsRole:
        {
            const auto cached = m_chipsCache.constFind(index.row());
            if (cached != m_chipsCache.constEnd())
                return *cached;
            return *m_chipsCache.insert(index.row(), buildChips(row));
        }
        case ChipStatesRole:
        {
            const auto cached = m_chipStatesCache.constFind(index.row());
            if (cached != m_chipStatesCache.constEnd())
                return *cached;
            return *m_chipStatesCache.insert(index.row(), buildChipStates(row));
        }
        default:
            return QVariant();
    }
}

QVariantList FlowRowsModel::buildChips(const FlowRowLayout::Row& row) const
{
    QVariantList chips;
    if (!m_sourceModel)
        return chips;

    QList<std::pair<QString, int>> roles;
    if (m_itemRoles.isEmpty())
    {
        for (auto it = m_sourceRoles.constBegin(); it != m_sourceRoles.constEnd(); ++it)
        {
            roles.append({ QString::fromUtf8(it.value()), it.key() });
        }
    }
    else
    {
        for (const QString& name : m_itemRoles)
        {
            const int roleId = roleOf(name);
            if (roleId >= 0)
                roles.append({ name, roleId });
        }
    }

    chips.reserve(row.count);
    for (int i = row.firstIndex; i < row.firstIndex + row.count; ++i)
    {
        const QModelIndex sourceIndex = m_sourceModel->index(i, 0);
        QVariantMap chip;
        for (const auto& role : roles)
        {
            chip.insert(role.first, sourceIndex.data(role.second));
        }
        chip.insert(QStringLiteral("chipIndex"), i);
        chip.insert(QStringLiteral("chipWidth"), m_layout.itemWidth(i));
        chips.append(chip);
    }
    return chips;
}

QVariantList FlowRowsModel::buildChipStates(const FlowRowLayout::Row& row) const
{
    QVariantList states;
    if (!m_sourceModel)
        return states;

    QList<std::pair<QString, int>> roles;
    for (const QString& name : m_volatileRoles)
    {
        const int roleId = roleOf(name);
        if (roleId >= 0)
            roles.append({ name, roleId });
    }

    states.reserve(row.count);
    for (int i = row.firstIndex; i < row.firstIndex + row.count; ++i)
    {
        const QModelIndex sourceIndex = m_sourceModel->index(i, 0);
        QVariantMap state;
        for (const auto& role : roles)
        {
            state.insert(role.first, sourceIndex.data(role.second));
        }
        states.append(state);
    }
    return states;
}

void FlowRowsModel::invalidateCaches()
{
    m_chipsCache.clear();
    m_chipStatesCache.clear();
}

QHash<int, QByteArray> FlowRowsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FirstIndexRole] = "firstIndex";
    roles[ChipCountRole] = "chipCount";
    roles[RowWidthRole] = "rowWidth";
    roles[ChipsRole] = "chips";
    roles[ChipStatesRole] = "chipStates";
    return roles;
}

QAbstractItemModel* FlowRowsModel::sourceModel() const { return m_sourceModel; }

void FlowRowsModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    if (m_sourceModel == sourceModel)
        return;

    if (m_sourceModel)
        m_sourceModel->disconnect(this);

    m_sourceModel = sourceModel;
    m_sourceRoles = m_sourceModel ? m_sourceModel->roleNames() : QHash<int, QByteArray>();
    connectSourceModel();

    emit sourceModelChanged();
    scheduleRebuild();
}

QStringList FlowRowsModel::lineRoles() const { return m_lineRoles; }

void FlowRowsModel::setLineRoles(const QStringList& lineRoles)
{
    if (m_lineRoles == lineRoles)
        return;

    m_lineRoles = lineRoles;
    emit lineRolesChanged();
    scheduleRebuild();
}

QStringList FlowRowsModel::volatileRoles() const { return m_volatileRoles; }

void FlowRowsModel::setVolatileRoles(const QStringList& volatileRoles)
{
    if (m_volatileRoles == volatileRoles)
        return;

    m_volatileRoles = volatileRoles;
    emit volatileRolesChanged();

    m_chipStatesCache.clear();
    if (m_layout.rowCount() > 0)
        emit dataChanged(index(0), index(m_layout.rowCount() - 1), { ChipStatesRole });
}

QStringList FlowRowsModel::itemRoles() const { return m_itemRoles; }

void FlowRowsModel::setItemRoles(const QStringList& itemRoles)
{
    if (m_itemRoles == itemRoles)
        return;

    m_itemRoles = itemRoles;
    emit itemRolesChanged();

    m_chipsCache.clear();
    if (m_layout.rowCount() > 0)
        emit dataChanged(index(0), index(m_layout.rowCount() - 1), { ChipsRole });
}

QVariantList FlowRowsModel::lineFonts() const { return m_lineFonts; }

void FlowRowsModel::setLineFonts(const QVariantList& lineFonts)
{
    if (m_lineFonts == lineFonts)
        return;

    m_lineFonts = lineFonts;

    QList<QFont> fonts;
    fonts.reserve(lineFonts.size());
    for (const QVariant& value : lineFonts)
    {
        fonts.append(value.value<QFont>());
    }
    m_layout.setLineFonts(fonts);

    emit lineFontsChanged();
    scheduleRebuild();
}

qreal FlowRowsModel::extraWidth() const { return m_extraWidth; }

void FlowRowsModel::setExtraWidth(qreal extraWidth)
{
    if (qFuzzyCompare(m_extraWidth, extraWidth))
        return;

    m_extraWidth = extraWidth;
    emit extraWidthChanged();
    scheduleRebuild();
}

QString FlowRowsModel::conditionalExtraWidthRole() const { return m_conditionalExtraWidthRole; }

void FlowRowsModel::setConditionalExtraWidthRole(const QString& role)
{
    if (m_conditionalExtraWidthRole == role)
        return;

    m_conditionalExtraWidthRole = role;
    emit conditionalExtraWidthRoleChanged();
    scheduleRebuild();
}

qreal FlowRowsModel::conditionalExtraWidth() const { return m_conditionalExtraWidth; }

void FlowRowsModel::setConditionalExtraWidth(qreal conditionalExtraWidth)
{
    if (qFuzzyCompare(m_conditionalExtraWidth, conditionalExtraWidth))
        return;

    m_conditionalExtraWidth = conditionalExtraWidth;
    emit conditionalExtraWidthChanged();
    scheduleRebuild();
}

qreal FlowRowsModel::containerWidth() const { return m_layout.containerWidth(); }

void FlowRowsModel::setContainerWidth(qreal containerWidth)
{
    if (qFuzzyCompare(m_layout.containerWidth(), containerWidth))
        return;

    beginResetModel();
    m_layout.setContainerWidth(containerWidth);
    invalidateCaches();
    endResetModel();

    emit layoutMetricsChanged();
}

qreal FlowRowsModel::spacing() const { return m_layout.spacing(); }

void FlowRowsModel::setSpacing(qreal spacing)
{
    if (qFuzzyCompare(m_layout.spacing(), spacing))
        return;

    beginResetModel();
    m_layout.setSpacing(spacing);
    invalidateCaches();
    endResetModel();

    emit layoutMetricsChanged();
}

qreal FlowRowsModel::rowHeight() const { return m_layout.rowHeight(); }

void FlowRowsModel::setRowHeight(qreal rowHeight)
{
    if (qFuzzyCompare(m_layout.rowHeight(), rowHeight))
        return;

    m_layout.setRowHeight(rowHeight);
    emit layoutMetricsChanged();
}

qreal FlowRowsModel::contentHeight() const { return m_layout.contentHeight(); }

int FlowRowsModel::itemCount() const { return m_layout.itemCount(); }

void FlowRowsModel::connectSourceModel()
{
    if (!m_sourceModel)
        return;

    connect(m_sourceModel, &QAbstractItemModel::dataChanged, this,
            &FlowRowsModel::onSourceDataChanged);
    connect(m_sourceModel, &QAbstractItemModel::modelReset, this, &FlowRowsModel::scheduleRebuild);
    connect(m_sourceModel, &QAbstractItemModel::rowsInserted, this,
            &FlowRowsModel::scheduleRebuild);
    connect(m_sourceModel, &QAbstractItemModel::rowsRemoved, this, &FlowRowsModel::scheduleRebuild);
    connect(m_sourceModel, &QAbstractItemModel::rowsMoved, this, &FlowRowsModel::scheduleRebuild);
    connect(m_sourceModel, &QAbstractItemModel::layoutChanged, this,
            &FlowRowsModel::scheduleRebuild);
}

void FlowRowsModel::scheduleRebuild()
{
    if (m_rebuildScheduled)
        return;

    m_rebuildScheduled = true;
    QMetaObject::invokeMethod(this, &FlowRowsModel::performRebuild, Qt::QueuedConnection);
}

void FlowRowsModel::performRebuild()
{
    m_rebuildScheduled = false;
    rebuild();
}

void FlowRowsModel::rebuild()
{
    beginResetModel();

    if (!m_sourceModel)
    {
        m_layout.clear();
        invalidateCaches();
        endResetModel();
        emit layoutMetricsChanged();
        return;
    }

    QList<int> lineRoleIds;
    lineRoleIds.reserve(m_lineRoles.size());
    for (const QString& name : m_lineRoles)
    {
        lineRoleIds.append(roleOf(name));
    }
    const int conditionalRoleId = roleOf(m_conditionalExtraWidthRole);

    const int count = m_sourceModel->rowCount();
    QList<FlowRowLayout::Item> items;
    items.reserve(count);

    for (int i = 0; i < count; ++i)
    {
        const QModelIndex sourceIndex = m_sourceModel->index(i, 0);

        FlowRowLayout::Item item;
        for (int roleId : lineRoleIds)
        {
            item.lines.append(roleId >= 0 ? sourceIndex.data(roleId).toString() : QString());
        }

        item.extraWidth = m_extraWidth;
        if (conditionalRoleId >= 0 && sourceIndex.data(conditionalRoleId).toBool())
        {
            item.extraWidth += m_conditionalExtraWidth;
        }

        items.append(item);
    }

    m_layout.setItems(items);
    invalidateCaches();
    endResetModel();

    emit layoutMetricsChanged();
}

void FlowRowsModel::onSourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
                                        const QList<int>& roles)
{
    if (affectsMeasurement(roles))
    {
        scheduleRebuild();
        return;
    }

    const int firstRow = m_layout.rowOfItem(topLeft.row());
    const int lastRow = m_layout.rowOfItem(bottomRight.row());
    if (firstRow < 0 || lastRow < 0)
        return;

    for (int row = firstRow; row <= lastRow; ++row)
    {
        m_chipStatesCache.remove(row);
    }

    emit dataChanged(index(firstRow), index(lastRow), { ChipStatesRole });
}

int FlowRowsModel::roleOf(const QString& name) const
{
    if (name.isEmpty())
        return -1;

    const QByteArray key = name.toUtf8();
    for (auto it = m_sourceRoles.constBegin(); it != m_sourceRoles.constEnd(); ++it)
    {
        if (it.value() == key)
            return it.key();
    }
    return -1;
}

bool FlowRowsModel::affectsMeasurement(const QList<int>& roles) const
{
    if (roles.isEmpty())
        return true;

    for (const QString& name : m_lineRoles)
    {
        if (roles.contains(roleOf(name)))
            return true;
    }

    return !m_conditionalExtraWidthRole.isEmpty()
        && roles.contains(roleOf(m_conditionalExtraWidthRole));
}
