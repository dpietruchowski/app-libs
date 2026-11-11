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
    final List<String> modeLabels;
    final int activeModeIndex;

    QuickEntryViewState(QuickEntryModel model)
    {
        valueText = model.valueText();
        variantVisible = model.hasVariants();
        variantLabel = variantVisible ? model.variant().label : "";
        modeVisible = model.modes().size() > 1;
        List<String> labels = new ArrayList<>();
        for (QuickEntryModel.Mode mode : model.modes())
        {
            labels.add(mode.label);
        }
        modeLabels = Collections.unmodifiableList(labels);
        activeModeIndex = model.modeIndex();
    }
}
