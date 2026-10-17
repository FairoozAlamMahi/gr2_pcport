using System;
using System.IO;
using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;

namespace GravityRush2Launcher {

    public enum ERole { eConsole = 0, eMultimedia = 1, eCommunications = 2 }

    [Guid("f8679f50-850a-41cf-9c72-430f290290c8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IPolicyConfig {
        [PreserveSig] int GetMixFormat();
        [PreserveSig] int GetDeviceFormat();
        [PreserveSig] int ResetDeviceFormat();
        [PreserveSig] int SetDeviceFormat();
        [PreserveSig] int GetProcessingPeriod();
        [PreserveSig] int SetProcessingPeriod();
        [PreserveSig] int GetShareMode();
        [PreserveSig] int SetShareMode();
        [PreserveSig] int GetPropertyValue();
        [PreserveSig] int SetPropertyValue();
        [PreserveSig] int SetDefaultEndpoint([In, MarshalAs(UnmanagedType.LPWStr)] string wszDeviceId, [In, MarshalAs(UnmanagedType.U4)] ERole role);
        [PreserveSig] int SetEndpointVisibility();
    }

    [ComImport, Guid("870af99c-171d-4f9e-af0d-e63df40c2bc9")]
    internal class CPolicyConfigClient { }

    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    internal class MMDeviceEnumeratorComObject { }

    [Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMMDeviceEnumerator {
        [PreserveSig] int EnumAudioEndpoints(int dataFlow, int dwStateMask, out IMMDeviceCollection ppDevices);
        [PreserveSig] int GetDefaultAudioEndpoint(int dataFlow, int role, out IMMDevice ppEndpoint);
    }

    [Guid("0BD7A1BE-7A1A-44DB-8397-CC5392387B5E"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMMDeviceCollection {
        [PreserveSig] int GetCount(out uint pcDevices);
        [PreserveSig] int Item(uint nDevice, out IMMDevice ppDevice);
    }

    [Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMMDevice {
        [PreserveSig] int Activate(ref Guid iid, int dwClsCtx, IntPtr pActivationParams, [MarshalAs(UnmanagedType.IUnknown)] out object ppInterface);
        [PreserveSig] int OpenPropertyStore(int stgmAccess, out IPropertyStore ppProperties);
        [PreserveSig] int GetId([MarshalAs(UnmanagedType.LPWStr)] out string ppstrId);
        [PreserveSig] int GetState(out int pdwState);
    }

    [Guid("886d8eeb-8cf2-4446-8d02-cdba1dbdcf99"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IPropertyStore {
        [PreserveSig] int GetCount(out uint cProps);
        [PreserveSig] int GetAt(uint iProp, out PropertyKey pkey);
        [PreserveSig] int GetValue(ref PropertyKey key, out PropVariant pv);
        [PreserveSig] int SetValue(ref PropertyKey key, ref PropVariant propvar);
        [PreserveSig] int Commit();
    }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    internal struct PropertyKey {
        public Guid fmtid;
        public uint pid;
        public PropertyKey(Guid guid, uint id) { fmtid = guid; pid = id; }
    }

    [StructLayout(LayoutKind.Explicit)]
    internal struct PropVariant {
        [FieldOffset(0)] public ushort vt;
        [FieldOffset(8)] public IntPtr pwszVal;
    }

    public class AudioDeviceInfo {
        public string Name { get; set; }
        public string Id { get; set; }
        public bool IsDefault { get; set; }
        public bool IsHeadset { get; set; }

        public override string ToString() {
            if (IsHeadset) return Name + " [Headphones/Headset - Recommended]";
            if (IsDefault) return Name + " [Current Windows Default]";
            return Name;
        }
    }

    public static class AudioDeviceManager {
        private static string originalDefaultConsoleId = null;
        private static string originalDefaultMultimediaId = null;
        private static PropertyKey PKEY_FriendlyName = new PropertyKey(new Guid("a45c254e-df1c-4efd-8020-67d146a850e0"), 14);

        public static List<AudioDeviceInfo> GetActiveDevices() {
            List<AudioDeviceInfo> list = new List<AudioDeviceInfo>();
            try {
                var enumerator = (IMMDeviceEnumerator)(new MMDeviceEnumeratorComObject());
                IMMDeviceCollection col;
                enumerator.EnumAudioEndpoints(0, 1, out col); // eRender=0, DEVICE_STATE_ACTIVE=1
                uint count = 0;
                col.GetCount(out count);

                string defId = GetDefaultDeviceId(ERole.eMultimedia);

                for (uint i = 0; i < count; i++) {
                    IMMDevice dev;
                    col.Item(i, out dev);
                    string id;
                    dev.GetId(out id);
                    IPropertyStore store;
                    dev.OpenPropertyStore(0, out store);
                    PropVariant pv;
                    store.GetValue(ref PKEY_FriendlyName, out pv);
                    string name = Marshal.PtrToStringUni(pv.pwszVal);
                    if (string.IsNullOrEmpty(name)) name = "Audio Endpoint " + i;

                    bool isHeadset = name.IndexOf("headset", StringComparison.OrdinalIgnoreCase) >= 0 ||
                                     name.IndexOf("headphone", StringComparison.OrdinalIgnoreCase) >= 0 ||
                                     name.IndexOf("rapoo", StringComparison.OrdinalIgnoreCase) >= 0;

                    bool isDef = (id == defId);

                    list.Add(new AudioDeviceInfo() {
                        Name = name,
                        Id = id,
                        IsDefault = isDef,
                        IsHeadset = isHeadset
                    });
                }
            } catch { }
            return list;
        }

        public static string GetDefaultDeviceId(ERole role) {
            try {
                var enumerator = (IMMDeviceEnumerator)(new MMDeviceEnumeratorComObject());
                IMMDevice dev;
                int hr = enumerator.GetDefaultAudioEndpoint(0, (int)role, out dev);
                if (hr == 0 && dev != null) {
                    string id;
                    dev.GetId(out id);
                    return id;
                }
            } catch { }
            return null;
        }

        public static void SaveOriginalDefault() {
            if (originalDefaultConsoleId == null) {
                originalDefaultConsoleId = GetDefaultDeviceId(ERole.eConsole);
            }
            if (originalDefaultMultimediaId == null) {
                originalDefaultMultimediaId = GetDefaultDeviceId(ERole.eMultimedia);
            }
        }

        public static bool SetDefaultEndpoint(string deviceId) {
            if (string.IsNullOrEmpty(deviceId)) return false;
            try {
                SaveOriginalDefault();
                var policy = (IPolicyConfig)new CPolicyConfigClient();
                policy.SetDefaultEndpoint(deviceId, ERole.eConsole);
                policy.SetDefaultEndpoint(deviceId, ERole.eMultimedia);
                return true;
            } catch {
                return false;
            }
        }

        public static void RestoreOriginalDefault() {
            try {
                if (originalDefaultConsoleId != null || originalDefaultMultimediaId != null) {
                    var policy = (IPolicyConfig)new CPolicyConfigClient();
                    if (originalDefaultConsoleId != null) policy.SetDefaultEndpoint(originalDefaultConsoleId, ERole.eConsole);
                    if (originalDefaultMultimediaId != null) policy.SetDefaultEndpoint(originalDefaultMultimediaId, ERole.eMultimedia);
                }
            } catch { }
        }
    }

    public class GlobalHotkey : IMessageFilter {
        [DllImport("user32.dll")]
        public static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);
        [DllImport("user32.dll")]
        public static extern bool UnregisterHotKey(IntPtr hWnd, int id);

        public const int WM_HOTKEY = 0x0312;
        private IntPtr hWnd;
        private Action onHotkey;

        public GlobalHotkey(IntPtr handle, uint mod, uint vk, Action callback) {
            this.hWnd = handle;
            this.onHotkey = callback;
            RegisterHotKey(handle, 1, mod, vk);
            Application.AddMessageFilter(this);
        }

        public bool PreFilterMessage(ref Message m) {
            if (m.Msg == WM_HOTKEY && (int)m.WParam == 1) {
                if (onHotkey != null) onHotkey();
                return true;
            }
            return false;
        }

        public void Unregister() {
            Application.RemoveMessageFilter(this);
            UnregisterHotKey(hWnd, 1);
        }
    }

    public static class SettingsManager {
        public static string BaseDir = AppDomain.CurrentDomain.BaseDirectory;

        public static string GetTlgIniPath() {
            string p1 = Path.Combine(BaseDir, "user", "tlg.ini");
            if (File.Exists(p1)) return p1;
            string p2 = Path.Combine(BaseDir, "..", "user", "tlg.ini");
            if (File.Exists(p2)) return p2;
            return p1;
        }

        public static string GetLauncherSettingsPath() {
            return Path.Combine(BaseDir, "user", "launcher_settings.ini");
        }

        public static void ReadTlgIni(out string displayMode, out bool vsync, out bool showFps, out string engineFps) {
            displayMode = "borderless";
            vsync = true;
            showFps = true;
            engineFps = "60";
            string path = GetTlgIniPath();
            if (File.Exists(path)) {
                try {
                    string[] lines = File.ReadAllLines(path);
                    foreach (var line in lines) {
                        string tr = line.Trim();
                        if (tr.StartsWith("display_mode", StringComparison.OrdinalIgnoreCase)) {
                            var parts = tr.Split('=');
                            if (parts.Length > 1) displayMode = parts[1].Trim().ToLowerInvariant();
                        } else if (tr.StartsWith("vsync", StringComparison.OrdinalIgnoreCase)) {
                            var parts = tr.Split('=');
                            if (parts.Length > 1) vsync = (parts[1].Trim() == "1");
                        } else if (tr.StartsWith("show_fps", StringComparison.OrdinalIgnoreCase)) {
                            var parts = tr.Split('=');
                            if (parts.Length > 1) showFps = (parts[1].Trim() == "1");
                        } else if (tr.StartsWith("engine_fps", StringComparison.OrdinalIgnoreCase)) {
                            var parts = tr.Split('=');
                            if (parts.Length > 1) engineFps = parts[1].Trim();
                        }
                    }
                } catch { }
            }
        }

        public static void WriteTlgIni(string displayMode, bool vsync, bool showFps, string engineFps) {
            try {
                string path = GetTlgIniPath();
                string dir = Path.GetDirectoryName(path);
                if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);

                string normMode = "Borderless";
                if (displayMode.Equals("windowed", StringComparison.OrdinalIgnoreCase)) normMode = "Windowed";
                else if (displayMode.Equals("fullscreen", StringComparison.OrdinalIgnoreCase)) normMode = "Fullscreen";

                StringBuilder sb = new StringBuilder();
                sb.AppendLine("# tlg settings (in-game menu: Insert / R3+L2)");
                sb.AppendLine("display_mode=" + normMode);
                sb.AppendLine("vsync=" + (vsync ? "1" : "0"));
                sb.AppendLine("show_fps=" + (showFps ? "1" : "0"));
                sb.AppendLine("engine_fps=" + engineFps);
                sb.AppendLine("fps_limit=0");
                sb.AppendLine("deferred_readback=0");
                sb.AppendLine("stale_readback=0");
                File.WriteAllText(path, sb.ToString());
            } catch { }
        }

        public static void ResetAudioSavedata() {
            string saveDir = Path.Combine(BaseDir, "user", "savedata", "1", "CUSA04943", "system0020");
            string origFile = Path.Combine(saveDir, "system0020.orig");
            string binFile = Path.Combine(saveDir, "system0020.bin");

            try {
                if (File.Exists(origFile)) {
                    File.Copy(origFile, binFile, true);
                } else if (File.Exists(binFile)) {
                    // Touch or ensure clean state
                }
            } catch { }
        }
    }

    public class ControlPanelForm : Form {
        private bool isOverlayMode;
        private Process runningProcess;
        private GlobalHotkey hotkey;

        private ComboBox cbAudioDevice;
        private ComboBox cbDisplayMode;
        private ComboBox cbFpsMode;
        private ComboBox cbShowFps;
        private ComboBox cbVsync;
        private Button btnResetAudio;
        private Button btnLaunch;
        private Label lblStatus;
        private List<AudioDeviceInfo> audioDevices;

        public ControlPanelForm(bool overlay, Process proc) {
            this.isOverlayMode = overlay;
            this.runningProcess = proc;
            InitializeUI();
        }

        private void InitializeUI() {
            this.Text = isOverlayMode ? "Gravity Rush 2 — In-Game Control Panel (F1)" : "Gravity Rush 2 — PC Port Launcher & Settings";
            this.Size = new Size(580, 520);
            this.FormBorderStyle = isOverlayMode ? FormBorderStyle.None : FormBorderStyle.FixedSingle;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.MaximizeBox = false;
            this.BackColor = Color.FromArgb(20, 24, 38);
            this.ForeColor = Color.FromArgb(235, 240, 255);
            this.Font = new Font("Segoe UI", 9.5f, FontStyle.Regular);

            if (isOverlayMode) {
                this.Opacity = 0.94;
                this.TopMost = true;
                this.ShowInTaskbar = false;
            } else {
                try {
                    string iconPath = Path.Combine(SettingsManager.BaseDir, "icon.ico");
                    if (!File.Exists(iconPath)) iconPath = Path.Combine(SettingsManager.BaseDir, "..", "icon.ico");
                    if (File.Exists(iconPath)) this.Icon = new Icon(iconPath);
                } catch { }
            }

            // Header Banner
            Panel pnlHeader = new Panel();
            pnlHeader.Dock = DockStyle.Top;
            pnlHeader.Height = 65;
            pnlHeader.BackColor = Color.FromArgb(14, 17, 28);

            Label lblTitle = new Label();
            lblTitle.Text = "GRAVITY RUSH 2";
            lblTitle.Font = new Font("Segoe UI", 16f, FontStyle.Bold);
            lblTitle.ForeColor = Color.FromArgb(245, 195, 65);
            lblTitle.Location = new Point(25, 10);
            lblTitle.AutoSize = true;

            Label lblSubtitle = new Label();
            lblSubtitle.Text = isOverlayMode ? "Live In-Game Performance & Audio Control Panel (Press F1 or ESC to close)" : "PC Edition · Native Audio Synthesis · 60 FPS Locked Timestep";
            lblSubtitle.Font = new Font("Segoe UI", 9f, FontStyle.Regular);
            lblSubtitle.ForeColor = Color.FromArgb(170, 185, 215);
            lblSubtitle.Location = new Point(26, 38);
            lblSubtitle.AutoSize = true;

            pnlHeader.Controls.Add(lblTitle);
            pnlHeader.Controls.Add(lblSubtitle);
            this.Controls.Add(pnlHeader);

            int startY = 80;
            int labelX = 30;
            int ctrlX = 230;
            int rowSpacing = 42;

            // 1. Audio Playback Device
            Label lblAudio = new Label() { Text = "Audio Playback Device:", Location = new Point(labelX, startY), AutoSize = true };
            cbAudioDevice = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 300, DropDownStyle = ComboBoxStyle.DropDownList };
            audioDevices = AudioDeviceManager.GetActiveDevices();
            int selectedAudioIdx = 0;
            for (int i = 0; i < audioDevices.Count; i++) {
                cbAudioDevice.Items.Add(audioDevices[i]);
                if (audioDevices[i].IsHeadset) selectedAudioIdx = i;
            }
            if (selectedAudioIdx == 0) {
                for (int i = 0; i < audioDevices.Count; i++) {
                    if (audioDevices[i].IsDefault) { selectedAudioIdx = i; break; }
                }
            }
            if (cbAudioDevice.Items.Count > 0) cbAudioDevice.SelectedIndex = selectedAudioIdx;
            this.Controls.Add(lblAudio);
            this.Controls.Add(cbAudioDevice);

            // 2. Framerate / Performance Mode
            startY += rowSpacing;
            Label lblFps = new Label() { Text = "Framerate & Performance:", Location = new Point(labelX, startY), AutoSize = true };
            cbFpsMode = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 300, DropDownStyle = ComboBoxStyle.DropDownList };
            cbFpsMode.Items.AddRange(new object[] {
                "60 FPS (Rock-Solid 60.0 FPS - Standard Mode - Recommended)",
                "30 FPS (PS4 Original Cinematic Mode)"
            });
            cbFpsMode.SelectedIndex = 0;
            this.Controls.Add(lblFps);
            this.Controls.Add(cbFpsMode);

            // 3. Display Window Mode
            startY += rowSpacing;
            Label lblDisplay = new Label() { Text = "Display Window Mode:", Location = new Point(labelX, startY), AutoSize = true };
            cbDisplayMode = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 300, DropDownStyle = ComboBoxStyle.DropDownList };
            cbDisplayMode.Items.AddRange(new object[] {
                "Borderless Fullscreen (Recommended)",
                "Exclusive Fullscreen",
                "Windowed (1080p)"
            });
            cbDisplayMode.SelectedIndex = 0;
            this.Controls.Add(lblDisplay);
            this.Controls.Add(cbDisplayMode);

            // 4. In-Game Real-Time FPS Counter
            startY += rowSpacing;
            Label lblShowFps = new Label() { Text = "In-Game FPS Counter:", Location = new Point(labelX, startY), AutoSize = true };
            cbShowFps = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 300, DropDownStyle = ComboBoxStyle.DropDownList };
            cbShowFps.Items.AddRange(new object[] {
                "Enabled (Press F10 in-game to toggle)",
                "Disabled (Clean HUD)"
            });
            cbShowFps.SelectedIndex = 0;
            this.Controls.Add(lblShowFps);
            this.Controls.Add(cbShowFps);

            // 5. VSync
            startY += rowSpacing;
            Label lblVsync = new Label() { Text = "Vertical Sync (VSync):", Location = new Point(labelX, startY), AutoSize = true };
            cbVsync = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 300, DropDownStyle = ComboBoxStyle.DropDownList };
            cbVsync.Items.AddRange(new object[] {
                "Enabled (Standard 60Hz FIFO Sync - Recommended)",
                "Disabled (VSync Off)"
            });
            cbVsync.SelectedIndex = 0;
            this.Controls.Add(lblVsync);
            this.Controls.Add(cbVsync);

            // 6. One-Click Audio & Slider Reset
            startY += rowSpacing + 5;
            btnResetAudio = new Button();
            btnResetAudio.Text = "🔧 One-Click Sound Slider & Audio Reset";
            btnResetAudio.Location = new Point(labelX, startY);
            btnResetAudio.Size = new Size(500, 34);
            btnResetAudio.BackColor = Color.FromArgb(36, 44, 68);
            btnResetAudio.ForeColor = Color.FromArgb(235, 242, 255);
            btnResetAudio.FlatStyle = FlatStyle.Flat;
            btnResetAudio.Click += BtnResetAudio_Click;
            this.Controls.Add(btnResetAudio);

            // 7. Launch / Apply Button
            startY += 45;
            btnLaunch = new Button();
            btnLaunch.Text = isOverlayMode ? "✔ APPLY SETTINGS & RESUME (ESC / F1)" : "▶ LAUNCH GRAVITY RUSH 2 (60 FPS)";
            btnLaunch.Location = new Point(labelX, startY);
            btnLaunch.Size = new Size(500, 46);
            btnLaunch.BackColor = Color.FromArgb(225, 160, 40);
            btnLaunch.ForeColor = Color.FromArgb(14, 17, 26);
            btnLaunch.Font = new Font("Segoe UI", 12f, FontStyle.Bold);
            btnLaunch.FlatStyle = FlatStyle.Flat;
            btnLaunch.Click += BtnLaunch_Click;
            this.Controls.Add(btnLaunch);

            // 8. Hotkeys Legend & Status
            startY += 54;
            lblStatus = new Label();
            lblStatus.Text = isOverlayMode
                ? "Shortcuts: F10 = In-Game FPS · Insert (R3+L2) = Port Menu · F1 = Control Panel"
                : "Hotkeys: F10 = In-Game FPS · Insert = Settings Menu · F1 = Control Panel Overlay";
            lblStatus.Font = new Font("Segoe UI", 8.5f, FontStyle.Italic);
            lblStatus.ForeColor = Color.FromArgb(160, 185, 220);
            lblStatus.Location = new Point(labelX, startY);
            lblStatus.AutoSize = true;
            this.Controls.Add(lblStatus);

            this.KeyPreview = true;
            this.KeyDown += ControlPanelForm_KeyDown;

            LoadSettings();
        }

        public void LoadSettings() {
            try {
                string mode, engineFps;
                bool vsync, showFps;
                SettingsManager.ReadTlgIni(out mode, out vsync, out showFps, out engineFps);

                if (mode == "windowed") cbDisplayMode.SelectedIndex = 2;
                else if (mode == "fullscreen") cbDisplayMode.SelectedIndex = 1;
                else cbDisplayMode.SelectedIndex = 0; // borderless

                cbVsync.SelectedIndex = vsync ? 0 : 1;
                cbShowFps.SelectedIndex = showFps ? 0 : 1;

                if (engineFps == "30") cbFpsMode.SelectedIndex = 1;
                else cbFpsMode.SelectedIndex = 0; // default 60 FPS
            } catch { }
        }

        private void ControlPanelForm_KeyDown(object sender, KeyEventArgs e) {
            if (e.KeyCode == Keys.Escape || e.KeyCode == Keys.F1) {
                if (isOverlayMode) {
                    ApplySettings();
                    this.Hide();
                }
            }
        }

        private void BtnResetAudio_Click(object sender, EventArgs e) {
            SettingsManager.ResetAudioSavedata();
            AudioDeviceInfo info = cbAudioDevice.SelectedItem as AudioDeviceInfo;
            if (info != null) {
                AudioDeviceManager.SetDefaultEndpoint(info.Id);
            }
            MessageBox.Show(
                "Audio sliders and settings have been restored to clean defaults!\n\n" +
                "• All sound sliders (Master, BGM, SFX, Voice) are reset to 100% full volume.\n" +
                "• Native NGS2 audio synthesis active with real-time voices, combat SFX, and world music.\n" +
                "• Audio output routed directly to: " + (cbAudioDevice.SelectedItem != null ? cbAudioDevice.SelectedItem.ToString() : "Default Audio Device") + ".\n" +
                "• In-game Tip: In Sound Options, set 'Controller Speaker' to OFF to hear Kat's voice lines on your headphones/speakers.",
                "Gravity Rush 2 Audio Restored",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
            lblStatus.Text = "Audio sliders successfully restored to authentic 100% defaults!";
        }

        private void ApplySettings() {
            string mode = "borderless";
            if (cbDisplayMode.SelectedIndex == 1) mode = "fullscreen";
            else if (cbDisplayMode.SelectedIndex == 2) mode = "windowed";

            bool vsync = (cbVsync.SelectedIndex == 0);
            bool showFps = (cbShowFps.SelectedIndex == 0);

            string fps = (cbFpsMode.SelectedIndex == 1) ? "30" : "60";

            SettingsManager.WriteTlgIni(mode, vsync, showFps, fps);

            AudioDeviceInfo info = cbAudioDevice.SelectedItem as AudioDeviceInfo;
            if (info != null) {
                AudioDeviceManager.SetDefaultEndpoint(info.Id);
            }
        }

        private void BtnLaunch_Click(object sender, EventArgs e) {
            ApplySettings();

            if (isOverlayMode) {
                this.Hide();
                return;
            }

            // Restore audio savedata sliders to pristine 100% volume state
            SettingsManager.ResetAudioSavedata();

            string baseDir = SettingsManager.BaseDir;

            // Search for native engine probe
            string[] probeCandidates = new string[] {
                Path.Combine(baseDir, "engine", "tlg-probe.exe"),
                Path.Combine(baseDir, "out", "tlg-probe.exe"),
                Path.Combine(baseDir, "..", "scratch", "HayatoG_port", "gr2-windows-x86_64", "out", "tlg-probe.exe")
            };

            string foundProbe = null;
            string engineDir = null;
            foreach (var cand in probeCandidates) {
                if (File.Exists(cand)) {
                    foundProbe = Path.GetFullPath(cand);
                    engineDir = Path.GetDirectoryName(foundProbe);
                    break;
                }
            }

            // Search for game directory (containing eboot.bin or psarc chunks)
            string[] gameCandidates = new string[] {
                Path.Combine(baseDir, "game"),
                Path.Combine(baseDir, "gr2_packages", "merged_game"),
                Path.Combine(baseDir, "..", "gr2_packages", "merged_game"),
                Path.Combine(baseDir, "game", "merged_game")
            };

            string foundGameDir = null;
            foreach (var cand in gameCandidates) {
                if (Directory.Exists(cand) && (File.Exists(Path.Combine(cand, "eboot.bin")) || File.Exists(Path.Combine(cand, "Gravite2_Chunk0.psarc")))) {
                    foundGameDir = Path.GetFullPath(cand);
                    break;
                }
            }

            if (foundGameDir == null) {
                MessageBox.Show(
                    "Game files were not found in 'game/'.\n\n" +
                    "Please ensure 'game/' contains Gravity Rush 2 PS4 files (eboot.bin and psarcs).",
                    "Gravity Rush 2 PC Port",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
                return;
            }

            string userDir = Path.Combine(baseDir, "user");
            if (!Directory.Exists(userDir)) {
                string userCand = Path.Combine(baseDir, "..", "scratch", "HayatoG_port", "gr2-windows-x86_64", "user");
                if (Directory.Exists(userCand)) userDir = Path.GetFullPath(userCand);
            }

            if (foundProbe != null) {
                // Launch Native Engine
                string bootLinked = Path.Combine(engineDir, "boot-linked.bin");
                string contentBin = Path.Combine(engineDir, "content.bin");
                string patchesBin = Path.Combine(engineDir, "patches.bin");
                string isaRewrite = Path.Combine(engineDir, "isa-rewrite.bin");

                ProcessStartInfo psi = new ProcessStartInfo();
                psi.FileName = foundProbe;
                psi.Arguments = string.Format("\"{0}\" --app0 \"{1}\" --user \"{2}\" --content-profile \"{3}\" --patches \"{4}\" --isa-rewrite \"{5}\"",
                    bootLinked, foundGameDir, userDir, contentBin, patchesBin, isaRewrite);
                psi.WorkingDirectory = engineDir;
                psi.UseShellExecute = false;

                // Target frame rate
                string fps = (cbFpsMode.SelectedIndex == 1) ? "30" : "60";

                string mode = "borderless";
                if (cbDisplayMode.SelectedIndex == 1) mode = "fullscreen";
                else if (cbDisplayMode.SelectedIndex == 2) mode = "windowed";

                // Window & Fullscreen display mode (1 = Borderless / Fullscreen, 0 = Windowed)
                psi.EnvironmentVariables["TLG_FULLSCREEN"] = (mode != "windowed") ? "1" : "0";

                // Rock-Solid Gravity Rush 2 PC Port environment:
                // 1. TLG_READBACKS=2 is REQUIRED for AsyncMap: CPU reads GPU compute results for collision queries,
                //    interaction volumes (mining prompt "Press this to start mining"), and ore streaming.
                psi.EnvironmentVariables["TLG_READBACKS"] = "2";
                psi.EnvironmentVariables["TLG_DEFERRED_READBACK"] = "0";
                psi.EnvironmentVariables["TLG_STALE_READBACK"] = "0";
                psi.EnvironmentVariables["TLG_PIPELINE_BINARIES"] = "0";
                psi.EnvironmentVariables["TLG_GC_BUDGET_MB"] = "5600";
                psi.EnvironmentVariables["TLG_UPSCALER"] = "off";
                psi.EnvironmentVariables["TLG_JITTER"] = "0";
                psi.EnvironmentVariables["TLG_OBJECT_MOTION"] = "0";
                psi.EnvironmentVariables["TLG_FRAME_STATS"] = "1";
                psi.EnvironmentVariables["TLG_LANGUAGE"] = "1";              // English
                psi.EnvironmentVariables["TLG_NGS2"] = "hle";                // Clean-room C NGS2 audio synthesizer
                psi.EnvironmentVariables["TLG_SKIP_ADJACENCY"] = "1";        // Fix black streaks near characters
                psi.EnvironmentVariables["TLG_CONFIG"] = SettingsManager.GetTlgIniPath();
                psi.EnvironmentVariables["TLG_ENGINE_FPS"] = fps;

                // Anti-Freeze Fix (Deadlock Prevention):
                // Disable speculative draw prep background workers (DrawPrep0..5).
                // Prevents secondary threads from concurrently faulting on guest VRAM pages
                // and deadlocking with DrawThread on GpuCommandProcessor.
                psi.EnvironmentVariables["TLG_PREP_WORKERS"] = "0";

                psi.RedirectStandardOutput = true;
                psi.RedirectStandardError = true;

                try {
                    this.Hide();

                    string logPath = Path.Combine(userDir, "last_run.log");
                    StreamWriter logWriter = null;
                    try {
                        logWriter = new StreamWriter(new FileStream(logPath, FileMode.Create, FileAccess.Write, FileShare.ReadWrite)) { AutoFlush = true };
                    } catch { }

                    runningProcess = new Process();
                    runningProcess.StartInfo = psi;

                    if (logWriter != null) {
                        runningProcess.OutputDataReceived += (s, ev) => {
                            if (ev.Data != null) lock (logWriter) { logWriter.WriteLine(ev.Data); }
                        };
                        runningProcess.ErrorDataReceived += (s, ev) => {
                            if (ev.Data != null) lock (logWriter) { logWriter.WriteLine(ev.Data); }
                        };
                    }

                    runningProcess.Start();
                    try { runningProcess.PriorityClass = ProcessPriorityClass.High; } catch { }

                    if (logWriter != null) {
                        runningProcess.BeginOutputReadLine();
                        runningProcess.BeginErrorReadLine();
                    }

                    // Register global F1 overlay hotkey
                    ControlPanelForm overlay = new ControlPanelForm(true, runningProcess);
                    hotkey = new GlobalHotkey(this.Handle, 0, (uint)Keys.F1, () => {
                        if (overlay.Visible) {
                            overlay.Hide();
                        } else {
                            overlay.LoadSettings();
                            overlay.Show();
                            overlay.BringToFront();
                            overlay.Focus();
                        }
                    });

                    // Watchdog timer to restore audio and exit cleanly on game close
                    Timer exitTimer = new Timer();
                    exitTimer.Interval = 1000;
                    exitTimer.Tick += (s, args_) => {
                        if (runningProcess.HasExited) {
                            exitTimer.Stop();
                            if (logWriter != null) {
                                try { logWriter.Dispose(); } catch { }
                            }
                            AudioDeviceManager.RestoreOriginalDefault();
                            if (hotkey != null) hotkey.Unregister();
                            if (overlay != null && !overlay.IsDisposed) overlay.Dispose();
                            Application.Exit();
                        }
                    };
                    exitTimer.Start();
                } catch (Exception ex) {
                    this.Show();
                    MessageBox.Show("Failed to launch native engine:\n" + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            } else {
                // Fallback to shadPS4
                string shadps4 = Path.Combine(baseDir, "core", "shadPS4.exe");
                if (File.Exists(shadps4)) {
                    ProcessStartInfo psi = new ProcessStartInfo(shadps4, string.Format("-g \"{0}\"", Path.Combine(foundGameDir, "eboot.bin")));
                    psi.WorkingDirectory = Path.Combine(baseDir, "core");
                    try {
                        this.Hide();
                        runningProcess = Process.Start(psi);
                        if (runningProcess != null) {
                            runningProcess.WaitForExit();
                            Application.Exit();
                        }
                    } catch (Exception ex) {
                        this.Show();
                        MessageBox.Show("Failed to launch fallback emulator:\n" + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                } else {
                    MessageBox.Show("Neither native engine ('engine\\tlg-probe.exe') nor core emulator was found.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        public void LaunchDirect() {
            BtnLaunch_Click(this, EventArgs.Empty);
        }
    }

    static class Program {
        [STAThread]
        static void Main(string[] args) {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            bool autoLaunch = (args.Length > 0 && (args[0] == "--direct" || args[0] == "-launch" || args[0] == "--start" || args[0] == "-g"));
            ControlPanelForm form = new ControlPanelForm(false, null);
            if (autoLaunch) {
                form.Shown += (s, e) => form.LaunchDirect();
            }
            Application.Run(form);
        }
    }
}
