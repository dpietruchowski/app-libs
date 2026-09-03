#include <QAbstractListModel>
#include <QCoreApplication>
#include <QFont>
#include <QSignalSpy>
#include <gtest/gtest.h>

#include "qmlutils/flowrowsmodel.h"

namespace
{
class FakeChipModel : public QAbstractListModel
{
public:
    enum Roles
    {
        HeadwordRole = Qt::UserRole + 1,
        TranslationRole,
        SelectedRole,
        FlaggedRole
    };

    struct Entry
    {
        QString headword;
        QString translation;
        bool selected = false;
        bool flagged = false;
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
            return QVariant();

        const Entry& entry = m_entries[static_cast<size_t>(index.row())];
        switch (role)
        {
            case HeadwordRole:
                return entry.headword;
            case TranslationRole:
                return entry.translation;
            case SelectedRole:
                return entry.selected;
            case FlaggedRole:
                return entry.flagged;
            default:
                return QVariant();
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return { { HeadwordRole, "headword" },
                 { TranslationRole, "translation" },
                 { SelectedRole, "selected" },
                 { FlaggedRole, "flagged" } };
    }

    void setEntries(const std::vector<Entry>& entries)
    {
        beginResetModel();
        m_entries = entries;
        endResetModel();
    }

    void setSelected(int row, bool selected)
    {
        m_entries[static_cast<size_t>(row)].selected = selected;
        emit dataChanged(index(row), index(row), { SelectedRole });
    }

    void setHeadword(int row, const QString& headword)
    {
        m_entries[static_cast<size_t>(row)].headword = headword;
        emit dataChanged(index(row), index(row), { HeadwordRole });
    }

private:
    std::vector<Entry> m_entries;
};

std::vector<FakeChipModel::Entry> makeEntries(int count)
{
    std::vector<FakeChipModel::Entry> entries;
    for (int i = 0; i < count; ++i)
    {
        entries.push_back({ QStringLiteral("headword%1").arg(i),
                            QStringLiteral("translation%1").arg(i), false, false });
    }
    return entries;
}

void flush() { QCoreApplication::processEvents(); }

FlowRowsModel* configure(FlowRowsModel* rows, FakeChipModel* source)
{
    rows->setLineRoles({ "headword", "translation" });
    rows->setVolatileRoles({ "selected" });
    rows->setSpacing(8);
    rows->setRowHeight(52);
    rows->setContainerWidth(400);
    rows->setSourceModel(source);
    flush();
    return rows;
}

QVariantList chipsOf(const FlowRowsModel& rows, int row)
{
    return rows.data(rows.index(row), FlowRowsModel::ChipsRole).toList();
}

QVariantList statesOf(const FlowRowsModel& rows, int row)
{
    return rows.data(rows.index(row), FlowRowsModel::ChipStatesRole).toList();
}
}

TEST(FlowRowsModelTest, EmptySourceProducesNoRows)
{
    FakeChipModel source;
    FlowRowsModel rows;
    configure(&rows, &source);

    EXPECT_EQ(rows.rowCount(), 0);
    EXPECT_EQ(rows.itemCount(), 0);
}

TEST(FlowRowsModelTest, PacksSourceItemsIntoRows)
{
    FakeChipModel source;
    source.setEntries(makeEntries(30));

    FlowRowsModel rows;
    configure(&rows, &source);

    EXPECT_EQ(rows.itemCount(), 30);
    EXPECT_GT(rows.rowCount(), 1);

    int chipsSeen = 0;
    for (int row = 0; row < rows.rowCount(); ++row)
    {
        chipsSeen += rows.data(rows.index(row), FlowRowsModel::ChipCountRole).toInt();
    }
    EXPECT_EQ(chipsSeen, 30);
}

TEST(FlowRowsModelTest, ChipCarriesIndexAndWidth)
{
    FakeChipModel source;
    source.setEntries(makeEntries(4));

    FlowRowsModel rows;
    configure(&rows, &source);

    const QVariantMap chip = chipsOf(rows, 0).first().toMap();
    EXPECT_EQ(chip.value("chipIndex").toInt(), 0);
    EXPECT_GT(chip.value("chipWidth").toDouble(), 0.0);
    EXPECT_EQ(chip.value("headword").toString(), QStringLiteral("headword0"));
}

TEST(FlowRowsModelTest, ItemRolesTrimTheChipPayload)
{
    FakeChipModel source;
    source.setEntries(makeEntries(4));

    FlowRowsModel rows;
    configure(&rows, &source);

    EXPECT_TRUE(chipsOf(rows, 0).first().toMap().contains("flagged"));

    rows.setItemRoles({ "headword", "translation" });
    flush();

    const QVariantMap trimmed = chipsOf(rows, 0).first().toMap();
    EXPECT_FALSE(trimmed.contains("flagged"));
    EXPECT_FALSE(trimmed.contains("selected"));
    EXPECT_TRUE(trimmed.contains("headword"));
    EXPECT_TRUE(trimmed.contains("chipWidth"));
}

TEST(FlowRowsModelTest, VolatileRoleChangeRefreshesChipStates)
{
    FakeChipModel source;
    source.setEntries(makeEntries(4));

    FlowRowsModel rows;
    configure(&rows, &source);

    EXPECT_FALSE(statesOf(rows, 0).first().toMap().value("selected").toBool());

    source.setSelected(0, true);
    flush();

    EXPECT_TRUE(statesOf(rows, 0).first().toMap().value("selected").toBool());
}

TEST(FlowRowsModelTest, VolatileRoleChangeDoesNotResetTheModel)
{
    FakeChipModel source;
    source.setEntries(makeEntries(4));

    FlowRowsModel rows;
    configure(&rows, &source);

    QSignalSpy resetSpy(&rows, &QAbstractItemModel::modelReset);
    QSignalSpy dataSpy(&rows, &QAbstractItemModel::dataChanged);

    source.setSelected(0, true);
    flush();

    EXPECT_EQ(resetSpy.count(), 0);
    ASSERT_EQ(dataSpy.count(), 1);
    const QList<int> roles = dataSpy.first().at(2).value<QList<int>>();
    EXPECT_TRUE(roles.contains(FlowRowsModel::ChipStatesRole));
    EXPECT_FALSE(roles.contains(FlowRowsModel::ChipsRole));
}

TEST(FlowRowsModelTest, MeasuredRoleChangeRebuildsTheLayout)
{
    FakeChipModel source;
    source.setEntries(makeEntries(4));

    FlowRowsModel rows;
    configure(&rows, &source);

    const double before = chipsOf(rows, 0).first().toMap().value("chipWidth").toDouble();

    QSignalSpy resetSpy(&rows, &QAbstractItemModel::modelReset);
    source.setHeadword(0, QStringLiteral("a substantially longer headword than before"));
    flush();

    EXPECT_EQ(resetSpy.count(), 1);
    const double after = chipsOf(rows, 0).first().toMap().value("chipWidth").toDouble();
    EXPECT_GT(after, before);
}

TEST(FlowRowsModelTest, SourceResetRefreshesCachedChips)
{
    FakeChipModel source;
    source.setEntries(makeEntries(4));

    FlowRowsModel rows;
    configure(&rows, &source);

    EXPECT_EQ(chipsOf(rows, 0).first().toMap().value("headword").toString(),
              QStringLiteral("headword0"));

    source.setEntries(
        { { QStringLiteral("brand new"), QStringLiteral("translation"), false, false } });
    flush();

    EXPECT_EQ(rows.itemCount(), 1);
    EXPECT_EQ(chipsOf(rows, 0).first().toMap().value("headword").toString(),
              QStringLiteral("brand new"));
}

TEST(FlowRowsModelTest, ResizingRepacksAndRefreshesCachedChips)
{
    FakeChipModel source;
    source.setEntries(makeEntries(30));

    FlowRowsModel rows;
    configure(&rows, &source);

    const int wideRows = rows.rowCount();
    const QVariantList firstRowWide = chipsOf(rows, 0);

    rows.setContainerWidth(150);

    EXPECT_GT(rows.rowCount(), wideRows);
    EXPECT_LT(chipsOf(rows, 0).size(), firstRowWide.size());
}

TEST(FlowRowsModelTest, ClearingTheSourceModelEmptiesTheRows)
{
    FakeChipModel source;
    source.setEntries(makeEntries(10));

    FlowRowsModel rows;
    configure(&rows, &source);
    ASSERT_GT(rows.rowCount(), 0);

    rows.setSourceModel(nullptr);
    flush();

    EXPECT_EQ(rows.rowCount(), 0);
    EXPECT_EQ(rows.itemCount(), 0);
    EXPECT_DOUBLE_EQ(rows.contentHeight(), 0);
}

TEST(FlowRowsModelTest, BurstOfSourceSignalsCollapsesIntoOneRebuild)
{
    FakeChipModel source;
    source.setEntries(makeEntries(30));

    FlowRowsModel rows;
    configure(&rows, &source);

    QSignalSpy resetSpy(&rows, &QAbstractItemModel::modelReset);

    for (int i = 0; i < 50; ++i)
    {
        source.setEntries(makeEntries(30 - (i % 5)));
    }
    flush();

    EXPECT_EQ(resetSpy.count(), 1);
}

TEST(FlowRowsModelTest, ConfiguringManyPropertiesCollapsesIntoOneRebuild)
{
    FakeChipModel source;
    source.setEntries(makeEntries(30));

    FlowRowsModel rows;
    QSignalSpy resetSpy(&rows, &QAbstractItemModel::modelReset);

    rows.setLineRoles({ "headword", "translation" });
    rows.setVolatileRoles({ "selected" });
    rows.setExtraWidth(24);
    rows.setConditionalExtraWidthRole("flagged");
    rows.setConditionalExtraWidth(12);
    rows.setSourceModel(&source);
    flush();

    EXPECT_EQ(resetSpy.count(), 1);
    EXPECT_EQ(rows.itemCount(), 30);
}
