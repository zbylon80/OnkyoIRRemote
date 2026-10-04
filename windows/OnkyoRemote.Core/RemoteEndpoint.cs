namespace OnkyoRemote.Core;

public static class RemoteEndpoint
{
    public const string Default = "http://192.168.1.46";

    public static string Normalize(string input)
    {
        if (string.IsNullOrWhiteSpace(input)) throw new ArgumentException("Podaj adres ESP32, np. http://192.168.1.46.");
        var value = input.Trim();
        if (!value.Contains("://", StringComparison.Ordinal)) value = "http://" + value;
        if (value.Contains('\\') || !Uri.TryCreate(value, UriKind.Absolute, out var uri)
            || uri.Scheme != "http" || string.IsNullOrWhiteSpace(uri.Host)
            || uri.HostNameType == UriHostNameType.Unknown || uri.Port < 1
            || uri.UserInfo.Length != 0 || uri.Query.Length != 0 || uri.Fragment.Length != 0
            || uri.AbsolutePath != "/")
            throw new ArgumentException("Podaj adres ESP32, np. http://192.168.1.46.");
        return uri.GetLeftPart(UriPartial.Authority).ToLowerInvariant();
    }
}
