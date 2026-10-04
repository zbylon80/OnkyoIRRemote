using System.Diagnostics;
using System.Windows;
using System.Windows.Threading;
using OnkyoRemote.Core;
using OnkyoRemote.Desktop;

namespace OnkyoRemote.Tests;

internal static class MediaKeyChecks
{
    private static void Assert(bool condition, string message)
    { if (!condition) throw new InvalidOperationException(message); }

    private static async Task Until(Func<bool> condition)
    {
        using var deadline = new CancellationTokenSource(4000);
        while (!condition()) await Task.Delay(10, deadline.Token);
    }

    public static async Task Run(string output)
    {
        // Every other virtual key, including STOP/PREV/NEXT, must pass through.
        var filter = new MediaKeyFilter();
        for (var key = 1; key < 255; key++)
        {
            if (MediaKeyFilter.IsMapped(key)) continue;
            Assert(!filter.Handle(key, true).Intercept && !filter.Handle(key, false).Intercept, "Unmapped key intercepted");
        }
        foreach (var key in new[] { MediaKeyFilter.Mute, MediaKeyFilter.Down, MediaKeyFilter.Up, MediaKeyFilter.PlayPause })
        {
            Assert(!filter.Handle(key, false).Intercept, "Unmatched release swallowed");
            Assert(filter.Handle(key, true) is { Intercept: true, Signal.Down: true }, "Down not intercepted");
            Assert(filter.Handle(key, true) is { Intercept: true, Signal: null }, "Auto-repeat triggered another action");
            Assert(filter.Handle(key, false) is { Intercept: true, Signal.Down: false }, "Release not intercepted");
        }
        filter.Disable();
        Assert(!filter.Handle(MediaKeyFilter.Up, true).Intercept, "Keys not restored after shutdown");
        var inherited = new MediaKeyFilter(new[] { MediaKeyFilter.PlayPause });
        Assert(!inherited.Handle(MediaKeyFilter.PlayPause, true).Intercept && !inherited.Handle(MediaKeyFilter.PlayPause, false).Intercept,
            "Startup consumed a key already held for Windows");
        Assert(inherited.Handle(MediaKeyFilter.PlayPause, true).Signal != null, "Inherited key did not recover");

        using var server = new FakeEsp32();
        using var api = new RemoteApi();
        var store = new SettingsStore(System.IO.Path.Combine(output, "media-settings.json"));
        store.Save(new WidgetSettings { Endpoint = server.Endpoint });
        // This window connects only to the fake device. Native-hook installation is
        // checked separately; no OS-level synthetic input or physical IR is emitted.
        var window = new MainWindow(api, store);
        window.Show();
        var liveFilter = new MediaKeyFilter();
        void Key(int key, bool down)
        {
            var decision = liveFilter.Handle(key, down);
            Assert(decision.Intercept, "Mapped event passed through while running");
            if (decision.Signal is MediaKeySignal signal) window.MediaKeys.Handle(signal);
        }
        try
        {
            await window.Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle);
            foreach (var pair in new[] { (MediaKeyFilter.Mute, "MUTE"), (MediaKeyFilter.PlayPause, "POWER") })
            {
                server.Clear();
                Key(pair.Item1, true);
                await window.Controller.Completion;
                Key(pair.Item1, true); // Even after HTTP completion, a held POWER cannot toggle twice.
                await Task.Delay(40);
                Key(pair.Item1, false);
                Assert(server.Requests.Count == 1 && server.Requests.Single().Body == "name=" + pair.Item2, "Media mapping repeated");
            }
            window.WindowState = WindowState.Minimized;
            foreach (var pair in new[] { (MediaKeyFilter.Up, "up"), (MediaKeyFilter.Down, "down") })
            {
                server.Clear();
                Key(pair.Item1, true);
                await Until(() => server.Requests.Any(r => r.Path == "/volume/keepalive"));
                Assert(window.WindowState == WindowState.Minimized && server.Requests.First().Body == "direction=" + pair.Item2, "Background hold failed");
                Key(pair.Item1, false);
                await window.Controller.Completion;
                Assert(server.Requests.Last().Path == "/volume/stop", "Media release failed");
                var count = server.Requests.Count;
                await Task.Delay(550);
                Assert(server.Requests.Count == count, "Media keepalive continued after release");
            }

            server.Clear();
            Key(MediaKeyFilter.Up, true); Key(MediaKeyFilter.Up, false);
            await window.Controller.Completion;
            Assert(server.Requests.Count == 2 && server.Requests.Last().Path == "/volume/stop", "Media quick tap was not one step");

            server.Clear();
            Key(MediaKeyFilter.Up, true);
            await window.Controller.Completion; // 3-second cap while the physical key stays down.
            var cappedCount = server.Requests.Count;
            Key(MediaKeyFilter.Up, true);
            await Task.Delay(30);
            Assert(server.Requests.Count == cappedCount, "Held key restarted after cap");
            Assert(window.Controller.BeginVolume("down"), "Could not start a later mouse gesture");
            Key(MediaKeyFilter.Up, false); // Late release belongs to the capped gesture, not the mouse.
            await Until(() => server.Requests.Count(r => r.Path == "/volume/start") == 2);
            window.Controller.ReleaseVolume(); await window.Controller.Completion;

            server.Clear();
            window.MediaKeys.Handle(new(MediaKeyFilter.PlayPause, true, Stopwatch.GetTimestamp() - 4 * Stopwatch.Frequency));
            Assert(server.Requests.IsEmpty && !window.Controller.IsBusy, "Stale POWER replayed");
            Key(MediaKeyFilter.Down, true);
            await Until(() => server.Requests.Any(r => r.Path == "/volume/start"));
            window.Close();
            await Until(() => !window.IsVisible);
            Assert(server.Requests.Last().Path == "/volume/stop", "Closing failed to stop media gesture");
            var closedCount = server.Requests.Count;
            window.MediaKeys.Handle(new(MediaKeyFilter.PlayPause, true, Stopwatch.GetTimestamp()));
            Assert(server.Requests.Count == closedCount, "Queued key ran after closing");
        }
        finally { window.ReleaseVolume(); await window.Controller.Completion; if (window.IsVisible) window.Close(); }

        using var hook = new MediaKeyHook(_ => { });
        Assert(hook.IsRunning, "Native hook did not register");
        hook.Dispose();
        Assert(!hook.IsRunning, "Native hook did not stop");
    }
}
