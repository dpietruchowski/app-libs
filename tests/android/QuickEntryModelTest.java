package app.libs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class QuickEntryModelTest
{
    private static final List<String> failures = new ArrayList<>();

    private static void check(String name, boolean condition)
    {
        if (!condition)
        {
            failures.add(name);
        }
    }

    private static void checkEquals(String name, Object expected, Object actual)
    {
        if (!expected.equals(actual))
        {
            failures.add(name + ": expected <" + expected + "> but was <" + actual + ">");
        }
    }

    private static QuickEntryModel.Mode drinkMode()
    {
        return new QuickEntryModel.Mode("in", "IN", "%1 ml", "last: %1", 200,
            Arrays.asList(50, 100, 150, 200, 250, 300),
            Arrays.asList(new QuickEntryModel.Variant("water", "Water"),
                new QuickEntryModel.Variant("tea", "Tea"), new QuickEntryModel.Variant("coffee", "Coffee")));
    }

    private static QuickEntryModel.Mode outMode()
    {
        return new QuickEntryModel.Mode("out", "OUT", "%1 ml", "last out: %1", 100,
            Arrays.asList(50, 100, 150, 200, 300), new ArrayList<QuickEntryModel.Variant>());
    }

    private static QuickEntryModel freshModel()
    {
        return new QuickEntryModel(Arrays.asList(drinkMode(), outMode()), null);
    }

    private static void startsOnTheFirstModeAtItsInitialValue()
    {
        QuickEntryModel model = freshModel();
        checkEquals("first mode", "in", model.mode().id);
        checkEquals("initial value", 200, model.value());
        checkEquals("value text", "200 ml", model.valueText());
        checkEquals("first variant", "water", model.variant().key);
    }

    private static void snapsAnInitialValueThatIsNotListedToTheNearestOne()
    {
        QuickEntryModel.Mode mode = new QuickEntryModel.Mode("in", "IN", "%1", "%1", 210,
            Arrays.asList(100, 200, 300), new ArrayList<QuickEntryModel.Variant>());
        checkEquals("nearest initial value", 200, new QuickEntryModel(Arrays.asList(mode), null).value());
    }

    private static void stepsThroughTheValuesAndStopsAtBothEnds()
    {
        QuickEntryModel model = freshModel();
        model.stepUp();
        checkEquals("step up", 250, model.value());
        model.stepUp();
        model.stepUp();
        checkEquals("clamped at the top", 300, model.value());
        for (int i = 0; i < 10; i++)
        {
            model.stepDown();
        }
        checkEquals("clamped at the bottom", 50, model.value());
    }

    private static void cyclesThroughTheVariantsAndWrapsAround()
    {
        QuickEntryModel model = freshModel();
        model.cycleVariant();
        checkEquals("second variant", "tea", model.variant().key);
        model.cycleVariant();
        model.cycleVariant();
        checkEquals("wrapped variant", "water", model.variant().key);
    }

    private static void aModeWithoutVariantsHasNoVariantAndIgnoresCycling()
    {
        QuickEntryModel model = freshModel();
        model.switchMode();
        check("no variant", !model.hasVariants());
        model.cycleVariant();
        check("variant stays absent", model.variant() == null);
        checkEquals("commit without variant", "", model.commit("a1", 1L).variantKey);
    }

    private static void switchingModesKeepsEachModesOwnValueAndVariant()
    {
        QuickEntryModel model = freshModel();
        model.stepUp();
        model.cycleVariant();
        model.switchMode();
        checkEquals("other mode", "out", model.mode().id);
        checkEquals("other mode initial value", 100, model.value());
        model.stepUp();
        model.switchMode();
        checkEquals("back to the first mode", "in", model.mode().id);
        checkEquals("first mode value remembered", 250, model.value());
        checkEquals("first mode variant remembered", "tea", model.variant().key);
        model.switchMode();
        checkEquals("second mode value remembered", 150, model.value());
    }

    private static void commitCarriesTheModeValueVariantAndTime()
    {
        QuickEntryModel model = freshModel();
        model.stepUp();
        model.cycleVariant();

        QuickEntryModel.Action action = model.commit("a1", 1234L);

        checkEquals("action id", "a1", action.id);
        checkEquals("action mode", "in", action.modeId);
        checkEquals("action value", 250, action.value);
        checkEquals("action variant", "tea", action.variantKey);
        checkEquals("action time", 1234L, action.timeMillis);
    }

    private static void commitDoesNotChangeTheSelection()
    {
        QuickEntryModel model = freshModel();
        model.stepUp();

        model.commit("a1", 1L);

        checkEquals("value after commit", 250, model.value());
        checkEquals("mode after commit", "in", model.mode().id);
    }

    private static void restoresASavedState()
    {
        QuickEntryModel first = freshModel();
        first.stepDown();
        first.cycleVariant();
        first.cycleVariant();
        first.switchMode();
        first.stepUp();

        QuickEntryModel restored
            = new QuickEntryModel(Arrays.asList(drinkMode(), outMode()), first.state());

        checkEquals("restored mode", "out", restored.mode().id);
        checkEquals("restored out value", 150, restored.value());
        restored.switchMode();
        checkEquals("restored in value", 150, restored.value());
        checkEquals("restored in variant", "coffee", restored.variant().key);
    }

    private static void snapsASavedValueThatLeftTheListToTheNearestOne()
    {
        QuickEntryModel first = freshModel();
        first.stepUp();
        QuickEntryModel.Mode shorterList = new QuickEntryModel.Mode("in", "IN", "%1 ml", "last: %1", 200,
            Arrays.asList(100, 200), new ArrayList<QuickEntryModel.Variant>());

        QuickEntryModel restored = new QuickEntryModel(Arrays.asList(shorterList), first.state());

        checkEquals("nearest saved value", 200, restored.value());
        check("saved variant gone", restored.variant() == null);
    }

    private static void fallsBackToTheFirstVariantWhenTheSavedOneIsGone()
    {
        QuickEntryModel first = freshModel();
        first.cycleVariant();
        QuickEntryModel.Mode withoutTea = new QuickEntryModel.Mode("in", "IN", "%1 ml", "last: %1", 200,
            Arrays.asList(200), Arrays.asList(new QuickEntryModel.Variant("water", "Water")));

        QuickEntryModel restored = new QuickEntryModel(Arrays.asList(withoutTea), first.state());

        checkEquals("fallback variant", "water", restored.variant().key);
    }

    private static void fallsBackToTheFirstModeWhenTheSavedOneIsGone()
    {
        QuickEntryModel first = freshModel();
        first.switchMode();

        QuickEntryModel restored = new QuickEntryModel(Arrays.asList(drinkMode()), first.state());

        checkEquals("fallback mode", "in", restored.mode().id);
    }

    private static void describesACommittedActionForTheHeader()
    {
        QuickEntryModel model = freshModel();
        model.cycleVariant();
        model.stepUp();
        checkEquals("header with variant", "last: Tea 250 ml, 16:21",
            model.headerAfter(model.commit("a1", 1L), "16:21"));

        model.switchMode();
        checkEquals("header without variant", "last out: 100 ml, 16:22",
            model.headerAfter(model.commit("a2", 2L), "16:22"));
    }

    private static void rejectsAConfigurationThatCannotBeRendered()
    {
        boolean noModes = false;
        try
        {
            new QuickEntryModel(new ArrayList<QuickEntryModel.Mode>(), null);
        }
        catch (IllegalArgumentException expected)
        {
            noModes = true;
        }
        check("no modes rejected", noModes);

        boolean noValues = false;
        try
        {
            new QuickEntryModel.Mode("in", "IN", "%1", "%1", 0, new ArrayList<Integer>(),
                new ArrayList<QuickEntryModel.Variant>());
        }
        catch (IllegalArgumentException expected)
        {
            noValues = true;
        }
        check("no values rejected", noValues);
    }

    public static void main(String[] args)
    {
        startsOnTheFirstModeAtItsInitialValue();
        snapsAnInitialValueThatIsNotListedToTheNearestOne();
        stepsThroughTheValuesAndStopsAtBothEnds();
        cyclesThroughTheVariantsAndWrapsAround();
        aModeWithoutVariantsHasNoVariantAndIgnoresCycling();
        switchingModesKeepsEachModesOwnValueAndVariant();
        commitCarriesTheModeValueVariantAndTime();
        commitDoesNotChangeTheSelection();
        restoresASavedState();
        snapsASavedValueThatLeftTheListToTheNearestOne();
        fallsBackToTheFirstVariantWhenTheSavedOneIsGone();
        fallsBackToTheFirstModeWhenTheSavedOneIsGone();
        describesACommittedActionForTheHeader();
        rejectsAConfigurationThatCannotBeRendered();

        for (String failure : failures)
        {
            System.err.println("FAILED: " + failure);
        }
        System.exit(failures.isEmpty() ? 0 : 1);
    }
}
