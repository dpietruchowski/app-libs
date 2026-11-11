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
        return new QuickEntryModel.Mode("in", "IN", "in", "%1 ml", "last: %1", 200, "",
            Arrays.asList(50, 100, 150, 200, 250, 300),
            Arrays.asList(new QuickEntryModel.Variant("water", "Water"),
                new QuickEntryModel.Variant("tea", "Tea"), new QuickEntryModel.Variant("coffee", "Coffee")));
    }

    private static QuickEntryModel.Mode outMode()
    {
        return new QuickEntryModel.Mode("out", "OUT", "out", "%1 ml", "last out: %1", 100, "",
            Arrays.asList(50, 100, 150, 200, 300), new ArrayList<QuickEntryModel.Variant>());
    }

    private static QuickEntryModel.Mode neutralMode()
    {
        return new QuickEntryModel.Mode("note", "NOTE", "", "%1", "last: %1", 1, "", Arrays.asList(1, 2),
            new ArrayList<QuickEntryModel.Variant>());
    }

    private static QuickEntryModel freshModel()
    {
        return new QuickEntryModel(Arrays.asList(drinkMode(), outMode()), null);
    }

    private static QuickEntryViewState describe(QuickEntryModel model)
    {
        return new QuickEntryViewState(model, true);
    }

    private static void describesAModeWithVariants()
    {
        QuickEntryViewState state = describe(freshModel());

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

        QuickEntryViewState state = describe(model);

        check("variant is hidden", !state.variantVisible);
        checkEquals("variant label", "", state.variantLabel);
        checkEquals("value text", "100 ml", state.valueText);
        checkEquals("active mode", 1, state.activeModeIndex);
        checkEquals("mode labels", Arrays.asList("IN", "OUT"), state.modeLabels);
        check("mode switch stays", state.modeVisible);
    }

    private static void hidesTheModeSwitchForASingleMode()
    {
        QuickEntryViewState state = describe(new QuickEntryModel(Arrays.asList(drinkMode()), null));

        check("mode switch is hidden", !state.modeVisible);
        checkEquals("mode labels", Arrays.asList("IN"), state.modeLabels);
    }

    private static void followsTheModelAfterEveryTap()
    {
        QuickEntryModel model = freshModel();

        model.stepUp();
        checkEquals("value after step up", "250 ml", describe(model).valueText);
        model.stepDown();
        model.stepDown();
        checkEquals("value after step down", "150 ml", describe(model).valueText);
        model.cycleVariant();
        checkEquals("variant after cycling", "Tea", describe(model).variantLabel);
        model.switchMode();
        model.switchMode();
        QuickEntryViewState back = describe(model);
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
            = describe(new QuickEntryModel(Arrays.asList(drinkMode(), outMode()), saved));

        checkEquals("restored mode", 1, state.activeModeIndex);
        checkEquals("restored value", "300 ml", state.valueText);
        check("restored mode has no variant", !state.variantVisible);
        checkEquals("restored tone", "out", state.tone);
    }

    private static void keepsItsModeLabelsApartFromTheModel()
    {
        QuickEntryViewState state = describe(freshModel());

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

    private static void keepsItsModeTonesApartFromTheModel()
    {
        QuickEntryViewState state = describe(freshModel());

        boolean rejected = false;
        try
        {
            state.modeTones.add("extra");
        }
        catch (UnsupportedOperationException expected)
        {
            rejected = true;
        }
        check("mode tones are read only", rejected);
    }

    private static void carriesTheToneOfTheActiveMode()
    {
        QuickEntryModel model = freshModel();

        QuickEntryViewState first = describe(model);
        checkEquals("first mode tone", "in", first.tone);
        checkEquals("every mode keeps its own tone", Arrays.asList("in", "out"), first.modeTones);

        model.switchMode();
        QuickEntryViewState second = describe(model);
        checkEquals("second mode tone", "out", second.tone);
        checkEquals("mode tones do not follow the active mode", Arrays.asList("in", "out"), second.modeTones);

        model.switchMode();
        checkEquals("tone follows the switch back", "in", describe(model).tone);
    }

    private static void aModeWithoutToneStaysNeutral()
    {
        QuickEntryModel model = new QuickEntryModel(Arrays.asList(drinkMode(), neutralMode()), null);
        model.switchMode();

        QuickEntryViewState state = describe(model);

        checkEquals("neutral tone", "", state.tone);
        checkEquals("tones", Arrays.asList("in", ""), state.modeTones);
    }

    private static void separatesVariantModeAndOpenWithGapsOnlyBetweenVisibleButtons()
    {
        QuickEntryViewState all = describe(freshModel());

        check("open is shown", all.openVisible);
        check("gap between variant and mode", all.modeGapVisible);
        check("gap between mode and open", all.openGapVisible);
    }

    private static void dropsTheGapBesideAHiddenVariant()
    {
        QuickEntryModel model = freshModel();
        model.switchMode();

        QuickEntryViewState state = describe(model);

        check("mode is shown", state.modeVisible);
        check("no gap before the mode", !state.modeGapVisible);
        check("gap between mode and open", state.openGapVisible);
    }

    private static void dropsTheGapBesideAHiddenModeSwitch()
    {
        QuickEntryViewState state = describe(new QuickEntryModel(Arrays.asList(drinkMode()), null));

        check("variant is shown", state.variantVisible);
        check("no gap before the hidden mode", !state.modeGapVisible);
        check("gap between variant and open", state.openGapVisible);
    }

    private static void needsNoGapWhenOpenIsTheOnlyButton()
    {
        QuickEntryViewState state = describe(new QuickEntryModel(Arrays.asList(neutralMode()), null));

        check("open is shown", state.openVisible);
        check("no variant", !state.variantVisible);
        check("no mode switch", !state.modeVisible);
        check("no gap before the mode", !state.modeGapVisible);
        check("no gap before open", !state.openGapVisible);
    }

    private static void hidesOpenAndItsGapWhenTheAppCannotBeLaunched()
    {
        QuickEntryViewState state = new QuickEntryViewState(freshModel(), false);

        check("open is hidden", !state.openVisible);
        check("no gap before the hidden open", !state.openGapVisible);
        check("gap between variant and mode stays", state.modeGapVisible);
    }

    public static void main(String[] args)
    {
        describesAModeWithVariants();
        hidesTheVariantInAModeWithoutVariants();
        hidesTheModeSwitchForASingleMode();
        followsTheModelAfterEveryTap();
        describesARestoredSelection();
        keepsItsModeLabelsApartFromTheModel();
        keepsItsModeTonesApartFromTheModel();
        carriesTheToneOfTheActiveMode();
        aModeWithoutToneStaysNeutral();
        separatesVariantModeAndOpenWithGapsOnlyBetweenVisibleButtons();
        dropsTheGapBesideAHiddenVariant();
        dropsTheGapBesideAHiddenModeSwitch();
        needsNoGapWhenOpenIsTheOnlyButton();
        hidesOpenAndItsGapWhenTheAppCannotBeLaunched();

        for (String failure : failures)
        {
            System.err.println("FAILED: " + failure);
        }
        System.exit(failures.isEmpty() ? 0 : 1);
    }
}
