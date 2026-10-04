using System.IO;
using System.Text.Json;
using OnkyoRemote.Core;

namespace OnkyoRemote.Desktop;

public sealed class WidgetSettings
{
    public string Endpoint { get; set; } = RemoteEndpoint.Default;
    public bool Pinned { get; set; }
    public double? Left { get; set; }
    public double? Top { get; set; }
    public double Width { get; set; } = 300;
    public double Height { get; set; } = 550;
}

public sealed class SettingsStore(string? path = null)
{
    private readonly string path = path ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "OnkyoRemote", "widget.json");

    public WidgetSettings Load()
    {
        try
        {
            var settings = JsonSerializer.Deserialize<WidgetSettings>(File.ReadAllText(path)) ?? new();
            settings.Endpoint = RemoteEndpoint.Normalize(settings.Endpoint);
            if (!double.IsFinite(settings.Width) || settings.Width < 280 || settings.Width > 700) settings.Width = 300;
            if (!double.IsFinite(settings.Height) || settings.Height < 520 || settings.Height > 1000) settings.Height = 550;
            return settings;
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or JsonException or ArgumentException)
        { return new(); }
    }

    public void Save(WidgetSettings settings)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
        var temporary = path + ".tmp";
        File.WriteAllText(temporary, JsonSerializer.Serialize(settings, new JsonSerializerOptions { WriteIndented = true }));
        File.Move(temporary, path, true);
    }
}
