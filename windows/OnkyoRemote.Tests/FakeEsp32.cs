using System.Collections.Concurrent;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;

namespace OnkyoRemote.Tests;

internal record Request(string Method, string Path, string Body);
internal record Reply(string Body, int Status = 200, int Delay = 0);

/// <summary>Loopback only. Never contacts the physical ESP32 or emits IR.</summary>
internal sealed class FakeEsp32 : IDisposable
{
    private readonly TcpListener listener = new(IPAddress.Loopback, 0);
    private readonly CancellationTokenSource shutdown = new();
    public readonly ConcurrentQueue<Request> Requests = new();
    public Func<Request, Reply>? Respond;
    public string Endpoint { get; }

    public FakeEsp32()
    {
        listener.Start();
        Endpoint = "http://127.0.0.1:" + ((IPEndPoint)listener.LocalEndpoint).Port;
        _ = AcceptAsync();
    }

    private async Task AcceptAsync()
    {
        try
        {
            while (!shutdown.IsCancellationRequested)
            {
                var socket = await listener.AcceptTcpClientAsync(shutdown.Token);
                _ = HandleAsync(socket);
            }
        }
        catch (OperationCanceledException) { }
        catch (SocketException) when (shutdown.IsCancellationRequested) { }
    }

    private async Task HandleAsync(TcpClient socket)
    {
        using (socket)
        {
            try
            {
                var stream = socket.GetStream();
                using var reader = new StreamReader(stream, Encoding.ASCII, false, 1024, true);
                var first = (await reader.ReadLineAsync(shutdown.Token))!.Split(' ');
                var length = 0;
                string? line;
                while (!string.IsNullOrEmpty(line = await reader.ReadLineAsync(shutdown.Token)))
                    if (line.StartsWith("Content-Length:", StringComparison.OrdinalIgnoreCase)) length = int.Parse(line[15..].Trim());
                var body = new char[length];
                var offset = 0;
                while (offset < length)
                {
                    var count = await reader.ReadAsync(body.AsMemory(offset), shutdown.Token);
                    if (count == 0) throw new IOException("Incomplete request");
                    offset += count;
                }
                var request = new Request(first[0], first[1], new string(body));
                Requests.Enqueue(request);
                var reply = Respond?.Invoke(request) ?? Default(request);
                await Task.Delay(reply.Delay, shutdown.Token);
                var bytes = Encoding.UTF8.GetBytes(reply.Body);
                var header = Encoding.ASCII.GetBytes($"HTTP/1.1 {reply.Status} Test\r\nContent-Type: application/json\r\nContent-Length: {bytes.Length}\r\nConnection: close\r\n\r\n");
                await stream.WriteAsync(header, shutdown.Token);
                await stream.WriteAsync(bytes, shutdown.Token);
            }
            catch (IOException) { }
            catch (OperationCanceledException) { }
        }
    }

    public static Reply Default(Request request) => new(request.Path switch
    {
        "/version" => "{\"version\":\"1.1.0\"}",
        "/volume/press" => "{\"ok\":true,\"session\":\"123\"}",
        _ => "{\"ok\":true}"
    });

    public void Clear() { while (Requests.TryDequeue(out _)) { } }
    public void Dispose() { shutdown.Cancel(); listener.Stop(); }
}
