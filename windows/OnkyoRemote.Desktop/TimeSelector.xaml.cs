using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;

namespace OnkyoRemote.Desktop;

public partial class TimeSelector : UserControl
{
    private string time = "07:00";
    public string Title { get; set; } = "Wybierz godzinę";
    public string Time
    {
        get => time;
        set
        {
            if (value.Length != 5 || value[2] != ':' || !Parse(value[..2], 24, out _) || !Parse(value[3..], 60, out _))
                throw new ArgumentException("Expected 24-hour HH:mm time.", nameof(value));
            time = value;
            ValueText.Text = value;
            TimeButton.SetValue(System.Windows.Automation.AutomationProperties.NameProperty, Title + ": " + value);
        }
    }

    public TimeSelector()
    {
        InitializeComponent(); Time = time;
        IsEnabledChanged += (_, _) => { if (!IsEnabled) Editor.IsOpen = false; };
        IsVisibleChanged += (_, _) => { if (!IsVisible) Editor.IsOpen = false; };
        Unloaded += (_, _) => Editor.IsOpen = false;
    }

    private static bool Parse(string value, int count, out int number)
    {
        number = 0;
        return value.Length is 1 or 2 && value.All(c => c is >= '0' and <= '9') && int.TryParse(value, out number) && number < count;
    }
    private void Open_Click(object sender, RoutedEventArgs e) => Editor.IsOpen = true;
    private void Editor_Opened(object? sender, EventArgs e)
    {
        HourInput.Text = time[..2]; MinuteInput.Text = time[3..]; ErrorText.Text = ""; EditorTitle.Text = Title;
        Dispatcher.BeginInvoke(() => { if (Editor.IsOpen) { HourInput.Focus(); HourInput.SelectAll(); } }, DispatcherPriority.Input);
    }
    private void CloseEditor() { Editor.IsOpen = false; TimeButton.Focus(); }
    public void DismissEditor() { if (Editor.IsOpen) CloseEditor(); }
    private void Cancel_Click(object sender, RoutedEventArgs e) => CloseEditor();
    private void Done_Click(object sender, RoutedEventArgs e)
    {
        if (!Parse(HourInput.Text, 24, out var hour) || !Parse(MinuteInput.Text, 60, out var minute))
        { ErrorText.Text = "Wpisz godzinę 00–23 i minuty 00–59."; return; }
        Time = $"{hour:00}:{minute:00}"; CloseEditor();
    }
    private void Step(string part, int change)
    {
        var input = part == "hour" ? HourInput : MinuteInput;
        var count = part == "hour" ? 24 : 60;
        if (!Parse(input.Text, count, out var number)) number = int.Parse(part == "hour" ? time[..2] : time[3..]);
        input.Text = ((number + change + count) % count).ToString("00"); ErrorText.Text = "";
    }
    private void Step_Click(object sender, RoutedEventArgs e)
    { if (sender is Button { Tag: string tag }) { var parts = tag.Split(':'); Step(parts[0], int.Parse(parts[1])); } }
    private void Input_Focus(object sender, KeyboardFocusChangedEventArgs e) => ((TextBox)sender).SelectAll();
    private void Input_Mouse(object sender, MouseButtonEventArgs e)
    { var input = (TextBox)sender; if (!input.IsKeyboardFocusWithin) { input.Focus(); e.Handled = true; } }
    private void Input_Text(object sender, TextCompositionEventArgs e) => e.Handled = !e.Text.All(c => c is >= '0' and <= '9');
    private void Input_Key(object sender, KeyEventArgs e)
    {
        if (e.Key is Key.Up or Key.Down) { Step((string)((TextBox)sender).Tag, e.Key == Key.Down ? 1 : -1); e.Handled = true; }
    }
    private void Input_Wheel(object sender, MouseWheelEventArgs e)
    { if (((TextBox)sender).IsKeyboardFocused) { Step((string)((TextBox)sender).Tag, e.Delta < 0 ? 1 : -1); e.Handled = true; } }
    private void Editor_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape) { CloseEditor(); e.Handled = true; }
        else if (e.Key == Key.Enter) { Done_Click(sender, e); e.Handled = true; }
    }
}
