using System.Net;
using System.Text.Json;

namespace OnkyoRemote.Core;

public interface IRemoteApi
{
    Task CommandAsync(string endpoint, string command);
    Task<string> VersionAsync(string endpoint);
    Task<string> PressAsync(string endpoint, string direction);
    Task VolumeAsync(string endpoint, string operation, string session);
}

public sealed class RemoteApi : IRemoteApi, IAlarmApi, IDisposable
{
    public static readonly IReadOnlyList<string> Commands = Array.AsReadOnly(new[]
    { "POWER", "VOL-", "VOL+", "MUTE", "TAPE-1", "CD", "PHONO", "TUNER", "VIDEO-1", "PRESET-", "PRESET+" });

    private readonly HttpClient client = new(new SocketsHttpHandler
    {
        AllowAutoRedirect = false, UseProxy = false, UseCookies = false,
        ConnectTimeout = TimeSpan.FromSeconds(2), MaxResponseHeadersLength = 8
    }) { Timeout = Timeout.InfiniteTimeSpan };

    public async Task CommandAsync(string endpoint, string command)
    {
        if (!Commands.Contains(command)) throw new ArgumentException("Unknown Basic command");
        Acknowledge(await RequestAsync(endpoint, "/command", "name", command));
    }

    public async Task<string> VersionAsync(string endpoint)
    {
        var json = await RequestAsync(endpoint, "/version");
        if (!json.TryGetProperty("version", out var version) || version.ValueKind != JsonValueKind.String
            || string.IsNullOrWhiteSpace(version.GetString())) throw new IOException("Invalid version response");
        return version.GetString()!;
    }

    public async Task<string> PressAsync(string endpoint, string direction)
    {
        if (direction is not ("up" or "down")) throw new ArgumentException("Invalid direction");
        var json = await RequestAsync(endpoint, "/volume/press", "direction", direction);
        Acknowledge(json);
        if (!json.TryGetProperty("session", out var token) || token.ValueKind != JsonValueKind.String)
            throw new IOException("Invalid volume session");
        var session = token.GetString()!;
        if (session.Length is < 1 or > 20 || session.Any(c => c is < '0' or > '9')
            || !ulong.TryParse(session, out var number) || number == 0)
            throw new IOException("Invalid volume session");
        return session;
    }

    public async Task VolumeAsync(string endpoint, string operation, string session)
    {
        if (operation is not ("start" or "keepalive" or "stop")) throw new ArgumentException("Invalid operation");
        Acknowledge(await RequestAsync(endpoint, "/volume/" + operation, "session", session));
    }

    public async Task<AlarmSnapshot> AlarmsAsync(string endpoint) => AlarmSnapshot.Parse(await RequestAsync(endpoint, "/alarms"));

    public async Task<AlarmSnapshot> ChangeAlarmAsync(string endpoint, string action, string? time = null, string? source = null)
        => AlarmSnapshot.Parse(await RequestFormAsync(endpoint, "/alarms", AlarmSnapshot.Form(action, time, source)));

    private Task<JsonElement> RequestAsync(string endpoint, string path, string? field = null, string? value = null)
        => RequestFormAsync(endpoint, path, field == null ? null : new Dictionary<string, string> { [field] = value! });

    private async Task<JsonElement> RequestFormAsync(string endpoint, string path, IEnumerable<KeyValuePair<string, string>>? form)
    {
        using var deadline = new CancellationTokenSource(TimeSpan.FromMilliseconds(path.StartsWith("/volume/", StringComparison.Ordinal) ? 400 : 2000));
        using var request = new HttpRequestMessage(form == null ? HttpMethod.Get : HttpMethod.Post,
            RemoteEndpoint.Normalize(endpoint) + path)
        { Version = HttpVersion.Version11, VersionPolicy = HttpVersionPolicy.RequestVersionExact };
        request.Headers.ConnectionClose = true;
        request.Headers.Accept.ParseAdd("application/json");
        if (form != null) request.Content = new FormUrlEncodedContent(form);
        // One fresh HTTP/1.1 connection per request; no redirects or application retries.
        using var response = await client.SendAsync(request, HttpCompletionOption.ResponseHeadersRead, deadline.Token).ConfigureAwait(false);
        if (response.StatusCode != HttpStatusCode.OK && path != "/alarms") throw new IOException("Device rejected request");
        await using var stream = await response.Content.ReadAsStreamAsync(deadline.Token).ConfigureAwait(false);
        using var body = new MemoryStream();
        var buffer = new byte[256];
        int count;
        while ((count = await stream.ReadAsync(buffer, deadline.Token).ConfigureAwait(false)) > 0)
        {
            if (body.Length + count > 2048) throw new IOException("Response too large");
            body.Write(buffer, 0, count);
        }
        using var document = JsonDocument.Parse(body.ToArray());
        if (document.RootElement.ValueKind != JsonValueKind.Object) throw new IOException("Invalid JSON response");
        if (response.StatusCode != HttpStatusCode.OK)
            throw new IOException(document.RootElement.TryGetProperty("error", out var error) && error.ValueKind == JsonValueKind.String
                ? error.GetString() : "Nie można odczytać budzika. Wymagane firmware 1.3.0 lub nowsze.");
        return document.RootElement.Clone();
    }

    private static void Acknowledge(JsonElement json)
    {
        if (!json.TryGetProperty("ok", out var ok) || ok.ValueKind != JsonValueKind.True)
            throw new IOException("Unconfirmed request");
    }

    public void Dispose() => client.Dispose();
}
