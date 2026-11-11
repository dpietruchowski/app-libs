package app.libs;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class QuickEntryModel
{
    public static final class Variant
    {
        public final String key;
        public final String label;

        public Variant(String key, String label)
        {
            this.key = key;
            this.label = label;
        }
    }

    public static final class Mode
    {
        public final String id;
        public final String label;
        public final String valueFormat;
        public final String headerFormat;
        public final int initialValue;
        public final List<Integer> values;
        public final List<Variant> variants;

        public Mode(String id, String label, String valueFormat, String headerFormat, int initialValue,
                    List<Integer> values, List<Variant> variants)
        {
            if (values.isEmpty())
            {
                throw new IllegalArgumentException("mode " + id + " has no values");
            }
            this.id = id;
            this.label = label;
            this.valueFormat = valueFormat;
            this.headerFormat = headerFormat;
            this.initialValue = initialValue;
            this.values = Collections.unmodifiableList(new ArrayList<>(values));
            this.variants = Collections.unmodifiableList(new ArrayList<>(variants));
        }
    }

    public static final class Action
    {
        public final String id;
        public final String modeId;
        public final int value;
        public final String variantKey;
        public final long timeMillis;

        public Action(String id, String modeId, int value, String variantKey, long timeMillis)
        {
            this.id = id;
            this.modeId = modeId;
            this.value = value;
            this.variantKey = variantKey;
            this.timeMillis = timeMillis;
        }
    }

    public static final class State
    {
        public String modeId = "";
        public final Map<String, Integer> values = new HashMap<>();
        public final Map<String, String> variants = new HashMap<>();
    }

    private final List<Mode> modes;
    private final int[] valueIndexes;
    private final int[] variantIndexes;
    private int modeIndex;

    public QuickEntryModel(List<Mode> modes, State saved)
    {
        if (modes.isEmpty())
        {
            throw new IllegalArgumentException("no modes");
        }
        this.modes = Collections.unmodifiableList(new ArrayList<>(modes));
        valueIndexes = new int[modes.size()];
        variantIndexes = new int[modes.size()];
        for (int i = 0; i < modes.size(); i++)
        {
            Mode mode = modes.get(i);
            Integer savedValue = saved == null ? null : saved.values.get(mode.id);
            valueIndexes[i] = nearestValueIndex(mode, savedValue != null ? savedValue : mode.initialValue);
            String savedVariant = saved == null ? null : saved.variants.get(mode.id);
            variantIndexes[i] = Math.max(0, indexOfVariant(mode, savedVariant));
            if (saved != null && mode.id.equals(saved.modeId))
            {
                modeIndex = i;
            }
        }
    }

    public Mode mode()
    {
        return modes.get(modeIndex);
    }

    public int value()
    {
        return mode().values.get(valueIndexes[modeIndex]);
    }

    public String valueText()
    {
        return formatValue(mode(), value());
    }

    public boolean hasVariants()
    {
        return !mode().variants.isEmpty();
    }

    public Variant variant()
    {
        return hasVariants() ? mode().variants.get(variantIndexes[modeIndex]) : null;
    }

    public List<Mode> modes()
    {
        return modes;
    }

    public int modeIndex()
    {
        return modeIndex;
    }

    public void stepDown()
    {
        valueIndexes[modeIndex] = Math.max(0, valueIndexes[modeIndex] - 1);
    }

    public void stepUp()
    {
        valueIndexes[modeIndex] = Math.min(mode().values.size() - 1, valueIndexes[modeIndex] + 1);
    }

    public void cycleVariant()
    {
        if (hasVariants())
        {
            variantIndexes[modeIndex] = (variantIndexes[modeIndex] + 1) % mode().variants.size();
        }
    }

    public void switchMode()
    {
        modeIndex = (modeIndex + 1) % modes.size();
    }

    public Action commit(String id, long timeMillis)
    {
        Variant current = variant();
        return new Action(id, mode().id, value(), current == null ? "" : current.key, timeMillis);
    }

    public State state()
    {
        State state = new State();
        state.modeId = mode().id;
        for (int i = 0; i < modes.size(); i++)
        {
            Mode mode = modes.get(i);
            state.values.put(mode.id, mode.values.get(valueIndexes[i]));
            if (!mode.variants.isEmpty())
            {
                state.variants.put(mode.id, mode.variants.get(variantIndexes[i]).key);
            }
        }
        return state;
    }

    public String headerAfter(Action action, String timeText)
    {
        Mode mode = findMode(action.modeId);
        if (mode == null)
        {
            return "";
        }
        StringBuilder entry = new StringBuilder();
        Variant actionVariant = findVariant(mode, action.variantKey);
        if (actionVariant != null)
        {
            entry.append(actionVariant.label).append(' ');
        }
        entry.append(formatValue(mode, action.value)).append(", ").append(timeText);
        return mode.headerFormat.replace("%1", entry);
    }

    private Mode findMode(String id)
    {
        for (Mode mode : modes)
        {
            if (mode.id.equals(id))
            {
                return mode;
            }
        }
        return null;
    }

    private static Variant findVariant(Mode mode, String key)
    {
        int index = indexOfVariant(mode, key);
        return index < 0 ? null : mode.variants.get(index);
    }

    private static String formatValue(Mode mode, int value)
    {
        return mode.valueFormat.replace("%1", Integer.toString(value));
    }

    private static int nearestValueIndex(Mode mode, int wanted)
    {
        int best = 0;
        for (int i = 1; i < mode.values.size(); i++)
        {
            if (Math.abs(mode.values.get(i) - wanted) < Math.abs(mode.values.get(best) - wanted))
            {
                best = i;
            }
        }
        return best;
    }

    private static int indexOfVariant(Mode mode, String key)
    {
        for (int i = 0; i < mode.variants.size(); i++)
        {
            if (mode.variants.get(i).key.equals(key))
            {
                return i;
            }
        }
        return -1;
    }
}
