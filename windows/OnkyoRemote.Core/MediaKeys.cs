using System.Diagnostics;

namespace OnkyoRemote.Core;

public readonly record struct MediaKeySignal(int Key, bool Down, long Timestamp);
public readonly record struct MediaKeyDecision(bool Intercept, MediaKeySignal? Signal);

/// <summary>Only these four keys are consumed. No ordinary keystrokes are retained.</summary>
public sealed class MediaKeyFilter(IEnumerable<int>? initiallyHeld = null)
{
    public const int Mute = 0xAD, Down = 0xAE, Up = 0xAF, PlayPause = 0xB3;
    private readonly HashSet<int> held = [];
    private readonly HashSet<int> inherited = new((initiallyHeld ?? []).Where(IsMapped));
    private bool enabled = true;
    public static bool IsMapped(int key) => key is Mute or Down or Up or PlayPause;

    public MediaKeyDecision Handle(int key, bool down)
    {
        if (!enabled || !IsMapped(key)) return new(false, null);
        // A key already held before startup belongs to Windows until its release.
        if (inherited.Contains(key))
        {
            if (!down) inherited.Remove(key);
            return new(false, null);
        }
        if (down)
            return new(true, held.Add(key) ? new(key, true, Stopwatch.GetTimestamp()) : null);
        return held.Remove(key) ? new(true, new(key, false, Stopwatch.GetTimestamp())) : new(false, null);
    }

    public void Disable() { enabled = false; held.Clear(); inherited.Clear(); }
}

/// <summary>Runs outside the native hook. Release refers to the exact gesture it started.</summary>
public sealed class MediaKeyActions(RemoteController controller) : IDisposable
{
    private readonly Dictionary<int, long> gestures = [];
    private bool disposed;

    public void Handle(MediaKeySignal signal)
    {
        if (disposed || !MediaKeyFilter.IsMapped(signal.Key)) return;
        if (!signal.Down)
        {
            if (gestures.Remove(signal.Key, out var gestureId)) controller.ReleaseVolume(gestureId);
            return;
        }
        // Do not replay an old down event after a blocked UI thread or resume.
        if (Stopwatch.GetElapsedTime(signal.Timestamp).TotalSeconds >= 3) return;
        if (signal.Key is MediaKeyFilter.Down or MediaKeyFilter.Up)
        {
            if (controller.TryBeginVolume(signal.Key == MediaKeyFilter.Up ? "up" : "down", out var id, signal.Timestamp))
                gestures[signal.Key] = id;
        }
        else _ = controller.CommandAsync(signal.Key == MediaKeyFilter.Mute ? "MUTE" : "POWER");
    }

    public void CancelVolume()
    {
        foreach (var id in gestures.Values) controller.ReleaseVolume(id);
        gestures.Clear();
    }

    public void Dispose() { disposed = true; CancelVolume(); }
}
