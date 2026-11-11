package app.libs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class QuickEntryQueueTest
{
    private static final List<String> failures = new ArrayList<>();

    private static void checkEquals(String name, Object expected, Object actual)
    {
        if (!expected.equals(actual))
        {
            failures.add(name + ": expected <" + expected + "> but was <" + actual + ">");
        }
    }

    private static QuickEntryModel.Action action(String id)
    {
        return new QuickEntryModel.Action(id, "in", 250, "tea", 1000L);
    }

    private static List<String> ids(QuickEntryQueue queue)
    {
        List<String> ids = new ArrayList<>();
        for (QuickEntryModel.Action action : queue.pending())
        {
            ids.add(action.id);
        }
        return ids;
    }

    private static QuickEntryQueue queueOf(String... ids)
    {
        QuickEntryQueue queue = new QuickEntryQueue(new ArrayList<QuickEntryModel.Action>());
        for (String id : ids)
        {
            queue.add(action(id));
        }
        return queue;
    }

    private static void startsEmpty()
    {
        checkEquals("empty queue", Arrays.asList(), ids(queueOf()));
    }

    private static void keepsActionsInTheOrderTheyWereAdded()
    {
        checkEquals("order", Arrays.asList("a1", "a2", "a3"), ids(queueOf("a1", "a2", "a3")));
    }

    private static void readingThePendingActionsDoesNotRemoveThem()
    {
        QuickEntryQueue queue = queueOf("a1", "a2");

        queue.pending();
        queue.pending();

        checkEquals("still pending", Arrays.asList("a1", "a2"), ids(queue));
    }

    private static void anUnacknowledgedActionIsPendingAgainOnTheNextRead()
    {
        QuickEntryQueue queue = queueOf("a1", "a2", "a3");
        queue.acknowledge("a2");

        checkEquals("first read", Arrays.asList("a1", "a3"), ids(queue));
        checkEquals("second read", Arrays.asList("a1", "a3"), ids(queue));
    }

    private static void anAcknowledgedActionNeverComesBack()
    {
        QuickEntryQueue queue = queueOf("a1", "a2");
        queue.acknowledge("a1");
        queue.add(action("a3"));

        checkEquals("acknowledged one is gone", Arrays.asList("a2", "a3"), ids(queue));
    }

    private static void acknowledgingKeepsTheOrderOfTheRest()
    {
        QuickEntryQueue queue = queueOf("a1", "a2", "a3", "a4");

        queue.acknowledge("a3");
        queue.acknowledge("a1");

        checkEquals("order of the rest", Arrays.asList("a2", "a4"), ids(queue));
    }

    private static void acknowledgingAnUnknownIdChangesNothing()
    {
        QuickEntryQueue queue = queueOf("a1", "a2");

        boolean removedUnknown = queue.acknowledge("unknown");
        boolean removedEmpty = queue.acknowledge("");

        checkEquals("unknown id reports nothing removed", false, removedUnknown);
        checkEquals("empty id reports nothing removed", false, removedEmpty);
        checkEquals("queue untouched", Arrays.asList("a1", "a2"), ids(queue));
    }

    private static void acknowledgingTwiceIsHarmless()
    {
        QuickEntryQueue queue = queueOf("a1", "a2");

        boolean first = queue.acknowledge("a1");
        boolean second = queue.acknowledge("a1");

        checkEquals("first acknowledge removes", true, first);
        checkEquals("second acknowledge removes nothing", false, second);
        checkEquals("rest untouched", Arrays.asList("a2"), ids(queue));
    }

    private static void twoTapsWithTheSameContentButDifferentIdsAreBothKept()
    {
        QuickEntryQueue queue = queueOf("a1", "a2");

        queue.acknowledge("a1");

        checkEquals("only the acknowledged tap is gone", Arrays.asList("a2"), ids(queue));
    }

    private static void startsFromAnExistingList()
    {
        QuickEntryQueue queue = new QuickEntryQueue(Arrays.asList(action("a1"), action("a2")));
        queue.add(action("a3"));

        checkEquals("restored then extended", Arrays.asList("a1", "a2", "a3"), ids(queue));
    }

    private static void thePendingListIsASnapshot()
    {
        QuickEntryQueue queue = queueOf("a1", "a2");
        List<QuickEntryModel.Action> snapshot = queue.pending();

        queue.acknowledge("a1");

        checkEquals("snapshot unchanged", 2, snapshot.size());
        boolean modifiable = true;
        try
        {
            snapshot.clear();
        }
        catch (UnsupportedOperationException expected)
        {
            modifiable = false;
        }
        checkEquals("snapshot is read-only", false, modifiable);
    }

    public static void main(String[] args)
    {
        startsEmpty();
        keepsActionsInTheOrderTheyWereAdded();
        readingThePendingActionsDoesNotRemoveThem();
        anUnacknowledgedActionIsPendingAgainOnTheNextRead();
        anAcknowledgedActionNeverComesBack();
        acknowledgingKeepsTheOrderOfTheRest();
        acknowledgingAnUnknownIdChangesNothing();
        acknowledgingTwiceIsHarmless();
        twoTapsWithTheSameContentButDifferentIdsAreBothKept();
        startsFromAnExistingList();
        thePendingListIsASnapshot();

        for (String failure : failures)
        {
            System.err.println("FAILED: " + failure);
        }
        System.exit(failures.isEmpty() ? 0 : 1);
    }
}
