package app.libs;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

final class QuickEntryConfig
{
    final String channelId;
    final String channelName;
    final String smallIcon;
    final List<QuickEntryModel.Mode> modes;

    private QuickEntryConfig(String channelId, String channelName, String smallIcon,
                             List<QuickEntryModel.Mode> modes)
    {
        this.channelId = channelId;
        this.channelName = channelName;
        this.smallIcon = smallIcon;
        this.modes = modes;
    }

    static QuickEntryConfig parse(String json) throws JSONException
    {
        JSONObject root = new JSONObject(json);
        JSONArray modesJson = root.getJSONArray("modes");
        List<QuickEntryModel.Mode> modes = new ArrayList<>();
        for (int i = 0; i < modesJson.length(); i++)
        {
            modes.add(parseMode(modesJson.getJSONObject(i)));
        }
        if (modes.isEmpty())
        {
            throw new JSONException("no modes");
        }
        return new QuickEntryConfig(root.getString("channelId"), root.getString("channelName"),
            root.getString("smallIcon"), modes);
    }

    private static QuickEntryModel.Mode parseMode(JSONObject json) throws JSONException
    {
        JSONArray valuesJson = json.getJSONArray("values");
        List<Integer> values = new ArrayList<>();
        for (int i = 0; i < valuesJson.length(); i++)
        {
            values.add(valuesJson.getInt(i));
        }
        JSONArray variantsJson = json.getJSONArray("variants");
        List<QuickEntryModel.Variant> variants = new ArrayList<>();
        for (int i = 0; i < variantsJson.length(); i++)
        {
            JSONObject variant = variantsJson.getJSONObject(i);
            variants.add(new QuickEntryModel.Variant(variant.getString("key"), variant.getString("label")));
        }
        try
        {
            return new QuickEntryModel.Mode(json.getString("id"), json.getString("label"),
                json.getString("valueFormat"), json.getString("headerFormat"), json.getInt("initialValue"),
                values, variants);
        }
        catch (IllegalArgumentException invalid)
        {
            throw new JSONException(invalid.getMessage());
        }
    }
}
