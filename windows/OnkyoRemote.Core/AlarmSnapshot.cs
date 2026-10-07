using System.Globalization;
using System.Text.Json;

namespace OnkyoRemote.Core;

public interface IAlarmApi
{
    Task<AlarmSnapshot> AlarmsAsync(string endpoint);
    Task<AlarmSnapshot> ChangeAlarmAsync(string endpoint, string action, string? time = null, string? source = null);
}

public sealed record AlarmSlotSnapshot(bool Enabled, string Time, string Date);
public sealed record AlarmSnapshot(bool ClockReady, bool StorageReady, string LocalTime,
    AlarmSlotSnapshot On, AlarmSlotSnapshot Off, string Source)
{
    public static readonly IReadOnlyList<string> Sources = Array.AsReadOnly(new[]
        { "TUNER", "CD", "PHONO", "TAPE-1", "TAPE-2", "VIDEO-1", "VIDEO-2" });

    public static bool ValidTime(string? value) => value is { Length: 5 } && value[2] == ':'
        && value.Where((_, i) => i != 2).All(c => c is >= '0' and <= '9')
        && int.Parse(value[..2], CultureInfo.InvariantCulture) < 24
        && int.Parse(value[3..], CultureInfo.InvariantCulture) < 60;

    public static AlarmSnapshot Parse(JsonElement json)
    {
        static bool Flag(JsonElement value, string key)
        {
            var flag = value.GetProperty(key);
            if (flag.ValueKind is not (JsonValueKind.True or JsonValueKind.False)) throw new IOException("Nieprawidłowa odpowiedź ESP32.");
            return flag.GetBoolean();
        }
        static AlarmSlotSnapshot Slot(JsonElement value)
        {
            var enabled = Flag(value, "enabled");
            var time = value.GetProperty("time").GetString();
            var date = value.GetProperty("date").GetString();
            if (!ValidTime(time) || date == null || (date.Length != 0 &&
                !DateOnly.TryParseExact(date, "yyyy-MM-dd", CultureInfo.InvariantCulture, DateTimeStyles.None, out _))
                || (enabled && date.Length == 0)) throw new IOException("Nieprawidłowy harmonogram ESP32.");
            return new(enabled, time!, date);
        }
        var on = json.GetProperty("on");
        var source = on.GetProperty("source").GetString();
        var localTime = json.GetProperty("localTime").GetString();
        if (source == null || !Sources.Contains(source) || localTime == null || localTime.Length > 40)
            throw new IOException("Nieprawidłowa odpowiedź ESP32.");
        return new(Flag(json, "clockReady"), Flag(json, "storageReady"), localTime,
            Slot(on), Slot(json.GetProperty("off")), source);
    }

    public static Dictionary<string, string> Form(string action, string? time, string? source)
    {
        if (action is not ("on" or "off" or "cancelOn" or "cancelOff")) throw new ArgumentException("Nieznana akcja budzika.");
        var form = new Dictionary<string, string> { ["action"] = action };
        if (action is "on" or "off")
        {
            if (!ValidTime(time)) throw new ArgumentException("Wybierz poprawną godzinę.");
            form[action + "Time"] = time!;
            if (action == "on")
            {
                if (source == null || !Sources.Contains(source)) throw new ArgumentException("Wybierz źródło.");
                form["source"] = source;
            }
        }
        return form;
    }
}
