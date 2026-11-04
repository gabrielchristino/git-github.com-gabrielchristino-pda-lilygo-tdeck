using System;
using System.IO;
using System.Drawing;
using System.Windows.Forms;

// Local test receiver: records only events delivered to this test window.
class BluetoothReceiver : Form {
    readonly TextBox editor = new TextBox();
    readonly string log;
    readonly Timer timer = new Timer();
    readonly DateTime started = DateTime.UtcNow;
    public BluetoothReceiver(string directory) {
        Directory.CreateDirectory(directory);
        log = Path.Combine(directory, "events.tsv");
        File.WriteAllText(log, "time\tevent\tvalue\n");
        Text = "T-Deck - teste Bluetooth";
        Size = new Size(900, 600);
        StartPosition = FormStartPosition.CenterScreen;
        KeyPreview = true;
        var help = new Label { Dock = DockStyle.Top, Height = 55,
            Text = "Teste local T-Deck Bluetooth. Digite e mova a bola aqui.\r\nOs eventos recebidos ficam registrados somente nesta janela.",
            Font = new Font("Segoe UI", 12) };
        editor.Multiline = true; editor.AcceptsReturn = true; editor.AcceptsTab = true;
        editor.Dock = DockStyle.Fill; editor.Font = new Font("Consolas", 18);
        editor.KeyDown += (s,e) => Write("DOWN", e.KeyCode + "/" + e.Modifiers);
        editor.KeyUp += (s,e) => Write("UP", e.KeyCode.ToString());
        editor.KeyPress += (s,e) => Write("CHAR", ((int)e.KeyChar).ToString());
        editor.MouseMove += (s,e) => Write("MOVE", e.X + "," + e.Y + "," + e.Button);
        editor.MouseDown += (s,e) => Write("MOUSE_DOWN", e.Button.ToString());
        editor.MouseUp += (s,e) => Write("MOUSE_UP", e.Button.ToString());
        editor.MouseWheel += (s,e) => Write("WHEEL", e.Delta.ToString());
        editor.TextChanged += (s,e) => File.WriteAllText(Path.Combine(directory,"text.txt"),editor.Text);
        Controls.Add(editor); Controls.Add(help);
        Shown += (s,e) => editor.Focus();
        timer.Interval = 250;
        timer.Tick += (s,e) => {
            File.WriteAllText(Path.Combine(directory,"state.txt"),
                "ACTIVE=" + (Form.ActiveForm == this) + " FOCUSED=" + editor.Focused +
                " POINTER_INSIDE=" + editor.ClientRectangle.Contains(editor.PointToClient(Cursor.Position)));
            if ((DateTime.UtcNow-started).TotalMinutes > 10) Close();
        };
        timer.Start();
    }
    void Write(string name, string value) {
        File.AppendAllText(log, DateTime.UtcNow.ToString("o") + "\t" + name + "\t" + value + "\n");
    }
    [STAThread] static void Main(string[] args) {
        Application.EnableVisualStyles();
        Application.Run(new BluetoothReceiver(args[0]));
    }
}
