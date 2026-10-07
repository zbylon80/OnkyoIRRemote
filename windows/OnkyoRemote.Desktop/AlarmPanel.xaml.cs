using System.Windows;
using System.Windows.Controls;
using OnkyoRemote.Core;

namespace OnkyoRemote.Desktop;

public partial class AlarmPanel : UserControl
{
    private RemoteController? controller;
    private bool busy, loaded, storageReady, clockReady;
    public event Action? Back;

    public AlarmPanel()
    {
        InitializeComponent();
        Source.ItemsSource = AlarmSnapshot.Sources;
        SetTime(true, "07:00"); SetTime(false, "02:00"); Source.SelectedItem = "TUNER";
        IsVisibleChanged += (_, _) => { if (!IsVisible) DismissEditors(); };
        UpdateEnabled();
    }

    public async Task OpenAsync(RemoteController remote)
    {
        controller = remote;
        await LoadAsync();
    }

    public void UpdateEnabled()
    {
        var available = !busy && controller?.IsBusy != true;
        OnTime.IsEnabled = OffTime.IsEnabled = Source.IsEnabled = available && loaded && storageReady;
        OnSet.IsEnabled = OffSet.IsEnabled = available && loaded && storageReady && clockReady;
        OnCancel.IsEnabled = OffCancel.IsEnabled = available && loaded && storageReady;
        RefreshButton.IsEnabled = BackButton.IsEnabled = available;
    }

    public void DismissEditors()
    { OnTime.DismissEditor(); OffTime.DismissEditor(); Source.IsDropDownOpen = false; }

    private void SetTime(bool on, string time)
    {
        (on ? OnTime : OffTime).Time = time;
    }

    private void Display(AlarmSnapshot snapshot, string replace)
    {
        loaded = true; storageReady = snapshot.StorageReady; clockReady = snapshot.ClockReady;
        ClockText.Text = clockReady ? "Czas ESP32: " + (snapshot.LocalTime.Length >= 19 ? snapshot.LocalTime[11..] : snapshot.LocalTime) + " · Polska" : "Zegar ESP32 czeka na synchronizację.";
        if (replace is "all" or "on") { SetTime(true, snapshot.On.Time); Source.SelectedItem = snapshot.Source; }
        if (replace is "all" or "off") SetTime(false, snapshot.Off.Time);
        static string Status(AlarmSlotSnapshot slot) => slot.Enabled
            ? $"Ustawiono: {slot.Date[8..10]}.{slot.Date[5..7]} o {slot.Time}." : "Nie ustawiono.";
        OnStatus.Text = Status(snapshot.On); OffStatus.Text = Status(snapshot.Off);
        OnCancel.Visibility = snapshot.On.Enabled ? Visibility.Visible : Visibility.Collapsed;
        OffCancel.Visibility = snapshot.Off.Enabled ? Visibility.Visible : Visibility.Collapsed;
    }

    private Task LoadAsync() => RunAsync(null);

    private async Task RunAsync(string? action)
    {
        if (busy || controller == null || controller.IsBusy) return;
        if (action != null && (!loaded || !storageReady || (!action.StartsWith("cancel", StringComparison.Ordinal) && !clockReady))) return;
        busy = true; UpdateEnabled(); ResultText.Text = action == null ? "Odczytywanie…" : "Zapisywanie…";
        try
        {
            var on = action is "on" or "cancelOn";
            var time = (on ? OnTime : OffTime).Time;
            var snapshot = action == null ? await controller.AlarmsAsync()
                : await controller.ChangeAlarmAsync(action, time, Source.SelectedItem as string);
            Display(snapshot, action == null ? "all" : on ? "on" : "off");
            ResultText.Text = !storageReady ? "Pamięć ESP32 niedostępna. Budzik zatrzymany."
                : action == null ? "" : action.StartsWith("cancel", StringComparison.Ordinal) ? "Anulowano." : "Ustawione. Możesz wrócić do pilota.";
        }
        catch (Exception e)
        {
            loaded = false;
            ResultText.Text = (action == null ? "Nie można odczytać budzika. " : "Brak potwierdzenia zapisu. ") + e.Message + " Odśwież ustawienia.";
        }
        finally { busy = false; UpdateEnabled(); }
    }

    private async void Refresh_Click(object sender, RoutedEventArgs e) => await LoadAsync();
    private async void Set_Click(object sender, RoutedEventArgs e)
    { if (sender is Button { Tag: string action }) await RunAsync(action); }
    private void Back_Click(object sender, RoutedEventArgs e) => Back?.Invoke();
}
