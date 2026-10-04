using System.Windows;

namespace OnkyoRemote.Desktop;

public partial class App : Application
{
    private Mutex? instance;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        instance = new Mutex(true, "Local\\OnkyoRemote.Windows", out var first);
        if (!first)
        {
            MessageBox.Show("Pilot Onkyo jest już uruchomiony. Znajdziesz go na pasku zadań.", "Pilot Onkyo");
            Shutdown();
            return;
        }
        MainWindow = new MainWindow();
        MainWindow.Show();
    }

    protected override void OnExit(ExitEventArgs e)
    {
        instance?.Dispose();
        base.OnExit(e);
    }
}
