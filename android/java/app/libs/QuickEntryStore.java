package app.libs;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.Iterator;

final class QuickEntryStore
{
    private static final String PREFERENCES = "app_libs_quick_entry";
    private static final String KEY_CONFIG = "config";
    private static final String KEY_STATE = "state";
    private static final String KEY_HEADER = "header";
    private static final String KEY_VISIBLE = "visible";
    private static final String KEY_QUEUE = "queue";

    private final SharedPreferences preferences;

    QuickEntryStore(Context context)
    {
        preferences = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
    }

    String config()
    {
        return preferences.getString(KEY_CONFIG, "");
    }

    void saveConfig(String json)
    {
        preferences.edit().putString(KEY_CONFIG, json).commit();
    }

    String header()
    {
        return preferences.getString(KEY_HEADER, "");
    }

    void saveHeader(String header)
    {
        preferences.edit().putString(KEY_HEADER, header).commit();
    }

    boolean visible()
    {
        return preferences.getBoolean(KEY_VISIBLE, false);
    }

    void saveVisible(boolean visible)
    {
        preferences.edit().putBoolean(KEY_VISIBLE, visible).commit();
    }

    QuickEntryModel.State state()
    {
        QuickEntryModel.State state = new QuickEntryModel.State();
        try
        {
            JSONObject json = new JSONObject(preferences.getString(KEY_STATE, "{}"));
            state.modeId = json.optString("mode", "");
            readInts(json.optJSONObject("values"), state);
            readStrings(json.optJSONObject("variants"), state);
        }
        catch (JSONException unreadable)
        {
            return new QuickEntryModel.State();
        }
        return state;
    }

    void saveState(QuickEntryModel.State state)
    {
        try
        {
            JSONObject json = new JSONObject();
            json.put("mode", state.modeId);
            json.put("values", new JSONObject(state.values));
            json.put("variants", new JSONObject(state.variants));
            preferences.edit().putString(KEY_STATE, json.toString()).commit();
        }
        catch (JSONException unwritable)
        {
            throw new IllegalStateException(unwritable);
        }
    }

    void enqueue(QuickEntryModel.Action action)
    {
        try
        {
            JSONArray queue = queue();
            queue.put(new JSONObject()
                .put("mode", action.modeId)
                .put("value", action.value)
                .put("variant", action.variantKey)
                .put("time", action.timeMillis));
            preferences.edit().putString(KEY_QUEUE, queue.toString()).commit();
        }
        catch (JSONException unwritable)
        {
            throw new IllegalStateException(unwritable);
        }
    }

    String drain()
    {
        String queue = queue().toString();
        preferences.edit().remove(KEY_QUEUE).commit();
        return queue;
    }

    private JSONArray queue()
    {
        try
        {
            return new JSONArray(preferences.getString(KEY_QUEUE, "[]"));
        }
        catch (JSONException unreadable)
        {
            return new JSONArray();
        }
    }

    private static void readInts(JSONObject json, QuickEntryModel.State state) throws JSONException
    {
        if (json == null)
        {
            return;
        }
        for (Iterator<String> keys = json.keys(); keys.hasNext();)
        {
            String key = keys.next();
            state.values.put(key, json.getInt(key));
        }
    }

    private static void readStrings(JSONObject json, QuickEntryModel.State state) throws JSONException
    {
        if (json == null)
        {
            return;
        }
        for (Iterator<String> keys = json.keys(); keys.hasNext();)
        {
            String key = keys.next();
            state.variants.put(key, json.getString(key));
        }
    }
}
