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
        [PreserveSig] int GetAt(uint iProp, out PROPERTYKEY pkey);
        [PreserveSig] int GetValue(ref PROPERTYKEY key, out PROPVARIANT pv);
        [PreserveSig] int SetValue(ref PROPERTYKEY key, ref PROPVARIANT propvar);
        [PreserveSig] int Commit();
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct PROPERTYKEY {
        public Guid fmtid;
        public uint pid;
    }

    [StructLayout(LayoutKind.Explicit)]
    internal struct PROPVARIANT {
        [FieldOffset(0)] public ushort vt;
        [FieldOffset(8)] public IntPtr pwszVal;
    }

    public class AudioDeviceInfo {
        public string Name { get; set; }
        public string Id { get; set; }
        public bool IsDefault { get; set; }
        public bool IsHeadset { get; set; }

        public override string ToString() {
            if (IsDefault) {
                return Name;
            }
            if (IsHeadset) {
                return Name + " (Headset / Headphones)";
            }
            return Name;
        }
    }

    public static class AudioDeviceManager {
        public static string originalDefaultConsoleId = null;
        public static string originalDefaultMultimediaId = null;

        public static List<AudioDeviceInfo> GetActiveDevices() {
            var devices = new List<AudioDeviceInfo>();
            try {
                var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumeratorComObject();
                string currentDefaultId = GetDefaultDeviceId(ERole.eMultimedia);

                IMMDeviceCollection collection;
                // eRender = 0, DEVICE_STATE_ACTIVE = 1
                if (enumerator.EnumAudioEndpoints(0, 1, out collection) == 0 && collection != null) {
                    uint count = 0;
                    collection.GetCount(out count);
                    for (uint i = 0; i < count; i++) {
                        IMMDevice dev;
                        if (collection.Item(i, out dev) == 0 && dev != null) {
                            string devId;
                            dev.GetId(out devId);

                            string friendlyName = "Audio Endpoint";
                            IPropertyStore store;
                            // STGM_READ = 0
                            if (dev.OpenPropertyStore(0, out store) == 0 && store != null) {
                                PROPERTYKEY keyFriendlyName;
                                keyFriendlyName.fmtid = new Guid("a45c254e-df1c-4efd-8020-67d146a850e0");
                                keyFriendlyName.pid = 14;

                                PROPVARIANT pv;
                                if (store.GetValue(ref keyFriendlyName, out pv) == 0 && pv.vt == 31) {
                                    friendlyName = Marshal.PtrToStringUni(pv.pwszVal);
                                }
                            }

                            bool isHeadset = false;
                            string lower = friendlyName.ToLowerInvariant();
                            if (lower.Contains("head") || lower.Contains("ear") || lower.Contains("buds") || lower.Contains("wh-") || lower.Contains("wf-") || lower.Contains("airpods") || lower.Contains("hyperx") || lower.Contains("arctis") || lower.Contains("corsair")) {
                                isHeadset = true;
                            }

                            bool isDefault = (devId == currentDefaultId);

                            devices.Add(new AudioDeviceInfo() {
                                Name = friendlyName,
                                Id = devId,
                                IsDefault = isDefault,
                                IsHeadset = isHeadset
                            });
                        }
                    }
                }
            } catch { }
            return devices;
        }

        public static string GetDefaultDeviceId(ERole role) {
            try {
                var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumeratorComObject();
                IMMDevice dev;
                if (enumerator.GetDefaultAudioEndpoint(0, (int)role, out dev) == 0 && dev != null) {
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
            string[] candidates = new string[] {
                Path.Combine(BaseDir, "user", "tlg.ini"),
                Path.Combine(BaseDir, "GravityRush2_PC_Port", "user", "tlg.ini"),
                Path.Combine(BaseDir, "..", "user", "tlg.ini"),
                Path.Combine(BaseDir, "..", "GravityRush2_PC_Port", "user", "tlg.ini")
            };
            foreach (var c in candidates) {
                if (File.Exists(c)) return Path.GetFullPath(c);
            }
            string pPort = Path.Combine(BaseDir, "GravityRush2_PC_Port", "user", "tlg.ini");
            if (Directory.Exists(Path.Combine(BaseDir, "GravityRush2_PC_Port"))) return pPort;
            return Path.Combine(BaseDir, "user", "tlg.ini");
        }

        public static string GetLauncherSettingsPath() {
            return Path.Combine(BaseDir, "user", "launcher_settings.ini");
        }

        public static void ReadTlgIni(out string displayMode, out bool vsync, out bool showFps, out string engineFps, out string renderResolution) {
            displayMode = "borderless";
            vsync = true;
            showFps = true;
            engineFps = "60";
            renderResolution = "native";
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
                        } else if (tr.StartsWith("render_resolution", StringComparison.OrdinalIgnoreCase)) {
                            var parts = tr.Split('=');
                            if (parts.Length > 1) renderResolution = parts[1].Trim().ToLowerInvariant();
                        }
                    }
                } catch { }
            }
        }

        public static void WriteTlgIni(string displayMode, bool vsync, bool showFps, string engineFps, string renderResolution) {
            try {
                string path = GetTlgIniPath();
                string dir = Path.GetDirectoryName(path);
                if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);

                string normMode = "borderless";
                if (displayMode.Equals("windowed", StringComparison.OrdinalIgnoreCase)) normMode = "windowed";
                else if (displayMode.Equals("fullscreen", StringComparison.OrdinalIgnoreCase)) normMode = "fullscreen";

                string normRes = "native";
                if (renderResolution.Equals("720p", StringComparison.OrdinalIgnoreCase)) normRes = "720p";
                else if (renderResolution.Equals("480p", StringComparison.OrdinalIgnoreCase)) normRes = "480p";
                else if (renderResolution.Equals("1440p", StringComparison.OrdinalIgnoreCase)) normRes = "1440p";
                else if (renderResolution.Equals("4K", StringComparison.OrdinalIgnoreCase)) normRes = "4K";

                StringBuilder sb = new StringBuilder();
                sb.AppendLine("# tlg settings (in-game menu: Insert / R3+L2)");
                sb.AppendLine("display_mode=" + normMode);
                sb.AppendLine("vsync=" + (vsync ? "1" : "0"));
                sb.AppendLine("show_fps=" + (showFps ? "1" : "0"));
                sb.AppendLine("engine_fps=" + engineFps);
                sb.AppendLine("fps_limit=0");
                sb.AppendLine("frames_queued=2");
                sb.AppendLine("render_resolution=" + normRes);
                sb.AppendLine("deferred_readback=1");
                sb.AppendLine("stale_readback=1");
                sb.AppendLine("async_shaders=1");
                File.WriteAllText(path, sb.ToString());
            } catch { }
        }

        public static void ResetAudioSavedata() {
            string[] candidateDirs = new string[] {
                Path.Combine(BaseDir, "user", "savedata", "1", "CUSA04943", "system0020"),
                Path.Combine(BaseDir, "GravityRush2_PC_Port", "user", "savedata", "1", "CUSA04943", "system0020"),
                Path.Combine(BaseDir, "..", "user", "savedata", "1", "CUSA04943", "system0020")
            };

            foreach (var saveDir in candidateDirs) {
                if (Directory.Exists(saveDir)) {
                    string origFile = Path.Combine(saveDir, "system0020.orig");
                    string binFile = Path.Combine(saveDir, "system0020.bin");
                    try {
                        if (File.Exists(origFile)) {
                            File.Copy(origFile, binFile, true);
                        }
                    } catch { }
                }
            }
        }
    }

    public class ControlPanelForm : Form {
        private bool isOverlayMode;
        private Process runningProcess;
        private GlobalHotkey hotkey;

        private ComboBox cbProfile;
        private ComboBox cbAudioDevice;
        private ComboBox cbDisplayMode;
        private ComboBox cbFpsMode;
        private ComboBox cbShowFps;
        private ComboBox cbVsync;
        private Button btnResetAudio;
        private Button btnLaunch;
        private Label lblStatus;
        private List<AudioDeviceInfo> audioDevices;

        public ControlPanelForm(bool overlay, Process proc, int defaultProfile = 0) {
            this.isOverlayMode = overlay;
            this.runningProcess = proc;
            InitializeUI(defaultProfile);
        }

        private void InitializeUI(int defaultProfile = 0) {
            this.Text = isOverlayMode ? "Gravity Rush 2 — In-Game Control Panel (F1)" : "Gravity Rush 2 — PC Port Launcher & Settings";
            this.Size = new Size(600, 560);
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

            int startY = 78;
            int labelX = 25;
            int ctrlX = 225;
            int rowSpacing = 38;

            // 0. Optimization Profile Selector
            Label lblProfile = new Label() { Text = "Optimization Preset:", Location = new Point(labelX, startY), AutoSize = true };
            cbProfile = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 335, DropDownStyle = ComboBoxStyle.DropDownList };
            cbProfile.Items.AddRange(new object[] {
                "Profile 1: Native 1080p @ 60 FPS (240Hz Cadence, Full CPU/GPU) [Default]",
                "Profile 2: Performance 720p (Downscaled Quality - High FPS Test)",
                "Profile 3: Diagnostic Silent Mode (Decoupled Audio Timer Test)",
                "Profile 4: High Refresh 120 FPS Mode (120 FPS Engine, 240Hz VBlank)"
            });
            cbProfile.SelectedIndex = (defaultProfile >= 0 && defaultProfile <= 3) ? defaultProfile : 0;
            cbProfile.SelectedIndexChanged += (s, e) => {
                if (cbProfile.SelectedIndex == 1) {
                    lblStatus.Text = "Profile 2: 720p render resolution for maximum FPS testing.";
                } else if (cbProfile.SelectedIndex == 2) {
                    lblStatus.Text = "Profile 3: Audio timer decoupled (silent) for timing diagnostic.";
                } else if (cbProfile.SelectedIndex == 3) {
                    if (cbFpsMode.Items.Count > 2) cbFpsMode.SelectedIndex = 2; // 120 FPS
                    lblStatus.Text = "Profile 4: 120 FPS high refresh mode unlocked.";
                } else {
                    cbFpsMode.SelectedIndex = 0; // 60 FPS
                    lblStatus.Text = "Profile 1: Native 1080p @ 60 FPS (240Hz VBlank, Full CPU/GPU).";
                }
            };
            this.Controls.Add(lblProfile);
            this.Controls.Add(cbProfile);

            // 1. Audio Playback Device
            startY += rowSpacing;
            Label lblAudio = new Label() { Text = "Audio Playback Device:", Location = new Point(labelX, startY), AutoSize = true };
            cbAudioDevice = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 335, DropDownStyle = ComboBoxStyle.DropDownList };
            audioDevices = AudioDeviceManager.GetActiveDevices();
            string currentDefaultName = "System Default";
            for (int i = 0; i < audioDevices.Count; i++) {
                if (audioDevices[i].IsDefault) {
                    currentDefaultName = audioDevices[i].Name;
                    break;
                }
            }

            cbAudioDevice.Items.Add(new AudioDeviceInfo() {
                Name = "Windows Default (Follows: " + currentDefaultName + ")",
                Id = "",
                IsDefault = true,
                IsHeadset = false
            });

            for (int i = 0; i < audioDevices.Count; i++) {
                cbAudioDevice.Items.Add(audioDevices[i]);
            }
            cbAudioDevice.SelectedIndex = 0;
            this.Controls.Add(lblAudio);
            this.Controls.Add(cbAudioDevice);

            // 2. Framerate / Performance Mode
            startY += rowSpacing;
            Label lblFps = new Label() { Text = "Framerate & Performance:", Location = new Point(labelX, startY), AutoSize = true };
            cbFpsMode = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 335, DropDownStyle = ComboBoxStyle.DropDownList };
            cbFpsMode.Items.AddRange(new object[] {
                "60 FPS (Rock-Solid 60.0 FPS - Standard Mode - Recommended)",
                "30 FPS (PS4 Original Cinematic Mode)",
                "120 FPS (High Refresh Rate Mode)"
            });
            cbFpsMode.SelectedIndex = (defaultProfile == 3) ? 2 : 0;
            this.Controls.Add(lblFps);
            this.Controls.Add(cbFpsMode);

            // 3. Display Window Mode
            startY += rowSpacing;
            Label lblDisplay = new Label() { Text = "Display Window Mode:", Location = new Point(labelX, startY), AutoSize = true };
            cbDisplayMode = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 335, DropDownStyle = ComboBoxStyle.DropDownList };
            cbDisplayMode.Items.AddRange(new object[] {
                "Exclusive Fullscreen (Direct Display Output)",
                "Borderless Fullscreen (Recommended)",
                "Windowed (1080p)"
            });
            cbDisplayMode.SelectedIndex = 1; // Default to borderless
            this.Controls.Add(lblDisplay);
            this.Controls.Add(cbDisplayMode);

            // 4. In-Game Real-Time FPS Counter
            startY += rowSpacing;
            Label lblShowFps = new Label() { Text = "In-Game FPS Counter:", Location = new Point(labelX, startY), AutoSize = true };
            cbShowFps = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 335, DropDownStyle = ComboBoxStyle.DropDownList };
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
            cbVsync = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 335, DropDownStyle = ComboBoxStyle.DropDownList };
            cbVsync.Items.AddRange(new object[] {
                "Triple-Buffered Mailbox (Fluid 60 FPS, No Drops - Recommended)",
                "Standard FIFO VSync (Strict 60Hz Lock)",
                "Disabled (VSync Off)"
            });
            cbVsync.SelectedIndex = 0;
            this.Controls.Add(lblVsync);
            this.Controls.Add(cbVsync);

            // 6. One-Click Audio & Slider Reset
            startY += rowSpacing + 4;
            btnResetAudio = new Button();
            btnResetAudio.Text = "🔧 One-Click Sound Slider & Audio Reset";
            btnResetAudio.Location = new Point(labelX, startY);
            btnResetAudio.Size = new Size(535, 32);
            btnResetAudio.BackColor = Color.FromArgb(36, 44, 68);
            btnResetAudio.ForeColor = Color.FromArgb(235, 242, 255);
            btnResetAudio.FlatStyle = FlatStyle.Flat;
            btnResetAudio.Click += BtnResetAudio_Click;
            this.Controls.Add(btnResetAudio);

            // 7. Launch / Apply Button
            startY += 40;
            btnLaunch = new Button();
            btnLaunch.Text = isOverlayMode ? "✔ APPLY SETTINGS & RESUME (ESC / F1)" : "▶ LAUNCH GRAVITY RUSH 2";
            btnLaunch.Location = new Point(labelX, startY);
            btnLaunch.Size = new Size(535, 44);
            btnLaunch.BackColor = Color.FromArgb(225, 160, 40);
            btnLaunch.ForeColor = Color.FromArgb(14, 17, 26);
            btnLaunch.Font = new Font("Segoe UI", 12f, FontStyle.Bold);
            btnLaunch.FlatStyle = FlatStyle.Flat;
            btnLaunch.Click += BtnLaunch_Click;
            this.Controls.Add(btnLaunch);

            // 8. Hotkeys Legend & Status
            startY += 50;
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
                string mode, engineFps, renderRes;
                bool vsync, showFps;
                SettingsManager.ReadTlgIni(out mode, out vsync, out showFps, out engineFps, out renderRes);

                if (mode == "windowed") cbDisplayMode.SelectedIndex = 2;
                else if (mode == "fullscreen") cbDisplayMode.SelectedIndex = 0; // Exclusive Fullscreen
                else cbDisplayMode.SelectedIndex = 1; // borderless

                cbVsync.SelectedIndex = vsync ? 1 : 0;
                cbShowFps.SelectedIndex = showFps ? 0 : 1;

                if (engineFps == "30") cbFpsMode.SelectedIndex = 1;
                else if (engineFps == "120") cbFpsMode.SelectedIndex = 2;
                else cbFpsMode.SelectedIndex = 0; // default 60 FPS

                if (renderRes == "720p") cbProfile.SelectedIndex = 1;
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
            if (info != null && !string.IsNullOrEmpty(info.Id) && !info.IsDefault) {
                AudioDeviceManager.SetDefaultEndpoint(info.Id);
            }
            string currentTarget = (info != null) ? info.ToString() : "Windows Default Playback Device";
            MessageBox.Show(
                "Audio sliders and settings have been restored to clean defaults!\n\n" +
                "• All sound sliders (Master, BGM, SFX, Voice) are reset to 100% full volume.\n" +
                "• Native NGS2 audio synthesis active with real-time voices, combat SFX, and world music.\n" +
                "• Audio output routed directly to: " + currentTarget + ".\n" +
                "• In-game Tip: In Sound Options, set 'Controller Speaker' to OFF to hear Kat's voice lines on your headphones/speakers.",
                "Gravity Rush 2 Audio Restored",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
            lblStatus.Text = "Audio sliders successfully restored to authentic 100% defaults!";
        }

        private void ApplySettings() {
            string mode = "borderless";
            if (cbDisplayMode.SelectedIndex == 0) mode = "fullscreen";
            else if (cbDisplayMode.SelectedIndex == 2) mode = "windowed";

            bool vsync = (cbVsync.SelectedIndex == 1);
            bool showFps = (cbShowFps.SelectedIndex == 0);

            string fps = "60";
            if (cbFpsMode.SelectedIndex == 1) fps = "30";
            else if (cbFpsMode.SelectedIndex == 2) fps = "120";

            string renderRes = "native";
            if (cbProfile.SelectedIndex == 1) renderRes = "720p";

            SettingsManager.WriteTlgIni(mode, vsync, showFps, fps, renderRes);

            AudioDeviceInfo info = cbAudioDevice.SelectedItem as AudioDeviceInfo;
            if (info != null && !string.IsNullOrEmpty(info.Id) && !info.IsDefault) {
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
                Path.Combine(baseDir, "GravityRush2_PC_Port", "engine", "tlg-probe.exe"),
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
                Path.Combine(baseDir, "GravityRush2_PC_Port", "game"),
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

            string userDir = Path.GetDirectoryName(SettingsManager.GetTlgIniPath());
            if (!Directory.Exists(userDir)) {
                string userCand = Path.Combine(baseDir, "user");
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

                // Target profile settings
                string fps = "60";
                if (cbFpsMode.SelectedIndex == 1) fps = "30";
                else if (cbFpsMode.SelectedIndex == 2) fps = "120";

                string renderRes = "1920x1080";
                string audioBackend = "wasapi";

                int pIdx = cbProfile.SelectedIndex;
                if (pIdx == 1) {
                    // Profile 2: 720p Performance Downscale
                    renderRes = "1280x720";
                } else if (pIdx == 2) {
                    // Profile 3: Diagnostic No Audio (Timer Backend)
                    audioBackend = "none";
                } else if (pIdx == 3) {
                    // Profile 4: 120 FPS High Refresh
                    fps = "120";
                }

                // Rock-Solid Gravity Rush 2 PC Port environment:
                // High-performance asynchronous readbacks, device-local GPU buffer copies & 240Hz VBlank pacing
                psi.EnvironmentVariables["TLG_RENDER_RES"] = renderRes;
                psi.EnvironmentVariables["TLG_OUTPUT_RES"] = "1920x1080";
                psi.EnvironmentVariables["TLG_UPSCALER"] = "off";
                psi.EnvironmentVariables["TLG_VBLANK_HZ"] = "240";
                psi.EnvironmentVariables["TLG_ENGINE_FPS"] = fps;
                psi.EnvironmentVariables["TLG_FPS_LIMIT"] = "0";
                psi.EnvironmentVariables["TLG_READBACKS"] = "2";
                psi.EnvironmentVariables["TLG_DEFERRED_READBACK"] = "1";
                psi.EnvironmentVariables["TLG_STALE_READBACK"] = "1";
                psi.EnvironmentVariables["TLG_ASYNC_SHADERS"] = "1";
                psi.EnvironmentVariables["TLG_ASYNC_FENCES"] = "1";
                psi.EnvironmentVariables["TLG_COPY_THREADS"] = "6";
                psi.EnvironmentVariables["TLG_COPY_GPU_BUFFERS"] = "1";
                psi.EnvironmentVariables["TLG_COPY_MERGE_KB"] = "256";
                psi.EnvironmentVariables["TLG_FAST_READ_FAULT"] = "1";
                psi.EnvironmentVariables["TLG_HOT_PAGES"] = "1";
                psi.EnvironmentVariables["TLG_PRESENT_THREAD"] = "1";
                psi.EnvironmentVariables["TLG_STAGING_KEEP_MB"] = "512";
                psi.EnvironmentVariables["TLG_FRAMES_AHEAD"] = "2";
                psi.EnvironmentVariables["TLG_PREP_WORKERS"] = "4";
                psi.EnvironmentVariables["TLG_DRAW_PIPE"] = "1";
                psi.EnvironmentVariables["TLG_GC_BUDGET_MB"] = "6144";
                psi.EnvironmentVariables["TLG_DMEM_MB"] = "7168";
                psi.EnvironmentVariables["TLG_PIPELINE_BINARIES"] = "0";
                psi.EnvironmentVariables["TLG_JITTER"] = "0";
                psi.EnvironmentVariables["TLG_OBJECT_MOTION"] = "0";
                psi.EnvironmentVariables["TLG_FRAME_STATS"] = "1";
                psi.EnvironmentVariables["TLG_LANGUAGE"] = "1";              // English
                psi.EnvironmentVariables["TLG_NGS2"] = "hle";                // Clean-room C NGS2 audio synthesizer
                psi.EnvironmentVariables["TLG_SKIP_ADJACENCY"] = "1";        // Fix black streaks near characters
                psi.EnvironmentVariables["TLG_CONFIG"] = SettingsManager.GetTlgIniPath();

                if (audioBackend == "none") {
                    psi.EnvironmentVariables["TLG_AUDIO"] = "none";
                } else {
                    psi.EnvironmentVariables["TLG_AUDIO"] = "wasapi";
                }

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
                    ControlPanelForm overlay = new ControlPanelForm(true, runningProcess, pIdx);
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

            int targetProfile = 0;
            bool autoLaunch = false;

            for (int i = 0; i < args.Length; i++) {
                string a = args[i].ToLowerInvariant();
                if (a == "--direct" || a == "-launch" || a == "--start" || a == "-g") {
                    autoLaunch = true;
                } else if (a == "--profile1" || a == "-p1") {
                    targetProfile = 0;
                    autoLaunch = true;
                } else if (a == "--profile2" || a == "-p2") {
                    targetProfile = 1;
                    autoLaunch = true;
                } else if (a == "--profile3" || a == "-p3") {
                    targetProfile = 2;
                    autoLaunch = true;
                } else if (a == "--profile4" || a == "-p4") {
                    targetProfile = 3;
                    autoLaunch = true;
                }
            }

            ControlPanelForm form = new ControlPanelForm(false, null, targetProfile);
            if (autoLaunch) {
                form.Shown += (s, e) => form.LaunchDirect();
            }
            Application.Run(form);
        }
    }
}
