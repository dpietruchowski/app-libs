package app.libs;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

final class QuickEntryQueue
{
    private final List<QuickEntryModel.Action> actions = new ArrayList<>();

    QuickEntryQueue(List<QuickEntryModel.Action> initial)
    {
        actions.addAll(initial);
    }

    void add(QuickEntryModel.Action action)
    {
        actions.add(action);
    }

    boolean acknowledge(String id)
    {
        for (Iterator<QuickEntryModel.Action> it = actions.iterator(); it.hasNext();)
        {
            if (it.next().id.equals(id))
            {
                it.remove();
                return true;
            }
        }
        return false;
    }

    List<QuickEntryModel.Action> pending()
    {
        return Collections.unmodifiableList(new ArrayList<>(actions));
    }
}
