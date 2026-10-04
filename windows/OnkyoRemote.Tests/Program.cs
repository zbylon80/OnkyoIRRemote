using System.IO;
using System.Windows;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using OnkyoRemote.Core;
using OnkyoRemote.Desktop;

namespace OnkyoRemote.Tests;

internal static class Program
{
    private static readonly List<string> results = [];
    private static string output = "";
    private static int exitCode;

    [STAThread]
    private static int Main(string[] args)
    {
        output = Path.GetFullPath(args.Length > 0 ? args[0] : "artifacts/tests");
        Directory.CreateDirectory(output);
        var app = new Application { ShutdownMode = ShutdownMode.OnExplicitShutdown };
        app.Startup += async (_, _) =>
        {
            try
            {
                await ProtocolTests();
                await WindowTests();
                await MediaKeyChecks.Run(output);
                results.Add("PASS: native media-hook registration/removal; exactly 4 intercepted keys; other keys passed through; POWER/MUTE once per press; volume both directions while minimized; release; quick tap; 3s cap; gesture ownership; stale/queued event rejection; shutdown restores default routing.");
                results.Add("PASS: all Windows widget checks. Physical ESP32 was not contacted.");
            }
            catch (Exception e) { results.Add("FAIL: " + e); exitCode = 1; }
            finally
            {
                File.WriteAllLines(Path.Combine(output, "result.txt"), results);
                foreach (var result in results) Console.WriteLine(result);
                app.Shutdown(exitCode);
            }
        };
        app.Run();
        return exitCode;
    }

    private static void Assert(bool condition, string message)
    { if (!condition) throw new InvalidOperationException(message); }

    private static async Task Until(Func<bool> condition)
    {
        using var timeout = new CancellationTokenSource(4000);
        while (!condition()) await Task.Delay(10, timeout.Token);
    }

    private static async Task ProtocolTests()
    {
        Assert(RemoteEndpoint.Normalize(" 192.168.1.46 ") == RemoteEndpoint.Default, "IP normalization");
        foreach (var invalid in new[] { "", "https://192.168.1.46", "http://user:pass@host", "http://host/command", "http://host?name=POWER", "http://host#x", "http://host:0", "http://host:99999", "http://host\\command" })
        {
            var rejected = false;
            try { RemoteEndpoint.Normalize(invalid); } catch (ArgumentException) { rejected = true; }
            Assert(rejected, "Accepted invalid endpoint: " + invalid);
        }
        using var server = new FakeEsp32();
        using var api = new RemoteApi();
        var controller = new RemoteController(api, server.Endpoint);
        var lastSuccess = false;
        controller.StatusChanged += (_, success) => lastSuccess = success;
        foreach (var command in RemoteApi.Commands) Assert(await controller.CommandAsync(command), "Command failed: " + command);
        Assert(server.Requests.Count == 11 && server.Requests.All(r => r.Method == "POST" && r.Path == "/command"), "Basic mappings");
        Assert(server.Requests.Select(r => r.Body).SequenceEqual(RemoteApi.Commands.Select(c => "name=" + Uri.EscapeDataString(c))), "Command forms");
        server.Clear();
        Assert(await controller.CheckAsync() && server.Requests.Single().Path == "/version" && server.Requests.Single().Method == "GET", "Check must be read-only");

        foreach (var direction in new[] { "up", "down" })
        {
            server.Clear();
            Assert(controller.BeginVolume(direction), "Could not start hold");
            await Until(() => server.Requests.Any(r => r.Path == "/volume/keepalive"));
            Assert(!await controller.CommandAsync("POWER") && !controller.BeginVolume("up"), "Concurrent actions must be dropped");
            var blocked = false;
            try { controller.SetEndpoint(RemoteEndpoint.Default); } catch (InvalidOperationException) { blocked = true; }
            Assert(blocked, "Endpoint changed during a hold");
            Assert(!server.Requests.Any(r => r.Path == "/volume/stop"), "Stopped while held");
            controller.ReleaseVolume();
            await controller.Completion;
            var count = server.Requests.Count;
            await Task.Delay(550);
            Assert(lastSuccess && count == server.Requests.Count, "Keepalive continued after release");
            Assert(server.Requests.First().Body == "direction=" + direction && server.Requests.Last().Path == "/volume/stop", "Hold sequence");
            Assert(server.Requests.Skip(1).All(r => r.Body == "session=123"), "Session mismatch");
        }
        server.Clear();
        controller.BeginVolume("up"); controller.ReleaseVolume(); await controller.Completion;
        Assert(server.Requests.Select(r => r.Path).SequenceEqual(new[] { "/volume/press", "/volume/stop" }), "Quick tap must be one step");

        server.Clear();
        server.Respond = r => r.Path == "/volume/press" ? new Reply("{\"ok\":true,\"session\":\"123\"}", Delay: 220) : FakeEsp32.Default(r);
        controller.BeginVolume("down"); await Task.Delay(40); controller.ReleaseVolume(); await controller.Completion;
        Assert(server.Requests.Select(r => r.Path).SequenceEqual(new[] { "/volume/press", "/volume/stop" }), "Late press acknowledgment started repeat");

        server.Respond = null; server.Clear();
        controller.BeginVolume("up"); await controller.Completion;
        Assert(server.Requests.Last().Path == "/volume/stop" && !controller.IsBusy, "3s cap failed");
        var capped = server.Requests.Count;
        await Task.Delay(550); Assert(capped == server.Requests.Count, "Repeat restarted after cap");

        foreach (var token in new[] { "\"18446744073709551616\"", "\"0\"", "123", "\"bad\"" })
        {
            server.Clear(); server.Respond = _ => new Reply("{\"ok\":true,\"session\":" + token + "}");
            controller.BeginVolume("up"); await controller.Completion;
            Assert(!lastSuccess && server.Requests.Count == 1, "Invalid session accepted or retried");
        }

        foreach (var failedPath in new[] { "/volume/start", "/volume/keepalive", "/volume/stop" })
        {
            server.Clear(); server.Respond = r => r.Path == failedPath ? new Reply("{\"ok\":false}", 409) : FakeEsp32.Default(r);
            controller.BeginVolume("down");
            if (failedPath == "/volume/stop") controller.ReleaseVolume();
            await controller.Completion;
            Assert(!lastSuccess && server.Requests.Count(r => r.Path == failedPath) == 1 && server.Requests.Last().Path == "/volume/stop", "Failed hold was retried or lacked stop");
        }

        foreach (var reply in new[] { new Reply("{\"ok\":false}"), new Reply("not JSON"), new Reply("{\"ok\":true}", 500), new Reply("{}", 302), new Reply(new string('x', 2049)), new Reply("{\"ok\":true}", Delay: 2200) })
        {
            server.Clear(); server.Respond = _ => reply;
            Assert(!await controller.CommandAsync("POWER"), "Bad response accepted");
            await Task.Delay(40); Assert(server.Requests.Count == 1, "Ambiguous command retried");
        }
        server.Clear(); server.Respond = null;
        Assert(await controller.CommandAsync("MUTE"), "Recovery failed");
        results.Add("PASS: endpoint validation; 11 Basic mappings; read-only version; both volume directions; quick tap; delayed acknowledgment; release; concurrent drop; 3s cap; invalid sessions; start/keepalive/stop errors; HTTP/JSON/size/timeout failures; no retries; recovery.");
    }

    private static async Task WindowTests()
    {
        using var server = new FakeEsp32();
        using var api = new RemoteApi();
        var store = new SettingsStore(Path.Combine(output, "settings.json"));
        store.Save(new WidgetSettings { Endpoint = server.Endpoint });
        var window = new MainWindow(api, store);
        window.Show();
        try
        {
            await window.Dispatcher.InvokeAsync(() => window.UpdateLayout(), DispatcherPriority.ApplicationIdle);
            var buttons = Descendants<Button>((DependencyObject)window.FindName("RemoteKeys")).ToArray();
            Assert(buttons.Length == 11 && buttons.All(b => b.ActualHeight >= 40), "Missing or clipped Basic keys");
            Assert(window.Icon != null && window.Icon.Width > 0, "Missing window icon");
            foreach (var size in new[] { (300d, 550d, "widget.png"), (280d, 520d, "widget-small.png"), (400d, 700d, "widget-large.png") })
            {
                window.Width = size.Item1; window.Height = size.Item2;
                await window.Dispatcher.InvokeAsync(() => window.UpdateLayout(), DispatcherPriority.ApplicationIdle);
                Assert(buttons.All(b => b.ActualHeight >= 40 && b.ActualWidth >= 45), "Keys clipped after resize");
                Render(window, size.Item3);
            }
            window.Width = 300; window.Height = 550;

            // Sample real rendered key faces during a slow request: the whole keypad
            // must keep its brightness while commands remain blocked.
            await window.Dispatcher.InvokeAsync(() => window.UpdateLayout(), DispatcherPriority.ApplicationIdle);
            Keyboard.ClearFocus();
            var panel = (FrameworkElement)window.FindName("RemoteKeys");
            var before = KeyFacePixels(panel, buttons);
            server.Clear();
            server.Respond = r => r.Path == "/command" ? new Reply("{\"ok\":true}", Delay: 250) : FakeEsp32.Default(r);
            var pending = window.Controller.CommandAsync("POWER");
            await Until(() => server.Requests.Any(r => r.Path == "/command"));
            await window.Dispatcher.InvokeAsync(() => window.UpdateLayout(), DispatcherPriority.ApplicationIdle);
            Assert(buttons.All(b => !b.IsEnabled), "Busy input gate was removed");
            Assert(before.SequenceEqual(KeyFacePixels(panel, buttons)), "Keypad brightness changed during request");
            Render(window, "widget-busy.png");
            Assert(await pending, "Slow command failed");
            await window.Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle);
            server.Respond = null;

            foreach (var button in buttons.Where(b => b is not VolumeButton))
            {
                server.Clear();
                button.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
                await Until(() => !window.Controller.IsBusy);
                Assert(server.Requests.Single().Body == "name=" + Uri.EscapeDataString((string)button.Tag), "UI mapping mismatch");
            }
            var up = (VolumeButton)window.FindName("VolumeUp");
            var down = (VolumeButton)window.FindName("VolumeDown");
            foreach (var key in new[] { up, down })
            {
                server.Clear();
                await window.Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle);
                Mouse(key, UIElement.PreviewMouseLeftButtonDownEvent);
                Assert(key.IsHolding && window.Controller.IsBusy, $"Mouse press did not begin: enabled={key.IsEnabled}, capture={key.IsMouseCaptured}, active={window.IsActive}");
                await Until(() => server.Requests.Any(r => r.Path == "/volume/keepalive"));
                Mouse(key, UIElement.PreviewMouseLeftButtonUpEvent);
                await window.Controller.Completion;
                Assert(server.Requests.Last().Path == "/volume/stop" && !key.IsHolding, "UI release failed");
            }
            server.Clear();
            await window.Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle);
            Mouse(up, UIElement.PreviewMouseLeftButtonDownEvent);
            await Until(() => server.Requests.Any(r => r.Path == "/volume/start"));
            up.ReleaseMouseCapture();
            await window.Controller.Completion;
            Assert(!up.IsHolding && server.Requests.Last().Path == "/volume/stop", "Lost capture failed to stop");

            server.Clear();
            await window.Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle);
            var peer = new ButtonAutomationPeer(down);
            ((IInvokeProvider)peer.GetPattern(PatternInterface.Invoke)).Invoke();
            await Until(() => server.Requests.Count >= 2 && !window.Controller.IsBusy);
            Assert(server.Requests.Select(r => r.Path).SequenceEqual(new[] { "/volume/press", "/volume/stop" }), "Accessibility invoke was not one step");

            await window.Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle);
            server.Clear(); up.Focus();
            Key(up, Keyboard.PreviewKeyDownEvent, System.Windows.Input.Key.Space);
            Key(up, Keyboard.PreviewKeyDownEvent, System.Windows.Input.Key.Space);
            await Until(() => server.Requests.Any(r => r.Path == "/volume/keepalive"));
            Key(up, Keyboard.PreviewKeyUpEvent, System.Windows.Input.Key.Space);
            await window.Controller.Completion;
            Assert(server.Requests.Count(r => r.Path == "/volume/press") == 1 && server.Requests.Last().Path == "/volume/stop", "Keyboard repeated or failed to stop");

            await window.Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle);
            server.Clear();
            Key(up, Keyboard.PreviewKeyDownEvent, System.Windows.Input.Key.Return);
            await Until(() => server.Requests.Any(r => r.Path == "/volume/start"));
            Key(up, Keyboard.PreviewKeyDownEvent, System.Windows.Input.Key.Escape);
            await window.Controller.Completion;
            Assert(server.Requests.Last().Path == "/volume/stop" && !up.IsHolding, "Escape failed to stop");

            await window.Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle);

            ((Button)window.FindName("PinButton")).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            Assert(window.Topmost && store.Load().Pinned, "Pin was not saved");
            ((Button)window.FindName("SettingsButton")).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            Render(window, "settings.png");
            var input = (TextBox)window.FindName("EndpointInput");
            input.Text = "http://host/command";
            ((Button)window.FindName("SaveButton")).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            Assert(window.Controller.Endpoint == server.Endpoint && ((FrameworkElement)window.FindName("SettingsPanel")).IsVisible, "Invalid settings accepted");
            input.Text = server.Endpoint;
            ((Button)window.FindName("SaveButton")).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            Assert(!((FrameworkElement)window.FindName("SettingsPanel")).IsVisible, "Settings did not close");

            server.Clear();
            await window.Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle);
            Mouse(up, UIElement.PreviewMouseLeftButtonDownEvent);
            await Until(() => server.Requests.Any(r => r.Path == "/volume/start"));
            window.WindowState = WindowState.Minimized;
            await window.Controller.Completion;
            Assert(server.Requests.Last().Path == "/volume/stop", "Minimizing did not stop hold");
            window.WindowState = WindowState.Normal;
            await Task.Delay(100);
            Mouse(down, UIElement.PreviewMouseLeftButtonDownEvent);
            await Until(() => server.Requests.Any(r => r.Path == "/volume/press" && r.Body == "direction=down"));
            window.Close();
            await Until(() => !window.IsVisible);
            Assert(server.Requests.Last().Path == "/volume/stop", "Closing did not stop hold");
            results.Add("PASS: WPF layout at 3 sizes; window icon; stable keypad brightness during requests with input gate retained; all Basic UI mappings; mouse down/up; lost capture; keyboard hold/release/Escape; accessibility single step; pin/settings persistence; invalid address; minimize/close stop. PNG previews saved.");
        }
        finally { window.ReleaseVolume(); await window.Controller.Completion; if (window.IsVisible) window.Close(); }
    }

    private static void Mouse(VolumeButton button, RoutedEvent routedEvent) => button.RaiseEvent(
        new MouseButtonEventArgs(System.Windows.Input.Mouse.PrimaryDevice, Environment.TickCount, MouseButton.Left) { RoutedEvent = routedEvent });

    private static void Key(VolumeButton button, RoutedEvent routedEvent, System.Windows.Input.Key key) => button.RaiseEvent(
        new KeyEventArgs(Keyboard.PrimaryDevice, PresentationSource.FromVisual(button), Environment.TickCount, key) { RoutedEvent = routedEvent });

    private static IEnumerable<T> Descendants<T>(DependencyObject root) where T : DependencyObject
    {
        for (int i = 0; i < VisualTreeHelper.GetChildrenCount(root); i++)
        {
            var child = VisualTreeHelper.GetChild(root, i);
            if (child is T match) yield return match;
            foreach (var nested in Descendants<T>(child)) yield return nested;
        }
    }

    private static void Render(Window window, string name)
    {
        window.UpdateLayout();
        var bitmap = new RenderTargetBitmap((int)window.ActualWidth, (int)window.ActualHeight, 96, 96, PixelFormats.Pbgra32);
        bitmap.Render(window);
        var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(bitmap));
        using var file = File.Create(Path.Combine(output, name)); encoder.Save(file);
    }

    private static byte[] KeyFacePixels(FrameworkElement panel, IEnumerable<Button> buttons)
    {
        panel.UpdateLayout();
        var bitmap = new RenderTargetBitmap((int)panel.ActualWidth, (int)panel.ActualHeight, 96, 96, PixelFormats.Pbgra32);
        bitmap.Render(panel);
        using var samples = new MemoryStream();
        foreach (var button in buttons)
        {
            var point = button.TranslatePoint(new Point(10, 10), panel);
            var pixel = new byte[4];
            bitmap.CopyPixels(new Int32Rect((int)point.X, (int)point.Y, 1, 1), pixel, 4, 0);
            samples.Write(pixel);
        }
        return samples.ToArray();
    }
}
