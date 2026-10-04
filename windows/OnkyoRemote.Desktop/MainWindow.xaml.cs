using System.ComponentModel;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using OnkyoRemote.Core;

namespace OnkyoRemote.Desktop;

public partial class MainWindow : Window
{
    private readonly SettingsStore store;
    private readonly WidgetSettings settings;
    private readonly RemoteApi? ownedApi;
    public RemoteController Controller { get; }
    public MediaKeyActions MediaKeys { get; }
    private readonly bool enableMediaKeys;
    private MediaKeyHook? mediaHook;
    private bool closing, allowClose;

    public MainWindow() : this(null, null, true) { }

    public MainWindow(IRemoteApi? api, SettingsStore? settingsStore, bool enableMediaKeys = false)
    {
        InitializeComponent();
        store = settingsStore ?? new();
        settings = store.Load();
        Controller = new RemoteController(api ?? (ownedApi = new RemoteApi()), settings.Endpoint);
        MediaKeys = new MediaKeyActions(Controller);
        this.enableMediaKeys = enableMediaKeys;
        Width = settings.Width; Height = settings.Height;
        if (settings.Left is double left && settings.Top is double top && double.IsFinite(left) && double.IsFinite(top))
        {
            // Keep the window reachable after disconnecting a monitor.
            WindowStartupLocation = WindowStartupLocation.Manual;
            Left = Math.Clamp(left, SystemParameters.VirtualScreenLeft,
                Math.Max(SystemParameters.VirtualScreenLeft, SystemParameters.VirtualScreenLeft + SystemParameters.VirtualScreenWidth - Width));
            Top = Math.Clamp(top, SystemParameters.VirtualScreenTop,
                Math.Max(SystemParameters.VirtualScreenTop, SystemParameters.VirtualScreenTop + SystemParameters.VirtualScreenHeight - Height));
        }
        Topmost = settings.Pinned;
        UpdatePin();
        EndpointInput.Text = settings.Endpoint;
        foreach (var key in new[] { VolumeDown, VolumeUp })
        {
            key.BeginHold = () => Controller.BeginVolume((string)key.Tag);
            key.EndHold = Controller.ReleaseVolume;
        }
        Controller.BusyChanged += busy => Dispatcher.BeginInvoke(() => SetBusy(busy));
        Controller.StatusChanged += (text, success) => Dispatcher.BeginInvoke(() =>
        {
            StatusText.Text = text;
            StatusText.Foreground = new SolidColorBrush(success ? Color.FromRgb(187, 181, 170) : Color.FromRgb(232, 157, 142));
            SettingsStatus.Text = text;
        });
    }

    protected override void OnSourceInitialized(EventArgs e)
    {
        base.OnSourceInitialized(e);
        if (!enableMediaKeys) return;
        try
        {
            mediaHook = new MediaKeyHook(signal => { _ = Dispatcher.BeginInvoke(() => MediaKeys.Handle(signal)); });
            InputModeText.Text = "Klawiatura → Onkyo · maks. 3 s";
            InputModeText.ToolTip = "VOL−/VOL+ i MUTE sterują Onkyo. PLAY/PAUSE = POWER. Zamknij widżet, aby przywrócić funkcje Windows.";
        }
        catch (Exception e2) when (e2 is InvalidOperationException or TimeoutException)
        {
            InputModeText.Text = "Klawisze multimedialne niedostępne";
            InputModeText.ToolTip = "Przyciski na widżecie nadal działają. Uruchom ponownie aplikację, aby ponowić przejęcie klawiszy.";
        }
    }

    private void SetBusy(bool busy)
    {
        // Query current state, since worker completion can precede queued UI updates.
        busy = Controller.IsBusy;
        foreach (var button in Descendants<Button>(RemoteKeys))
            button.IsEnabled = !busy || button is VolumeButton { IsHolding: true };
        SettingsButton.IsEnabled = !busy;
        CheckButton.IsEnabled = !busy;
        SaveButton.IsEnabled = !busy;
    }

    private static IEnumerable<T> Descendants<T>(DependencyObject root) where T : DependencyObject
    {
        for (var i = 0; i < VisualTreeHelper.GetChildrenCount(root); i++)
        {
            var child = VisualTreeHelper.GetChild(root, i);
            if (child is T match) yield return match;
            foreach (var nested in Descendants<T>(child)) yield return nested;
        }
    }

    private async void Command_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button { IsEnabled: true, Tag: string command }) await Controller.CommandAsync(command);
    }

    private void Header_Drag(object sender, MouseButtonEventArgs e)
    {
        if (e.OriginalSource is DependencyObject source)
        {
            for (var node = source; node != null && node != Header; node = VisualTreeHelper.GetParent(node))
                if (node is Button) return;
        }
        if (e.LeftButton == MouseButtonState.Pressed) DragMove();
    }

    private void Pin_Click(object sender, RoutedEventArgs e)
    {
        Topmost = !Topmost;
        UpdatePin();
        SaveSettings();
    }

    private void UpdatePin()
    {
        PinButton.Content = Topmost ? "◆" : "◇";
        PinButton.ToolTip = Topmost ? "Odepnij — zwykłe okno" : "Przypnij — zawsze na wierzchu";
        System.Windows.Automation.AutomationProperties.SetName(PinButton, (string)PinButton.ToolTip);
    }

    private void Settings_Click(object sender, RoutedEventArgs e)
    {
        var open = SettingsPanel.Visibility != Visibility.Visible;
        SettingsPanel.Visibility = open ? Visibility.Visible : Visibility.Collapsed;
        RemoteKeys.Visibility = open ? Visibility.Collapsed : Visibility.Visible;
        if (open) { EndpointInput.Text = Controller.Endpoint; SettingsStatus.Text = ""; EndpointInput.Focus(); }
    }

    private async void Check_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            await Controller.CheckAsync(EndpointInput.Text);
        }
        catch (ArgumentException ex) { SettingsStatus.Text = ex.Message; }
        catch (InvalidOperationException ex) { SettingsStatus.Text = ex.Message; }
    }

    private void Save_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            Controller.SetEndpoint(EndpointInput.Text);
            if (!SaveSettings()) return;
            SettingsPanel.Visibility = Visibility.Collapsed;
            RemoteKeys.Visibility = Visibility.Visible;
            StatusText.Text = "Gotowy · " + Controller.Endpoint;
        }
        catch (ArgumentException ex) { SettingsStatus.Text = ex.Message; }
        catch (InvalidOperationException ex) { SettingsStatus.Text = ex.Message; }
    }

    private void Endpoint_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter) { e.Handled = true; Save_Click(sender, e); }
    }

    private bool SaveSettings()
    {
        settings.Endpoint = Controller.Endpoint;
        settings.Pinned = Topmost;
        var bounds = WindowState == WindowState.Normal ? new Rect(Left, Top, ActualWidth, ActualHeight) : RestoreBounds;
        settings.Left = bounds.Left; settings.Top = bounds.Top;
        settings.Width = bounds.Width; settings.Height = bounds.Height;
        try { store.Save(settings); return true; }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        { SettingsStatus.Text = StatusText.Text = "Nie udało się zapisać ustawień."; return false; }
    }

    private void Minimize_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
    private void Close_Click(object sender, RoutedEventArgs e) => Close();
    private void Window_Deactivated(object? sender, EventArgs e) => ReleaseLocalVolume();
    private void Window_StateChanged(object? sender, EventArgs e) { if (WindowState != WindowState.Normal) ReleaseLocalVolume(); }

    private void Window_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape) { ReleaseVolume(); e.Handled = true; }
    }

    public void ReleaseVolume()
    {
        ReleaseLocalVolume(); MediaKeys.CancelVolume(); Controller.ReleaseVolume();
    }

    private void ReleaseLocalVolume() { VolumeDown.Release(); VolumeUp.Release(); }

    private async void Window_Closing(object? sender, CancelEventArgs e)
    {
        if (allowClose) return;
        e.Cancel = true;
        if (closing) return;
        closing = true;
        mediaHook?.Dispose();
        MediaKeys.Dispose();
        ReleaseVolume();
        IsEnabled = false;
        await Controller.Completion;
        SaveSettings();
        ownedApi?.Dispose();
        allowClose = true;
        // Completion may already be synchronous; wait until the first Closing event returns.
        _ = Dispatcher.BeginInvoke(new Action(Close));
    }
}
