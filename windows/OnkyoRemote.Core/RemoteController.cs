using System.Diagnostics;

namespace OnkyoRemote.Core;

/// <summary>Serializes user actions. Release cancels waiting, never a known-session stop.</summary>
public sealed class RemoteController(IRemoteApi api, string endpoint)
{
    private readonly object gate = new();
    private string endpoint = RemoteEndpoint.Normalize(endpoint);
    private bool busy;
    private Gesture? current;
    private Task active = Task.CompletedTask;
    private long nextGestureId;
    public event Action<bool>? BusyChanged;
    public event Action<string, bool>? StatusChanged;
    public bool IsBusy { get { lock (gate) return busy; } }
    public Task Completion { get { lock (gate) return active; } }
    public string Endpoint { get { lock (gate) return endpoint; } }

    private sealed class Gesture(string direction, long id, long began)
    {
        public readonly long Id = id;
        public readonly string Direction = direction;
        public long ElapsedMs => (long)Stopwatch.GetElapsedTime(began).TotalMilliseconds;
        public readonly CancellationTokenSource Released = new();
        public bool Held => !Released.IsCancellationRequested && ElapsedMs < 3000;
    }

    public void SetEndpoint(string value)
    {
        var normalized = RemoteEndpoint.Normalize(value);
        lock (gate)
        {
            if (busy) throw new InvalidOperationException("Poczekaj na zakończenie polecenia.");
            endpoint = normalized;
        }
    }

    public bool BeginVolume(string direction) => TryBeginVolume(direction, out _);

    public bool TryBeginVolume(string direction, out long gestureId, long? beganTimestamp = null)
    {
        if (direction is not ("up" or "down")) throw new ArgumentException("Invalid direction");
        lock (gate)
        {
            gestureId = 0;
            if (busy) return false;
            busy = true;
            current = new Gesture(direction, ++nextGestureId, beganTimestamp ?? Stopwatch.GetTimestamp());
            gestureId = current.Id;
            BusyChanged?.Invoke(true);
            active = RunVolumeAsync(current, endpoint);
            return true;
        }
    }

    public void ReleaseVolume()
    {
        lock (gate) current?.Released.Cancel();
    }

    public void ReleaseVolume(long gestureId)
    {
        lock (gate) { if (current?.Id == gestureId) current.Released.Cancel(); }
    }

    public Task<bool> CommandAsync(string command) => BeginActionAsync(command, false, null);
    public Task<bool> CheckAsync(string? target = null) => BeginActionAsync("", true, target);

    public Task<AlarmSnapshot> AlarmsAsync() => BeginAlarmAsync(null, null, null);
    public Task<AlarmSnapshot> ChangeAlarmAsync(string action, string? time = null, string? source = null)
        => BeginAlarmAsync(action, time, source);

    private Task<AlarmSnapshot> BeginAlarmAsync(string? action, string? time, string? source)
    {
        lock (gate)
        {
            if (busy) throw new InvalidOperationException("Poczekaj na zakończenie polecenia.");
            if (api is not IAlarmApi alarms) throw new InvalidOperationException("Obsługa budzika niedostępna.");
            busy = true;
            BusyChanged?.Invoke(true);
            var task = RunAlarmAsync(alarms, endpoint, action, time, source);
            active = task;
            return task;
        }
    }

    private async Task<AlarmSnapshot> RunAlarmAsync(IAlarmApi alarms, string target, string? action, string? time, string? source)
    {
        try
        {
            return action == null ? await alarms.AlarmsAsync(target).ConfigureAwait(false)
                : await alarms.ChangeAlarmAsync(target, action, time, source).ConfigureAwait(false);
        }
        finally { EndAction(); }
    }

    private Task<bool> BeginActionAsync(string command, bool check, string? target)
    {
        var checkEndpoint = target == null ? null : RemoteEndpoint.Normalize(target);
        lock (gate)
        {
            if (busy) return Task.FromResult(false);
            busy = true;
            BusyChanged?.Invoke(true);
            var task = RunActionAsync(checkEndpoint ?? endpoint, command, check);
            active = task;
            return task;
        }
    }

    private async Task<bool> RunActionAsync(string target, string command, bool check)
    {
        try
        {
            if (check) StatusChanged?.Invoke("Połączono · firmware " + await api.VersionAsync(target).ConfigureAwait(false), true);
            else
            {
                await api.CommandAsync(target, command).ConfigureAwait(false);
                StatusChanged?.Invoke("ESP32 przyjęło " + command, true);
            }
            return true;
        }
        catch (Exception) { StatusChanged?.Invoke(check ? "Brak połączenia z ESP32" : "Brak potwierdzenia: " + command, false); return false; }
        finally { EndAction(); }
    }

    private async Task RunVolumeAsync(Gesture gesture, string target)
    {
        string? session = null;
        bool success = false;
        try
        {
            session = await api.PressAsync(target, gesture.Direction).ConfigureAwait(false);
            success = true;
            await WaitAsync(gesture, Math.Max(0, 350 - gesture.ElapsedMs)).ConfigureAwait(false);
            if (gesture.Held)
            {
                await api.VolumeAsync(target, "start", session).ConfigureAwait(false);
                while (gesture.Held)
                {
                    await WaitAsync(gesture, Math.Min(500, Math.Max(0, 3000 - gesture.ElapsedMs))).ConfigureAwait(false);
                    if (gesture.Held) await api.VolumeAsync(target, "keepalive", session).ConfigureAwait(false);
                }
            }
        }
        catch (Exception) { success = false; }
        finally
        {
            if (session != null)
            {
                try { await api.VolumeAsync(target, "stop", session).ConfigureAwait(false); }
                catch (Exception) { success = false; }
            }
            lock (gate) { current = null; gesture.Released.Dispose(); }
            StatusChanged?.Invoke(success ? "ESP32 przyjęło " + (gesture.Direction == "up" ? "VOL+" : "VOL−") : "Brak potwierdzenia głośności", success);
            EndAction();
        }
    }

    private static async Task WaitAsync(Gesture gesture, long milliseconds)
    {
        try { await Task.Delay(TimeSpan.FromMilliseconds(milliseconds), gesture.Released.Token).ConfigureAwait(false); }
        catch (OperationCanceledException) when (gesture.Released.IsCancellationRequested) { }
    }

    private void EndAction()
    {
        lock (gate) busy = false;
        BusyChanged?.Invoke(false);
    }
}
