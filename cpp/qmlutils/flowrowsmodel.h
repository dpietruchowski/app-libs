#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QPointer>
#include <QStringList>
#include <QVariantList>

#include "qmlutils/flowrowlayout.h"

class FlowRowsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* sourceModel READ sourceModel WRITE setSourceModel NOTIFY
                   sourceModelChanged)
    Q_PROPERTY(QStringList lineRoles READ lineRoles WRITE setLineRoles NOTIFY lineRolesChanged)
    Q_PROPERTY(QStringList volatileRoles READ volatileRoles WRITE setVolatileRoles NOTIFY
                   volatileRolesChanged)
    Q_PROPERTY(QStringList itemRoles READ itemRoles WRITE setItemRoles NOTIFY itemRolesChanged)
    Q_PROPERTY(QVariantList lineFonts READ lineFonts WRITE setLineFonts NOTIFY lineFontsChanged)
    Q_PROPERTY(qreal extraWidth READ extraWidth WRITE setExtraWidth NOTIFY extraWidthChanged)
    Q_PROPERTY(QString conditionalExtraWidthRole READ conditionalExtraWidthRole WRITE
                   setConditionalExtraWidthRole NOTIFY conditionalExtraWidthRoleChanged)
    Q_PROPERTY(qreal conditionalExtraWidth READ conditionalExtraWidth WRITE setConditionalExtraWidth
                   NOTIFY conditionalExtraWidthChanged)
    Q_PROPERTY(qreal containerWidth READ containerWidth WRITE setContainerWidth NOTIFY
                   layoutMetricsChanged)
    Q_PROPERTY(qreal spacing READ spacing WRITE setSpacing NOTIFY layoutMetricsChanged)
    Q_PROPERTY(qreal rowHeight READ rowHeight WRITE setRowHeight NOTIFY layoutMetricsChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY layoutMetricsChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY layoutMetricsChanged)

public:
    enum Roles
    {
        FirstIndexRole = Qt::UserRole + 1,
        ChipCountRole,
        RowWidthRole,
        ChipsRole,
        ChipStatesRole
    };
    Q_ENUM(Roles)

    explicit FlowRowsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QAbstractItemModel* sourceModel() const;
    void setSourceModel(QAbstractItemModel* sourceModel);

    QStringList lineRoles() const;
    void setLineRoles(const QStringList& lineRoles);

    QStringList volatileRoles() const;
    void setVolatileRoles(const QStringList& volatileRoles);

    QStringList itemRoles() const;
    void setItemRoles(const QStringList& itemRoles);

    QVariantList lineFonts() const;
    void setLineFonts(const QVariantList& lineFonts);

    qreal extraWidth() const;
    void setExtraWidth(qreal extraWidth);

    QString conditionalExtraWidthRole() const;
    void setConditionalExtraWidthRole(const QString& role);

    qreal conditionalExtraWidth() const;
    void setConditionalExtraWidth(qreal conditionalExtraWidth);

    qreal containerWidth() const;
    void setContainerWidth(qreal containerWidth);

    qreal spacing() const;
    void setSpacing(qreal spacing);

    qreal rowHeight() const;
    void setRowHeight(qreal rowHeight);

    qreal contentHeight() const;
    int itemCount() const;

signals:
    void sourceModelChanged();
    void lineRolesChanged();
    void volatileRolesChanged();
    void itemRolesChanged();
    void lineFontsChanged();
    void extraWidthChanged();
    void conditionalExtraWidthRoleChanged();
    void conditionalExtraWidthChanged();
    void layoutMetricsChanged();

private:
    void connectSourceModel();
    void scheduleRebuild();
    void performRebuild();
    void rebuild();
    void onSourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
                             const QList<int>& roles);
    int roleOf(const QString& name) const;
    bool affectsMeasurement(const QList<int>& roles) const;
    void invalidateCaches();
    QVariantList buildChips(const FlowRowLayout::Row& row) const;
    QVariantList buildChipStates(const FlowRowLayout::Row& row) const;

    FlowRowLayout m_layout;
    QPointer<QAbstractItemModel> m_sourceModel;
    QHash<int, QByteArray> m_sourceRoles;
    mutable QHash<int, QVariantList> m_chipsCache;
    mutable QHash<int, QVariantList> m_chipStatesCache;
    QStringList m_lineRoles;
    QStringList m_volatileRoles;
    QStringList m_itemRoles;
    QVariantList m_lineFonts;
    QString m_conditionalExtraWidthRole;
    qreal m_extraWidth = 0;
    qreal m_conditionalExtraWidth = 0;
    bool m_rebuildScheduled = false;
};
