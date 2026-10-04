using System.ComponentModel;
using System.Runtime.InteropServices;
using OnkyoRemote.Core;

namespace OnkyoRemote.Desktop;

/// <summary>Dedicated message thread. Callback only filters and queues; never performs HTTP.</summary>
public sealed class MediaKeyHook : IDisposable
{
    private readonly Action<MediaKeySignal> enqueue;
    private readonly Thread thread;
    private readonly HookProc callback;
    private readonly ManualResetEventSlim ready = new();
    private MediaKeyFilter? filter;
    private nint hook;
    private uint threadId;
    private Exception? startupError;
    private volatile bool stopping;
    public bool IsRunning => hook != 0 && !stopping;

    public MediaKeyHook(Action<MediaKeySignal> enqueue)
    {
        this.enqueue = enqueue;
        callback = Callback;
        thread = new Thread(Run) { IsBackground = true, Name = "onkyo-media-keys" };
        thread.Start();
        if (!ready.Wait(TimeSpan.FromSeconds(3))) { Dispose(); throw new TimeoutException("Media key hook startup timed out"); }
        if (startupError != null) { Dispose(); throw new InvalidOperationException("Media key hook unavailable", startupError); }
    }

    private void Run()
    {
        try
        {
            threadId = GetCurrentThreadId();
            PeekMessage(out _, 0, 0, 0, 0); // Create the queue before shutdown can post WM_QUIT.
            var keys = new[] { MediaKeyFilter.Mute, MediaKeyFilter.Down, MediaKeyFilter.Up, MediaKeyFilter.PlayPause };
            filter = new MediaKeyFilter(keys.Where(key => (GetAsyncKeyState(key) & 0x8000) != 0));
            hook = SetWindowsHookEx(13, callback, GetModuleHandle(null), 0);
            if (hook == 0) throw new Win32Exception(Marshal.GetLastWin32Error());
            ready.Set();
            while (!stopping && GetMessage(out var message, 0, 0, 0) > 0)
            { TranslateMessage(ref message); DispatchMessage(ref message); }
        }
        catch (Exception e) { startupError = e; ready.Set(); }
        finally
        {
            if (hook != 0) { UnhookWindowsHookEx(hook); hook = 0; }
            filter?.Disable();
            GC.KeepAlive(callback);
        }
    }

    private nint Callback(int code, nint message, nint data)
    {
        if (code >= 0 && !stopping && message is 0x100 or 0x101 or 0x104 or 0x105)
        {
            var key = Marshal.ReadInt32(data);
            if (MediaKeyFilter.IsMapped(key))
            {
                try
                {
                    var decision = filter!.Handle(key, message is 0x100 or 0x104);
                    if (decision.Signal is MediaKeySignal signal) enqueue(signal);
                    if (decision.Intercept) return 1;
                }
                catch (Exception) { /* Shutdown/queue failure: let Windows handle the key. */ }
            }
        }
        return CallNextHookEx(hook, code, message, data);
    }

    public void Dispose()
    {
        if (stopping) return;
        stopping = true;
        if (threadId != 0) PostThreadMessage(threadId, 0x12, 0, 0);
        if (Thread.CurrentThread != thread) thread.Join(TimeSpan.FromSeconds(3));
    }

    private delegate nint HookProc(int code, nint message, nint data);
    [StructLayout(LayoutKind.Sequential)]
    private struct Message
    {
        public nint Window; public uint Id; public nuint WParam; public nint LParam;
        public uint Time; public int X, Y; public uint Private;
    }
    [DllImport("user32.dll", EntryPoint = "SetWindowsHookExW", SetLastError = true)]
    private static extern nint SetWindowsHookEx(int id, HookProc callback, nint module, uint thread);
    [DllImport("user32.dll")]
    private static extern bool UnhookWindowsHookEx(nint hook);
    [DllImport("user32.dll")]
    private static extern nint CallNextHookEx(nint hook, int code, nint message, nint data);
    [DllImport("user32.dll", EntryPoint = "GetMessageW")]
    private static extern int GetMessage(out Message message, nint window, uint minimum, uint maximum);
    [DllImport("user32.dll", EntryPoint = "PeekMessageW")]
    private static extern bool PeekMessage(out Message message, nint window, uint minimum, uint maximum, uint remove);
    [DllImport("user32.dll")]
    private static extern bool TranslateMessage(ref Message message);
    [DllImport("user32.dll", EntryPoint = "DispatchMessageW")]
    private static extern nint DispatchMessage(ref Message message);
    [DllImport("user32.dll", EntryPoint = "PostThreadMessageW")]
    private static extern bool PostThreadMessage(uint thread, uint message, nuint wParam, nint lParam);
    [DllImport("user32.dll")]
    private static extern short GetAsyncKeyState(int key);
    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();
    [DllImport("kernel32.dll", EntryPoint = "GetModuleHandleW", CharSet = CharSet.Unicode)]
    private static extern nint GetModuleHandle(string? name);
}
