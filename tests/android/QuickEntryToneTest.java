package app.libs;

import java.util.ArrayList;
import java.util.List;

public final class QuickEntryToneTest
{
    private static final List<String> failures = new ArrayList<>();

    private static void checkEquals(String name, Object expected, Object actual)
    {
        if (!expected.equals(actual))
        {
            failures.add(name + ": expected <" + expected + "> but was <" + actual + ">");
        }
    }

    private static void namesTheResourcesOfATone()
    {
        checkEquals("solid button", "quick_entry_in_solid_button", QuickEntryTone.solidButton("in"));
        checkEquals("soft button", "quick_entry_in_soft_button", QuickEntryTone.softButton("in"));
        checkEquals("solid text", "quick_entry_in_solid_text", QuickEntryTone.solidText("in"));
        checkEquals("text", "quick_entry_in_text", QuickEntryTone.text("in"));
    }

    private static void keepsTheToneNameInsideTheResourceName()
    {
        checkEquals("two word tone", "quick_entry_deep_blue_text", QuickEntryTone.text("deep_blue"));
        checkEquals("other tone", "quick_entry_out_soft_button", QuickEntryTone.softButton("out"));
    }

    private static void namesNothingForAToneThatIsNotSet()
    {
        checkEquals("solid button", "", QuickEntryTone.solidButton(""));
        checkEquals("soft button", "", QuickEntryTone.softButton(""));
        checkEquals("solid text", "", QuickEntryTone.solidText(""));
        checkEquals("text", "", QuickEntryTone.text(""));
    }

    public static void main(String[] args)
    {
        namesTheResourcesOfATone();
        keepsTheToneNameInsideTheResourceName();
        namesNothingForAToneThatIsNotSet();

        for (String failure : failures)
        {
            System.err.println("FAILED: " + failure);
        }
        System.exit(failures.isEmpty() ? 0 : 1);
    }
}
