package app.libs;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

final class QuickEntryViewState
{
    final String valueText;
    final boolean variantVisible;
    final String variantLabel;
    final boolean modeVisible;
    final boolean openVisible;
    final boolean modeGapVisible;
    final boolean openGapVisible;
    final List<String> modeLabels;
    final List<String> modeTones;
    final int activeModeIndex;
    final String tone;

    QuickEntryViewState(QuickEntryModel model, boolean openAvailable)
    {
        valueText = model.valueText();
        variantVisible = model.hasVariants();
        variantLabel = variantVisible ? model.variant().label : "";
        modeVisible = model.modes().size() > 1;
        openVisible = openAvailable;
        modeGapVisible = variantVisible && modeVisible;
        openGapVisible = openVisible && (variantVisible || modeVisible);
        List<String> labels = new ArrayList<>();
        List<String> tones = new ArrayList<>();
        for (QuickEntryModel.Mode mode : model.modes())
        {
            labels.add(mode.label);
            tones.add(mode.tone);
        }
        modeLabels = Collections.unmodifiableList(labels);
        modeTones = Collections.unmodifiableList(tones);
        activeModeIndex = model.modeIndex();
        tone = model.mode().tone;
    }
}
