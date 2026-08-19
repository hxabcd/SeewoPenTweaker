using System.Runtime.InteropServices;
using System.Windows.Forms;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new PenTweakerForm());
    }
}

internal sealed class PenTweakerForm : Form
{
    private const int HotKeyP = 1;
    private const int HotKeyQ = 2;
    private const int WmHotKey = 0x0312;
    private const uint ModAlt = 0x0001;
    private const uint ModControl = 0x0002;
    private const uint ModShift = 0x0004;
    private const uint InputMouse = 0;
    private const uint MouseEventLeftDown = 0x0002;
    private const uint MouseEventLeftUp = 0x0004;
    private const uint MouseEventRightDown = 0x0008;
    private const uint MouseEventRightUp = 0x0010;

    private readonly NotifyIcon trayIcon;
    private readonly System.Windows.Forms.Timer pressTimer;
    private long lastPTime;
    private bool leftPressed;
    private bool shuttingDown;

    public PenTweakerForm()
    {
        ShowInTaskbar = false;
        WindowState = FormWindowState.Minimized;
        FormBorderStyle = FormBorderStyle.FixedToolWindow;

        pressTimer = new System.Windows.Forms.Timer { Interval = 300 };
        pressTimer.Tick += (_, _) => PressLeft();

        var menu = new ContextMenuStrip();
        menu.Items.Add(new ToolStripMenuItem("状态: 运行中") { Enabled = false });
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add(new ToolStripMenuItem("退出", null, (_, _) => Close()));

        trayIcon = new NotifyIcon
        {
            Icon = SystemIcons.Application,
            Text = "SeewoPenTweaker",
            ContextMenuStrip = menu,
            Visible = true
        };

        Load += (_, _) => Hide();
    }

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        var modifiers = ModControl | ModShift | ModAlt;
        if (!RegisterHotKey(Handle, HotKeyP, modifiers, (uint)Keys.P) ||
            !RegisterHotKey(Handle, HotKeyQ, modifiers, (uint)Keys.Q))
        {
            throw new InvalidOperationException("注册全局快捷键失败，可能已被其他程序占用。");
        }
    }

    protected override void WndProc(ref Message m)
    {
        if (m.Msg == WmHotKey)
        {
            if (m.WParam.ToInt32() == HotKeyP)
            {
                lastPTime = Environment.TickCount64;
                pressTimer.Stop();
                pressTimer.Start();
            }
            else if (m.WParam.ToInt32() == HotKeyQ)
            {
                pressTimer.Stop();
                var isPair = lastPTime > 0 && Environment.TickCount64 - lastPTime <= 200;
                ReleaseLeft();
                if (isPair)
                {
                    ClickRight();
                }
                lastPTime = 0;
            }
        }

        base.WndProc(ref m);
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        if (shuttingDown)
        {
            base.OnFormClosing(e);
            return;
        }

        shuttingDown = true;
        pressTimer.Stop();
        UnregisterHotKey(Handle, HotKeyP);
        UnregisterHotKey(Handle, HotKeyQ);
        ReleaseLeft();
        trayIcon.Visible = false;
        trayIcon.Dispose();
        pressTimer.Dispose();
        base.OnFormClosing(e);
    }

    private void PressLeft()
    {
        pressTimer.Stop();
        if (!shuttingDown && !leftPressed)
        {
            SendMouse(MouseEventLeftDown);
            leftPressed = true;
        }
    }

    private void ReleaseLeft()
    {
        if (leftPressed)
        {
            SendMouse(MouseEventLeftUp);
            leftPressed = false;
        }
    }

    private static void ClickRight()
    {
        SendMouse(MouseEventRightDown | MouseEventRightUp);
    }

    private static void SendMouse(uint flags)
    {
        mouse_event(flags, 0, 0, 0, UIntPtr.Zero);
    }

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool UnregisterHotKey(IntPtr hWnd, int id);

    [DllImport("user32.dll")]
    private static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
}
