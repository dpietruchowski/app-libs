package app.libs;

final class QuickEntryTone
{
    private static final String PREFIX = "quick_entry_";

    private QuickEntryTone()
    {
    }

    static String solidButton(String tone)
    {
        return name(tone, "solid_button");
    }

    static String softButton(String tone)
    {
        return name(tone, "soft_button");
    }

    static String solidText(String tone)
    {
        return name(tone, "solid_text");
    }

    static String text(String tone)
    {
        return name(tone, "text");
    }

    private static String name(String tone, String part)
    {
        return tone.isEmpty() ? "" : PREFIX + tone + "_" + part;
    }
}
