package app.libs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class QuickEntryViewStateTest
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
        return new QuickEntryModel.Mode("in", "IN", "%1 ml", "last: %1", 200, "",
            Arrays.asList(50, 100, 150, 200, 250, 300),
            Arrays.asList(new QuickEntryModel.Variant("water", "Water"),
                new QuickEntryModel.Variant("tea", "Tea"), new QuickEntryModel.Variant("coffee", "Coffee")));
    }

    private static QuickEntryModel.Mode outMode()
    {
        return new QuickEntryModel.Mode("out", "OUT", "%1 ml", "last out: %1", 100, "",
            Arrays.asList(50, 100, 150, 200, 300), new ArrayList<QuickEntryModel.Variant>());
    }

    private static QuickEntryModel freshModel()
    {
        return new QuickEntryModel(Arrays.asList(drinkMode(), outMode()), null);
    }

    private static void describesAModeWithVariants()
    {
        QuickEntryViewState state = new QuickEntryViewState(freshModel());

        checkEquals("value text", "200 ml", state.valueText);
        check("variant is shown", state.variantVisible);
        checkEquals("variant label", "Water", state.variantLabel);
        check("mode switch is shown", state.modeVisible);
        checkEquals("mode labels", Arrays.asList("IN", "OUT"), state.modeLabels);
        checkEquals("active mode", 0, state.activeModeIndex);
    }

    private static void hidesTheVariantInAModeWithoutVariants()
    {
        QuickEntryModel model = freshModel();
        model.switchMode();

        QuickEntryViewState state = new QuickEntryViewState(model);

        check("variant is hidden", !state.variantVisible);
        checkEquals("variant label", "", state.variantLabel);
        checkEquals("value text", "100 ml", state.valueText);
        checkEquals("active mode", 1, state.activeModeIndex);
        checkEquals("mode labels", Arrays.asList("IN", "OUT"), state.modeLabels);
        check("mode switch stays", state.modeVisible);
    }

    private static void hidesTheModeSwitchForASingleMode()
    {
        QuickEntryViewState state
            = new QuickEntryViewState(new QuickEntryModel(Arrays.asList(drinkMode()), null));

        check("mode switch is hidden", !state.modeVisible);
        checkEquals("mode labels", Arrays.asList("IN"), state.modeLabels);
    }

    private static void followsTheModelAfterEveryTap()
    {
        QuickEntryModel model = freshModel();

        model.stepUp();
        checkEquals("value after step up", "250 ml", new QuickEntryViewState(model).valueText);
        model.stepDown();
        model.stepDown();
        checkEquals("value after step down", "150 ml", new QuickEntryViewState(model).valueText);
        model.cycleVariant();
        checkEquals("variant after cycling", "Tea", new QuickEntryViewState(model).variantLabel);
        model.switchMode();
        model.switchMode();
        QuickEntryViewState back = new QuickEntryViewState(model);
        checkEquals("variant kept across modes", "Tea", back.variantLabel);
        checkEquals("value kept across modes", "150 ml", back.valueText);
    }

    private static void describesARestoredSelection()
    {
        QuickEntryModel.State saved = new QuickEntryModel.State();
        saved.modeId = "out";
        saved.values.put("out", 300);
        saved.variants.put("in", "coffee");

        QuickEntryViewState state
            = new QuickEntryViewState(new QuickEntryModel(Arrays.asList(drinkMode(), outMode()), saved));

        checkEquals("restored mode", 1, state.activeModeIndex);
        checkEquals("restored value", "300 ml", state.valueText);
        check("restored mode has no variant", !state.variantVisible);
    }

    private static void keepsItsModeLabelsApartFromTheModel()
    {
        QuickEntryViewState state = new QuickEntryViewState(freshModel());

        boolean rejected = false;
        try
        {
            state.modeLabels.add("extra");
        }
        catch (UnsupportedOperationException expected)
        {
            rejected = true;
        }
        check("mode labels are read only", rejected);
    }

    public static void main(String[] args)
    {
        describesAModeWithVariants();
        hidesTheVariantInAModeWithoutVariants();
        hidesTheModeSwitchForASingleMode();
        followsTheModelAfterEveryTap();
        describesARestoredSelection();
        keepsItsModeLabelsApartFromTheModel();

        for (String failure : failures)
        {
            System.err.println("FAILED: " + failure);
        }
        System.exit(failures.isEmpty() ? 0 : 1);
    }
}
