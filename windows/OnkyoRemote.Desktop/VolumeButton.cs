using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace OnkyoRemote.Desktop;

/// <summary>Mouse capture delivers release outside the key. Keyboard auto-repeat is ignored.</summary>
public sealed class VolumeButton : Button
{
    public static readonly DependencyProperty IsHoldingProperty = DependencyProperty.Register(
        nameof(IsHolding), typeof(bool), typeof(VolumeButton), new PropertyMetadata(false));
    public bool IsHolding { get => (bool)GetValue(IsHoldingProperty); private set => SetValue(IsHoldingProperty, value); }
    public Func<bool>? BeginHold { get; set; }
    public Action? EndHold { get; set; }
    private bool keyboard;
    private Key heldKey;

    protected override void OnPreviewMouseLeftButtonDown(MouseButtonEventArgs e)
    {
        e.Handled = true;
        if (!IsEnabled || IsHolding) return;
        Focus();
        if (!CaptureMouse()) return;
        Start(false);
    }

    protected override void OnPreviewMouseLeftButtonUp(MouseButtonEventArgs e)
    {
        e.Handled = true;
        if (!keyboard) Release();
    }

    protected override void OnLostMouseCapture(MouseEventArgs e)
    {
        base.OnLostMouseCapture(e);
        if (!keyboard) Release();
    }

    protected override void OnPreviewKeyDown(KeyEventArgs e)
    {
        if (e.Key is not (Key.Space or Key.Return)) { base.OnPreviewKeyDown(e); return; }
        e.Handled = true;
        if (!IsEnabled || e.IsRepeat || IsHolding) return;
        heldKey = e.Key;
        Start(true);
    }

    protected override void OnPreviewKeyUp(KeyEventArgs e)
    {
        if (e.Key is not (Key.Space or Key.Return)) { base.OnPreviewKeyUp(e); return; }
        e.Handled = true;
        if (keyboard && heldKey == e.Key) Release();
    }

    protected override void OnLostKeyboardFocus(KeyboardFocusChangedEventArgs e)
    {
        base.OnLostKeyboardFocus(e);
        if (keyboard) Release();
    }

    // UI Automation Invoke is a single step, never an unattended hold.
    protected override void OnClick()
    {
        if (!IsHolding && BeginHold?.Invoke() == true) EndHold?.Invoke();
    }

    private void Start(bool byKeyboard)
    {
        keyboard = byKeyboard;
        IsHolding = true;
        if (BeginHold?.Invoke() != true)
        {
            IsHolding = false;
            keyboard = false;
            if (IsMouseCaptured) ReleaseMouseCapture();
        }
    }

    public void Release()
    {
        if (!IsHolding) { if (IsMouseCaptured) ReleaseMouseCapture(); return; }
        IsHolding = false;
        keyboard = false;
        EndHold?.Invoke();
        if (IsMouseCaptured) ReleaseMouseCapture();
    }
}
