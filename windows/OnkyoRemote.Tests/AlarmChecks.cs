using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using OnkyoRemote.Core;
using OnkyoRemote.Desktop;

namespace OnkyoRemote.Tests;

internal static class AlarmChecks
{
    internal static string Snapshot(bool on = false, bool off = false, bool clock = true, bool storage = true,
        string onTime = "07:00", string offTime = "02:00") => JsonSerializer.Serialize(new
        {
            clockReady = clock, storageReady = storage, localTime = "2026-10-07 12:00:00",
            on = new { enabled = on, time = onTime, date = "2026-10-08", source = "CD" },
            off = new { enabled = off, time = offTime, date = "2026-10-08" }
        });

    private static void Assert(bool value, string message) { if (!value) throw new InvalidOperationException(message); }
    private static async Task Until(Func<bool> condition)
    { using var timeout = new CancellationTokenSource(4000); while (!condition()) await Task.Delay(10, timeout.Token); }

    public static async Task Run(string output)
    {
        using var server = new FakeEsp32(); using var api = new RemoteApi();
        var reply = Snapshot();
        server.Respond = r => r.Path == "/alarms" ? new Reply(reply) : FakeEsp32.Default(r);
        var controller = new RemoteController(api, server.Endpoint);
        var read = await controller.AlarmsAsync();
        Assert(!read.On.Enabled && read.Source == "CD" && server.Requests.Single().Method == "GET", "Alarm open must only read");
        server.Clear(); reply = Snapshot(on: true, onTime: "23:59");
        await controller.ChangeAlarmAsync("on", "23:59", "CD");
        Assert(server.Requests.Single().Body == "action=on&onTime=23%3A59&source=CD", "Wake form encoding");
        server.Clear(); await controller.ChangeAlarmAsync("off", "00:00");
        Assert(server.Requests.Single().Body == "action=off&offTime=00%3A00", "Off form must preserve wake");
        server.Clear(); await controller.ChangeAlarmAsync("cancelOn");
        Assert(server.Requests.Single().Body == "action=cancelOn", "Cancel must only change selected action");
        server.Clear();
        foreach (var time in new[] { "24:00", "23:60", "7:00", "aa:00" })
        {
            try { await controller.ChangeAlarmAsync("on", time, "CD"); throw new InvalidOperationException("Invalid time accepted"); }
            catch (ArgumentException) { }
        }
        Assert(server.Requests.IsEmpty && !controller.IsBusy, "Validation leaked a write or busy gate");
        foreach (var bad in new[] { new Reply("{\"error\":\"Godziny kolidują\"}", 400), new Reply("{}"), new Reply(Snapshot(), Delay: 2300) })
        {
            server.Clear(); server.Respond = _ => bad;
            try { await controller.ChangeAlarmAsync("on", "07:00", "CD"); throw new InvalidOperationException("Invalid reply accepted"); }
            catch (Exception e) when (e is IOException or KeyNotFoundException or OperationCanceledException) { }
            Assert(server.Requests.Count == 1 && !controller.IsBusy, "Unknown write retried or gate locked");
        }
        server.Respond = r => r.Path == "/alarms" ? new Reply(reply, Delay: 100) : FakeEsp32.Default(r);
        server.Clear(); var pending = controller.AlarmsAsync();
        Assert(!await controller.CommandAsync("POWER") && !controller.BeginVolume("up"), "Alarm request must serialize manual controls");
        await pending; Assert(server.Requests.Count == 1, "Alarm request emitted IR");

        var store = new SettingsStore(Path.Combine(output, "alarm-settings.json"));
        store.Save(new WidgetSettings { Endpoint = server.Endpoint });
        var window = new MainWindow(api, store); window.Show();
        var panel = (AlarmPanel)window.FindName("Alarms");
        T Field<T>(string name) where T : FrameworkElement => (T)panel.FindName(name);
        async Task Idle() => await window.Dispatcher.InvokeAsync(() => window.UpdateLayout(), DispatcherPriority.ApplicationIdle);
        try
        {
            reply = Snapshot(); server.Clear();
            ((Button)window.FindName("ScheduleButton")).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            await Until(() => panel.IsVisible && Field<Button>("OnSet").IsEnabled);
            Assert(server.Requests.Single().Path == "/alarms" && !((FrameworkElement)window.FindName("RemoteKeys")).IsVisible, "Clock did not open native panel");
            var onTime = Field<TimeSelector>("OnTime");
            var offTime = Field<TimeSelector>("OffTime");
            onTime.Time = "23:59"; offTime.Time = "00:30";
            T TimeField<T>(string name) where T : FrameworkElement => (T)onTime.FindName(name);
            void Click(string name) => TimeField<Button>(name).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            var editor = TimeField<Popup>("Editor");
            Click("TimeButton"); await Idle();
            Assert(editor.IsOpen && TimeField<TextBox>("HourInput").Text == "23", "Editor did not open with current time");
            Click("HourDown"); Click("MinuteDown");
            Assert(TimeField<TextBox>("HourInput").Text == "00" && TimeField<TextBox>("MinuteInput").Text == "00", "24-hour controls did not wrap");
            Click("HourUp"); Click("MinuteUp");
            Assert(TimeField<TextBox>("HourInput").Text == "23" && TimeField<TextBox>("MinuteInput").Text == "59", "Reverse wrap failed");
            TimeField<TextBox>("HourInput").Text = "24"; TimeField<TextBox>("MinuteInput").Text = "60";
            Click("DoneButton");
            Assert(editor.IsOpen && onTime.Time == "23:59" && TimeField<TextBlock>("ErrorText").Text.Length > 0, "Invalid time was committed");
            TimeField<TextBox>("HourInput").Text = "0"; TimeField<TextBox>("MinuteInput").Text = "5";
            Click("DoneButton"); Assert(!editor.IsOpen && onTime.Time == "00:05", "Keyboard draft not normalized");
            Click("TimeButton"); await Idle(); Click("HourUp"); Click("CancelButton");
            Assert(onTime.Time == "00:05", "Cancel changed committed time");
            Click("TimeButton"); await Idle();
            TimeField<TextBox>("HourInput").RaiseEvent(new KeyEventArgs(Keyboard.PrimaryDevice, PresentationSource.FromVisual(TimeField<TextBox>("HourInput")), 0, Key.Escape) { RoutedEvent = Keyboard.PreviewKeyDownEvent });
            Assert(!editor.IsOpen && onTime.Time == "00:05", "Escape did not cancel editor");
            Click("TimeButton"); await Idle(); onTime.IsEnabled = false;
            Assert(!editor.IsOpen, "Disabled control left editor open"); onTime.IsEnabled = true;
            Assert(server.Requests.Count == 1, "Editing the clock sent an HTTP request");
            onTime.Time = "23:59";
            reply = Snapshot(on: true, onTime: "23:59"); server.Clear();
            Field<Button>("OnSet").RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            await Until(() => Field<TextBlock>("ResultText").Text.StartsWith("Ustawione"));
            Assert(offTime.Time == "00:30", "Wake save lost off draft");
            Assert(server.Requests.Single().Body == "action=on&onTime=23%3A59&source=CD", "Native Set form");
            foreach (var size in new[] { (300d, 550d, "alarm.png"), (280d, 520d, "alarm-small.png"), (400d, 700d, "alarm-large.png") })
            {
                window.Width = size.Item1; window.Height = size.Item2; await Idle();
                Assert(onTime.ActualWidth >= 130 && Field<ComboBox>("Source").ActualWidth >= 130, "Alarm controls clipped");
                var bitmap = new RenderTargetBitmap((int)window.ActualWidth, (int)window.ActualHeight, 96, 96, PixelFormats.Pbgra32); bitmap.Render(window);
                var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(bitmap));
                using var file = File.Create(Path.Combine(output, size.Item3)); encoder.Save(file);
            }
            window.Width = 280; window.Height = 520; await Idle();
            var scroll = Field<ScrollViewer>("PanelScroll");
            var scrollBar = (ScrollBar)scroll.Template.FindName("PART_VerticalScrollBar", scroll);
            Assert(scrollBar.ActualWidth <= 8, $"System scrollbar remains: {scrollBar.ActualWidth}");
            scroll.ScrollToBottom(); await Idle();
            Assert(scroll.VerticalOffset > 0 && Field<Button>("RefreshButton").TransformToAncestor(scroll).Transform(new Point()).Y < scroll.ActualHeight, "Small panel footer cannot be reached");
            scroll.ScrollToTop(); await Idle();
            Click("TimeButton"); await Idle();
            Save((FrameworkElement)editor.Child, "time-editor.png"); Click("CancelButton");
            var source = Field<ComboBox>("Source"); source.IsDropDownOpen = true; await Idle();
            var sourcePopup = (Popup)source.Template.FindName("PART_Popup", source);
            Save((FrameworkElement)sourcePopup.Child, "source-picker.png"); source.IsDropDownOpen = false;
            void Save(FrameworkElement view, string name)
            {
                view.UpdateLayout();
                var image = new RenderTargetBitmap((int)Math.Ceiling(view.ActualWidth), (int)Math.Ceiling(view.ActualHeight), 96, 96, PixelFormats.Pbgra32);
                image.Render(view); var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(image));
                using var file = File.Create(Path.Combine(output, name)); encoder.Save(file);
            }
            reply = Snapshot(on: true, clock: false); server.Clear();
            Field<Button>("RefreshButton").RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            await Until(() => Field<TextBlock>("ClockText").Text.Contains("czeka"));
            Assert(!Field<Button>("OnSet").IsEnabled && Field<Button>("OnCancel").IsEnabled, "NTP must block Set but allow Cancel");
            reply = Snapshot(clock: false); server.Clear();
            Field<Button>("OnCancel").RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            await Until(() => Field<TextBlock>("ResultText").Text == "Anulowano.");
            Assert(server.Requests.Single().Body == "action=cancelOn", "Native Cancel form");
            server.Respond = _ => new Reply("{\"error\":\"błąd testowy\"}", 500);
            Field<Button>("RefreshButton").RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            await Until(() => Field<TextBlock>("ResultText").Text.Contains("błąd testowy"));
            Assert(!Field<Button>("OnCancel").IsEnabled && !Field<Button>("OnSet").IsEnabled && Field<Button>("RefreshButton").IsEnabled, "Read failure requires refresh");
            Field<Button>("BackButton").RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            Assert(!panel.IsVisible && ((FrameworkElement)window.FindName("RemoteKeys")).IsVisible, "Native Back did not return to remote");
        }
        finally { window.Close(); await Until(() => !window.IsVisible); }
    }
}
