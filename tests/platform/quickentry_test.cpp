#include "platform/quickentry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <gtest/gtest.h>

namespace
{

QuickEntryMode drinkMode()
{
    return {
        .id = "in",
        .label = "IN",
        .values = { 50, 100, 150, 200, 250 },
        .initialValue = 200,
        .valueFormat = "%1 ml",
        .headerFormat = "last: %1",
        .variants = { { "water", "Water" }, { "tea", "Tea" } },
    };
}

QuickEntryMode outMode()
{
    return {
        .id = "out",
        .label = "OUT",
        .values = { 50, 100, 150 },
        .initialValue = 100,
        .valueFormat = "%1 ml",
        .headerFormat = "last out: %1",
    };
}

QuickEntryConfig validConfig()
{
    QuickEntryConfig config;
    config.modes = { drinkMode(), outMode() };
    return config;
}

QJsonObject parsedConfig(const QuickEntryConfig& config)
{
    return QJsonDocument::fromJson(config.toJson()).object();
}

struct QuickEntryRecorder
{
    explicit QuickEntryRecorder(QuickEntry& quickEntry)
    {
        QObject::connect(&quickEntry, &QuickEntry::committed,
                         [this](const QuickEntryAction& action) { actions << action; });
        QObject::connect(&quickEntry, &QuickEntry::visibleChanged, [this] { ++visibleChanges; });
        QObject::connect(&quickEntry, &QuickEntry::headerChanged, [this] { ++headerChanges; });
    }

    QList<QuickEntryAction> actions;
    int visibleChanges = 0;
    int headerChanges = 0;
};

}  // namespace

TEST(QuickEntryConfigTest, ValidConfig_HasNoValidationError)
{
    EXPECT_TRUE(validConfig().validationError().isEmpty());
}

TEST(QuickEntryConfigTest, ConfigWithoutModes_IsRejected)
{
    QuickEntryConfig config;

    EXPECT_FALSE(config.validationError().isEmpty());
}

TEST(QuickEntryConfigTest, ConfigWithoutChannelIdOrName_IsRejected)
{
    QuickEntryConfig withoutId = validConfig();
    withoutId.channelId.clear();
    QuickEntryConfig withoutName = validConfig();
    withoutName.channelName.clear();

    EXPECT_FALSE(withoutId.validationError().isEmpty());
    EXPECT_FALSE(withoutName.validationError().isEmpty());
}

TEST(QuickEntryConfigTest, ModeWithoutIdLabelOrValues_IsRejected)
{
    QuickEntryConfig withoutId = validConfig();
    withoutId.modes[0].id.clear();
    QuickEntryConfig withoutLabel = validConfig();
    withoutLabel.modes[0].label.clear();
    QuickEntryConfig withoutValues = validConfig();
    withoutValues.modes[1].values.clear();

    EXPECT_FALSE(withoutId.validationError().isEmpty());
    EXPECT_FALSE(withoutLabel.validationError().isEmpty());
    EXPECT_FALSE(withoutValues.validationError().isEmpty());
}

TEST(QuickEntryConfigTest, RepeatedModeId_IsRejected)
{
    QuickEntryConfig config = validConfig();
    config.modes[1].id = config.modes[0].id;

    EXPECT_FALSE(config.validationError().isEmpty());
}

TEST(QuickEntryConfigTest, FormatsWithoutThePlaceholder_AreRejected)
{
    QuickEntryConfig noValuePlaceholder = validConfig();
    noValuePlaceholder.modes[0].valueFormat = "ml";
    QuickEntryConfig noHeaderPlaceholder = validConfig();
    noHeaderPlaceholder.modes[1].headerFormat = "last";

    EXPECT_FALSE(noValuePlaceholder.validationError().isEmpty());
    EXPECT_FALSE(noHeaderPlaceholder.validationError().isEmpty());
}

TEST(QuickEntryConfigTest, VariantWithoutKeyOrLabel_IsRejected)
{
    QuickEntryConfig withoutKey = validConfig();
    withoutKey.modes[0].variants[0].key.clear();
    QuickEntryConfig withoutLabel = validConfig();
    withoutLabel.modes[0].variants[1].label.clear();

    EXPECT_FALSE(withoutKey.validationError().isEmpty());
    EXPECT_FALSE(withoutLabel.validationError().isEmpty());
}

TEST(QuickEntryConfigTest, RepeatedVariantKeyInOneMode_IsRejected)
{
    QuickEntryConfig config = validConfig();
    config.modes[0].variants[1].key = config.modes[0].variants[0].key;

    EXPECT_FALSE(config.validationError().isEmpty());
}

TEST(QuickEntryConfigTest, SameVariantKeyInDifferentModes_IsAllowed)
{
    QuickEntryConfig config = validConfig();
    config.modes[1].variants = { { "water", "Water" } };

    EXPECT_TRUE(config.validationError().isEmpty());
}

TEST(QuickEntryConfigTest, Json_CarriesChannelAndEveryMode)
{
    const QJsonObject json = parsedConfig(validConfig());

    EXPECT_EQ(json["channelId"].toString(), "quick_entry");
    EXPECT_EQ(json["channelName"].toString(), "Quick entry");
    EXPECT_EQ(json["smallIcon"].toString(), "ic_notification");
    ASSERT_EQ(json["modes"].toArray().size(), 2);
}

TEST(QuickEntryConfigTest, Json_KeepsTheOrderOfModesValuesAndVariants)
{
    const QJsonArray modes = parsedConfig(validConfig())["modes"].toArray();
    const QJsonObject drink = modes[0].toObject();

    EXPECT_EQ(drink["id"].toString(), "in");
    EXPECT_EQ(modes[1].toObject()["id"].toString(), "out");
    EXPECT_EQ(drink["label"].toString(), "IN");
    EXPECT_EQ(drink["valueFormat"].toString(), "%1 ml");
    EXPECT_EQ(drink["headerFormat"].toString(), "last: %1");
    EXPECT_EQ(drink["initialValue"].toInt(), 200);

    const QJsonArray values = drink["values"].toArray();
    ASSERT_EQ(values.size(), 5);
    EXPECT_EQ(values[0].toInt(), 50);
    EXPECT_EQ(values[4].toInt(), 250);

    const QJsonArray variants = drink["variants"].toArray();
    ASSERT_EQ(variants.size(), 2);
    EXPECT_EQ(variants[0].toObject()["key"].toString(), "water");
    EXPECT_EQ(variants[1].toObject()["key"].toString(), "tea");
    EXPECT_EQ(variants[1].toObject()["label"].toString(), "Tea");
}

TEST(QuickEntryConfigTest, Json_OfAModeWithoutVariantsHasAnEmptyVariantList)
{
    const QJsonObject out = parsedConfig(validConfig())["modes"].toArray()[1].toObject();

    EXPECT_TRUE(out["variants"].isArray());
    EXPECT_TRUE(out["variants"].toArray().isEmpty());
}

TEST(QuickEntryActionTest, JsonList_ParsesActionsInTheOrderTheyWereQueued)
{
    const QByteArray json = R"([
        {"mode":"in","value":250,"variant":"tea","time":1700000000123},
        {"mode":"out","value":100,"variant":"","time":1700000060000}
    ])";

    const QList<QuickEntryAction> actions = QuickEntryAction::listFromJson(json);

    ASSERT_EQ(actions.size(), 2);
    EXPECT_EQ(actions[0].modeId, "in");
    EXPECT_EQ(actions[0].value, 250);
    EXPECT_EQ(actions[0].variantKey, "tea");
    EXPECT_EQ(actions[0].time.toMSecsSinceEpoch(), 1700000000123);
    EXPECT_EQ(actions[1].modeId, "out");
    EXPECT_EQ(actions[1].variantKey, "");
    EXPECT_EQ(actions[1].time.toMSecsSinceEpoch(), 1700000060000);
}

TEST(QuickEntryActionTest, JsonList_SkipsEntriesWithoutAMode)
{
    const QByteArray json = R"([{"value":5,"time":1},{"mode":"in","value":6,"time":2},42])";

    const QList<QuickEntryAction> actions = QuickEntryAction::listFromJson(json);

    ASSERT_EQ(actions.size(), 1);
    EXPECT_EQ(actions[0].value, 6);
}

TEST(QuickEntryActionTest, JsonList_OfEmptyMalformedOrNonArrayInput_IsEmpty)
{
    EXPECT_TRUE(QuickEntryAction::listFromJson("[]").isEmpty());
    EXPECT_TRUE(QuickEntryAction::listFromJson("").isEmpty());
    EXPECT_TRUE(QuickEntryAction::listFromJson("[{").isEmpty());
    EXPECT_TRUE(QuickEntryAction::listFromJson(R"({"mode":"in"})").isEmpty());
}

TEST(QuickEntryTest, StartsHiddenWithoutHeader)
{
    QuickEntry quickEntry;

    EXPECT_FALSE(quickEntry.visible());
    EXPECT_TRUE(quickEntry.header().isEmpty());
}

TEST(QuickEntryTest, Configure_AcceptsAValidConfigAndRejectsAnInvalidOne)
{
    QuickEntry quickEntry;

    EXPECT_FALSE(quickEntry.configure(QuickEntryConfig {}));
    EXPECT_TRUE(quickEntry.configure(validConfig()));
}

TEST(QuickEntryTest, WithoutBackend_CannotShowEvenWhenConfigured)
{
    QuickEntry quickEntry;
    QuickEntryRecorder recorder(quickEntry);
    quickEntry.configure(validConfig());

    EXPECT_FALSE(quickEntry.canShow());
    EXPECT_FALSE(quickEntry.show());
    EXPECT_FALSE(quickEntry.visible());
    EXPECT_EQ(recorder.visibleChanges, 0);
}

TEST(QuickEntryTest, ShowingBeforeConfiguring_IsRefused)
{
    QuickEntry quickEntry;

    EXPECT_FALSE(quickEntry.show());
    EXPECT_FALSE(quickEntry.visible());
}

TEST(QuickEntryTest, HidingWhatIsNotShown_IsHarmless)
{
    QuickEntry quickEntry;
    QuickEntryRecorder recorder(quickEntry);

    quickEntry.hide();

    EXPECT_FALSE(quickEntry.visible());
    EXPECT_EQ(recorder.visibleChanges, 0);
}

TEST(QuickEntryTest, SettingTheHeader_ExposesItAndAnnouncesOnlyChanges)
{
    QuickEntry quickEntry;
    QuickEntryRecorder recorder(quickEntry);

    quickEntry.setHeader("last: Tea 250 ml, 16:21");
    quickEntry.setHeader("last: Tea 250 ml, 16:21");
    quickEntry.setHeader("last: Water 200 ml, 16:40");

    EXPECT_EQ(quickEntry.header(), "last: Water 200 ml, 16:40");
    EXPECT_EQ(recorder.headerChanges, 2);
}

TEST(QuickEntryTest, DeliveredActions_AreAnnouncedEachInTheOrderDelivered)
{
    QuickEntry quickEntry;
    QuickEntryRecorder recorder(quickEntry);
    const QuickEntryAction first { "in", 250, "tea", QDateTime::fromMSecsSinceEpoch(1000) };
    const QuickEntryAction second { "in", 250, "tea", QDateTime::fromMSecsSinceEpoch(1000) };
    const QuickEntryAction third { "out", 100, "", QDateTime::fromMSecsSinceEpoch(2000) };

    quickEntry.deliver(first);
    quickEntry.deliver(second);
    quickEntry.deliver(third);

    EXPECT_EQ(recorder.actions, (QList<QuickEntryAction> { first, second, third }));
}

TEST(QuickEntryTest, StartingDeliveryWithoutBackend_DeliversNothing)
{
    QuickEntry quickEntry;
    QuickEntryRecorder recorder(quickEntry);

    quickEntry.startDelivery();
    quickEntry.startDelivery();

    EXPECT_TRUE(recorder.actions.isEmpty());
}
