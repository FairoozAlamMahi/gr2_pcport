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

    public class SettingsManager {
        public static string BaseDir = AppDomain.CurrentDomain.BaseDirectory;
        
        public static string GetConfigPath() {
            string custom = Path.Combine(BaseDir, "core", "user", "custom_configs", "CUSA04943.json");
            if (File.Exists(custom)) return custom;
            string exCustom = Path.Combine(BaseDir, "exodus_core", "user", "custom_configs", "CUSA04943.json");
            if (File.Exists(exCustom)) return exCustom;
            return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "shadPS4", "custom_configs", "CUSA04943.json");
        }

        public static string GetBaseConfigPath() {
            string b1 = Path.Combine(BaseDir, "core", "user", "config.json");
            if (File.Exists(b1)) return b1;
            string b2 = Path.Combine(BaseDir, "exodus_core", "user", "config.json");
            if (File.Exists(b2)) return b2;
            return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "shadPS4", "config.json");
        }

        public static void UpdateConfig(int vblank, int audioBackend, int volumeSlider, string screenMode, int resWidth, int resHeight, int rcas) {
            List<string> paths = new List<string>() {
                GetConfigPath(),
                GetBaseConfigPath(),
                Path.Combine(BaseDir, "core", "user", "config.json"),
                Path.Combine(BaseDir, "core", "user", "custom_configs", "CUSA04943.json"),
                Path.Combine(BaseDir, "exodus_core", "user", "config.json"),
                Path.Combine(BaseDir, "exodus_core", "user", "custom_configs", "CUSA04943.json"),
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "shadPS4", "config.json"),
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "shadPS4", "custom_configs", "CUSA04943.json")
            };

            foreach (var p in paths) {
                if (File.Exists(p)) {
                    try {
                        string text = File.ReadAllText(p);
                        // Update vblank_frequency
                        text = Regex.Replace(text, "\"vblank_frequency\"\\s*:\\s*\\d+", "\"vblank_frequency\": " + vblank);
                        // Update audio_backend
                        text = Regex.Replace(text, "\"audio_backend\"\\s*:\\s*\\d+", "\"audio_backend\": " + audioBackend);
                        // Update volume_slider
                        text = Regex.Replace(text, "\"volume_slider\"\\s*:\\s*\\d+", "\"volume_slider\": " + volumeSlider);
                        // Update full_screen_mode
                        if (!string.IsNullOrEmpty(screenMode)) {
                            text = Regex.Replace(text, "\"full_screen_mode\"\\s*:\\s*\"[^\"]*\"", "\"full_screen_mode\": \"" + screenMode + "\"");
                            text = Regex.Replace(text, "\"full_screen\"\\s*:\\s*(true|false)", "\"full_screen\": " + (screenMode == "Windowed" ? "false" : "true"));
                        }
                        // Update internal screen resolution
                        if (resWidth > 0 && resHeight > 0) {
                            text = Regex.Replace(text, "\"internal_screen_width\"\\s*:\\s*\\d+", "\"internal_screen_width\": " + resWidth);
                            text = Regex.Replace(text, "\"internal_screen_height\"\\s*:\\s*\\d+", "\"internal_screen_height\": " + resHeight);
                            text = Regex.Replace(text, "\"window_width\"\\s*:\\s*\\d+", "\"window_width\": " + resWidth);
                            text = Regex.Replace(text, "\"window_height\"\\s*:\\s*\\d+", "\"window_height\": " + resHeight);
                        }
                        // Update RCAS
                        if (rcas >= 0) {
                            text = Regex.Replace(text, "\"rcas_attenuation\"\\s*:\\s*\\d+", "\"rcas_attenuation\": " + rcas);
                            text = Regex.Replace(text, "\"rcas_enabled\"\\s*:\\s*(true|false)", "\"rcas_enabled\": " + (rcas > 0 ? "true" : "false"));
                        }
                        File.WriteAllText(p, text);
                    } catch { }
                }
            }
        }

        public static void ResetAudioSavedata() {
            string templateDir = Path.Combine(BaseDir, "core", "user", "home", "1000", "savedata", "CUSA04943", "system0020.orig");
            if (!Directory.Exists(templateDir)) {
                templateDir = Path.Combine(BaseDir, "exodus_core", "user", "home", "1000", "savedata", "CUSA04943", "system0020.orig");
            }
            if (!Directory.Exists(templateDir)) {
                templateDir = Path.Combine(BaseDir, "core", "user", "home", "1000", "savedata", "CUSA04943", "system0020");
            }
            if (!Directory.Exists(templateDir)) {
                templateDir = Path.Combine(BaseDir, "exodus_core", "user", "home", "1000", "savedata", "CUSA04943", "system0020");
            }

            string[] targets = new string[] {
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "shadPS4", "home", "1000", "savedata", "CUSA04943", "system0020"),
                Path.Combine(BaseDir, "core", "user", "home", "1000", "savedata", "CUSA04943", "system0020"),
                Path.Combine(BaseDir, "exodus_core", "user", "home", "1000", "savedata", "CUSA04943", "system0020")
            };

            foreach (var t in targets) {
                try {
                    if (Directory.Exists(templateDir)) {
                        if (!Directory.Exists(t)) Directory.CreateDirectory(t);
                        foreach (var f in Directory.GetFiles(templateDir, "*.*", SearchOption.TopDirectoryOnly)) {
                            string dest = Path.Combine(t, Path.GetFileName(f));
                            File.Copy(f, dest, true);
                        }
                        string sceSysDir = Path.Combine(templateDir, "sce_sys");
                        if (Directory.Exists(sceSysDir)) {
                            string destSceSys = Path.Combine(t, "sce_sys");
                            if (!Directory.Exists(destSceSys)) Directory.CreateDirectory(destSceSys);
                            foreach (var f in Directory.GetFiles(sceSysDir, "*.*", SearchOption.TopDirectoryOnly)) {
                                File.Copy(f, Path.Combine(destSceSys, Path.GetFileName(f)), true);
                            }
                        }
                    } else if (Directory.Exists(t)) {
                        Directory.Delete(t, true);
                    }
                } catch { }
            }
        }
    }

    public static class BgmPlayer {
        [DllImport("winmm.dll")]
        public static extern int waveOutGetNumDevs();

        [StructLayout(LayoutKind.Sequential)]
        public struct WAVEFORMATEX {
            public ushort wFormatTag;
            public ushort nChannels;
            public uint nSamplesPerSec;
            public uint nAvgBytesPerSec;
            public ushort nBlockAlign;
            public ushort wBitsPerSample;
            public ushort cbSize;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct WAVEHDR {
            public IntPtr lpData;
            public uint dwBufferLength;
            public uint dwBytesRecorded;
            public IntPtr dwUser;
            public uint dwFlags;
            public uint dwLoops;
            public IntPtr lpNext;
            public IntPtr reserved;
        }

        [DllImport("winmm.dll")]
        public static extern int waveOutOpen(out IntPtr hWaveOut, int uDeviceID, ref WAVEFORMATEX lpFormat, IntPtr dwCallback, IntPtr dwInstance, uint fdwOpen);

        [DllImport("winmm.dll")]
        public static extern int waveOutPrepareHeader(IntPtr hWaveOut, ref WAVEHDR lpWaveHdr, int uSize);

        [DllImport("winmm.dll")]
        public static extern int waveOutWrite(IntPtr hWaveOut, ref WAVEHDR lpWaveHdr, int uSize);

        [DllImport("winmm.dll")]
        public static extern int waveOutReset(IntPtr hWaveOut);

        [DllImport("winmm.dll")]
        public static extern int waveOutClose(IntPtr hWaveOut);

        [DllImport("winmm.dll")]
        public static extern int waveOutUnprepareHeader(IntPtr hWaveOut, ref WAVEHDR lpWaveHdr, int uSize);

        private static volatile bool isPlaying = false;
        private static bool isEnabled = true;
        private static System.Threading.Thread loopThread = null;
        private static System.Media.SoundPlayer fallbackPlayer = null;

        public static bool IsEnabled {
            get { return isEnabled; }
            set {
                isEnabled = value;
                if (!isEnabled) {
                    Stop();
                } else if (!isPlaying) {
                    Start();
                }
            }
        }

        public static string FindBgmPath() {
            string baseDir = SettingsManager.BaseDir;
            string[] candidates = new string[] {
                Path.Combine(baseDir, "audio", "title_bgm.wav"),
                Path.Combine(baseDir, "core", "user", "title_bgm.wav"),
                Path.Combine(baseDir, "..", "audio", "title_bgm.wav"),
                Path.Combine(baseDir, "title_bgm.wav")
            };
            foreach (var c in candidates) {
                if (File.Exists(c)) return Path.GetFullPath(c);
            }
            return null;
        }

        public static void Start() {
            if (!isEnabled) return;
            string bgm = FindBgmPath();
            if (bgm == null) return;
            try {
                Stop();
                isPlaying = true;
                loopThread = new System.Threading.Thread(() => MultiDeviceWorker(bgm));
                loopThread.IsBackground = true;
                loopThread.Start();
            } catch {
                try {
                    fallbackPlayer = new System.Media.SoundPlayer(bgm);
                    fallbackPlayer.Load();
                    fallbackPlayer.PlayLooping();
                } catch { }
            }
        }

        public static void Stop() {
            isPlaying = false;
            if (loopThread != null) {
                try { loopThread.Join(400); } catch { }
                loopThread = null;
            }
            if (fallbackPlayer != null) {
                try {
                    fallbackPlayer.Stop();
                    fallbackPlayer.Dispose();
                } catch { }
                fallbackPlayer = null;
            }
        }

        private static void MultiDeviceWorker(string wavPath) {
            try {
                byte[] fileBytes = File.ReadAllBytes(wavPath);
                if (fileBytes.Length < 44) return;
                ushort channels = BitConverter.ToUInt16(fileBytes, 22);
                uint sampleRate = BitConverter.ToUInt32(fileBytes, 24);
                uint byteRate = BitConverter.ToUInt32(fileBytes, 28);
                ushort blockAlign = BitConverter.ToUInt16(fileBytes, 32);
                ushort bitsPerSample = BitConverter.ToUInt16(fileBytes, 34);

                int dataPos = 12;
                while (dataPos < fileBytes.Length - 8) {
                    string chunkId = Encoding.ASCII.GetString(fileBytes, dataPos, 4);
                    int chunkSize = BitConverter.ToInt32(fileBytes, dataPos + 4);
                    if (chunkId == "data") {
                        dataPos += 8;
                        break;
                    }
                    dataPos += 8 + chunkSize;
                }

                int audioDataLen = fileBytes.Length - dataPos;
                if (audioDataLen <= 0) return;

                WAVEFORMATEX wfx = new WAVEFORMATEX();
                wfx.wFormatTag = 1; // PCM
                wfx.nChannels = channels;
                wfx.nSamplesPerSec = sampleRate;
                wfx.nAvgBytesPerSec = byteRate;
                wfx.nBlockAlign = blockAlign;
                wfx.wBitsPerSample = bitsPerSample;
                wfx.cbSize = 0;

                IntPtr pAudioData = Marshal.AllocHGlobal(audioDataLen);
                Marshal.Copy(fileBytes, dataPos, pAudioData, audioDataLen);

                int numDevs = waveOutGetNumDevs();
                List<int> targetDevs = new List<int>();
                for (int d = 0; d < numDevs; d++) {
                    targetDevs.Add(d);
                }

                while (isPlaying) {
                    List<IntPtr> handles = new List<IntPtr>();
                    List<WAVEHDR> hdrs = new List<WAVEHDR>();

                    foreach (int d in targetDevs) {
                        IntPtr hWave = IntPtr.Zero;
                        if (waveOutOpen(out hWave, d, ref wfx, IntPtr.Zero, IntPtr.Zero, 0) == 0) {
                            WAVEHDR hdr = new WAVEHDR();
                            hdr.lpData = pAudioData;
                            hdr.dwBufferLength = (uint)audioDataLen;
                            hdr.dwFlags = 0;
                            waveOutPrepareHeader(hWave, ref hdr, Marshal.SizeOf(typeof(WAVEHDR)));
                            waveOutWrite(hWave, ref hdr, Marshal.SizeOf(typeof(WAVEHDR)));
                            handles.Add(hWave);
                            hdrs.Add(hdr);
                        }
                    }

                    double durationMs = ((double)audioDataLen / byteRate) * 1000.0;
                    int sleepInterval = 200;
                    double elapsed = 0;
                    while (isPlaying && elapsed < durationMs - 100) {
                        System.Threading.Thread.Sleep(sleepInterval);
                        elapsed += sleepInterval;
                    }

                    for (int i = 0; i < handles.Count; i++) {
                        waveOutReset(handles[i]);
                        WAVEHDR h = hdrs[i];
                        waveOutUnprepareHeader(handles[i], ref h, Marshal.SizeOf(typeof(WAVEHDR)));
                        waveOutClose(handles[i]);
                    }
                }

                Marshal.FreeHGlobal(pAudioData);
            } catch { }
        }
    }

    public class ControlPanelForm : Form {
        private ComboBox cbFps;
        private ComboBox cbAudioBackend;
        private ComboBox cbVolumeBoost;
        private ComboBox cbScreenMode;
        private ComboBox cbResolution;
        private ComboBox cbRcas;
        private ComboBox cbBgm;
        private Button btnLaunch;
        private Button btnResetAudio;
        private Label lblStatus;
        private Process runningProcess = null;
        private GlobalHotkey hotkey = null;
        private bool isOverlayMode = false;

        public ControlPanelForm(bool overlayMode = false, Process activeProc = null) {
            this.isOverlayMode = overlayMode;
            this.runningProcess = activeProc;
            InitializeComponent();
        }

        private void InitializeComponent() {
            this.Text = isOverlayMode ? "Gravity Rush 2 — In-Game Control Panel (F1 to Toggle)" : "Gravity Rush 2 PC Port — Master Launcher & Control Panel";
            this.Size = new Size(580, 615);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Color.FromArgb(24, 27, 40);
            this.ForeColor = Color.FromArgb(230, 235, 245);
            this.Font = new Font("Segoe UI", 9.5f, FontStyle.Regular);

            if (isOverlayMode) {
                this.TopMost = true;
                this.Opacity = 0.95;
            }

            // Header Banner
            Panel pnlHeader = new Panel();
            pnlHeader.Dock = DockStyle.Top;
            pnlHeader.Height = 70;
            pnlHeader.BackColor = Color.FromArgb(17, 19, 29);

            Label lblTitle = new Label();
            lblTitle.Text = "GRAVITY RUSH 2";
            lblTitle.Font = new Font("Segoe UI", 16f, FontStyle.Bold);
            lblTitle.ForeColor = Color.FromArgb(245, 185, 66);
            lblTitle.Location = new Point(20, 10);
            lblTitle.AutoSize = true;
            pnlHeader.Controls.Add(lblTitle);

            Label lblSub = new Label();
            lblSub.Text = isOverlayMode ? "In-Game Live Control Panel • Press F1 or ESC to Resume" : "Native Host Port (CUSA04943 v1.11) • Direct 60 FPS Vulkan 1.3";
            lblSub.Font = new Font("Segoe UI", 8.5f, FontStyle.Regular);
            lblSub.ForeColor = Color.FromArgb(160, 175, 200);
            lblSub.Location = new Point(22, 42);
            lblSub.AutoSize = true;
            pnlHeader.Controls.Add(lblSub);

            this.Controls.Add(pnlHeader);

            int startY = 85;
            int labelX = 30;
            int ctrlX = 250;
            int rowSpacing = 42;

            // 1. Framerate / vblank
            Label lblFps = new Label() { Text = "Framerate & Speed Mode:", Location = new Point(labelX, startY), AutoSize = true };
            cbFps = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 260, DropDownStyle = ComboBoxStyle.DropDownList };
            cbFps.Items.AddRange(new object[] {
                "60 FPS (Recommended - Locked 100% Speed)",
                "120 FPS (High-Refresh Gaming Displays)",
                "144 FPS (Ultra-Refresh Displays)",
                "30 FPS (PS4 Original Cinematic Mode)"
            });
            cbFps.SelectedIndex = 0;
            this.Controls.Add(lblFps);
            this.Controls.Add(cbFps);

            // 2. Audio Backend
            startY += rowSpacing;
            Label lblAudio = new Label() { Text = "Audio Sound Backend:", Location = new Point(labelX, startY), AutoSize = true };
            cbAudioBackend = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 260, DropDownStyle = ComboBoxStyle.DropDownList };
            cbAudioBackend.Items.AddRange(new object[] {
                "OpenAL Soft (Surround / Headset - Recommended)",
                "SDL3 Audio (DirectSound Native Stereo)"
            });
            cbAudioBackend.SelectedIndex = 0;
            this.Controls.Add(lblAudio);
            this.Controls.Add(cbAudioBackend);

            // 3. Master Volume Booster
            startY += rowSpacing;
            Label lblVol = new Label() { Text = "Master Volume Boost:", Location = new Point(labelX, startY), AutoSize = true };
            cbVolumeBoost = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 260, DropDownStyle = ComboBoxStyle.DropDownList };
            cbVolumeBoost.Items.AddRange(new object[] {
                "100% (Normal Baseline)",
                "150% (Enhanced)",
                "200% (2.0x Gain Boost - Recommended)",
                "300% (High Output for Quiet Headsets)"
            });
            cbVolumeBoost.SelectedIndex = 2; // 200%
            this.Controls.Add(lblVol);
            this.Controls.Add(cbVolumeBoost);

            // 4. Screen Mode
            startY += rowSpacing;
            Label lblScr = new Label() { Text = "Display Window Mode:", Location = new Point(labelX, startY), AutoSize = true };
            cbScreenMode = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 260, DropDownStyle = ComboBoxStyle.DropDownList };
            cbScreenMode.Items.AddRange(new object[] {
                "Borderless Fullscreen (Recommended)",
                "Windowed (1080p)",
                "Exclusive Fullscreen"
            });
            cbScreenMode.SelectedIndex = 0;
            this.Controls.Add(lblScr);
            this.Controls.Add(cbScreenMode);

            // 5. Internal Resolution
            startY += rowSpacing;
            Label lblRes = new Label() { Text = "Render Resolution:", Location = new Point(labelX, startY), AutoSize = true };
            cbResolution = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 260, DropDownStyle = ComboBoxStyle.DropDownList };
            cbResolution.Items.AddRange(new object[] {
                "1920 x 1080 (Native Full HD 1080p)",
                "2560 x 1440 (2K QHD High Fidelity)",
                "3840 x 2160 (4K Ultra HD)"
            });
            cbResolution.SelectedIndex = 0;
            this.Controls.Add(lblRes);
            this.Controls.Add(cbResolution);

            // 6. FidelityFX RCAS Sharpness
            startY += rowSpacing;
            Label lblRcas = new Label() { Text = "AMD FidelityFX RCAS Sharpness:", Location = new Point(labelX, startY), AutoSize = true };
            cbRcas = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 260, DropDownStyle = ComboBoxStyle.DropDownList };
            cbRcas.Items.AddRange(new object[] {
                "Crisp Clarity (RCAS 250 - Recommended)",
                "Ultra Sharp (RCAS 400)",
                "Subtle (RCAS 150)",
                "Disabled (Raw Bilinear)"
            });
            cbRcas.SelectedIndex = 0;
            this.Controls.Add(lblRcas);
            this.Controls.Add(cbRcas);

            // 7. Background Orchestral Soundtrack (BGM)
            startY += rowSpacing;
            Label lblBgm = new Label() { Text = "Background Soundtrack (BGM):", Location = new Point(labelX, startY), AutoSize = true };
            cbBgm = new ComboBox() { Location = new Point(ctrlX, startY - 2), Width = 260, DropDownStyle = ComboBoxStyle.DropDownList };
            cbBgm.Items.AddRange(new object[] {
                "Enabled (Official Kohei Tanaka Theme)",
                "Muted / Disabled"
            });
            cbBgm.SelectedIndex = BgmPlayer.IsEnabled ? 0 : 1;
            this.Controls.Add(lblBgm);
            this.Controls.Add(cbBgm);

            // 8. Audio Reset & Fix Helper
            startY += rowSpacing + 5;
            btnResetAudio = new Button();
            btnResetAudio.Text = "🔧 One-Click In-Game Sound Slider & Audio Reset";
            btnResetAudio.Location = new Point(labelX, startY);
            btnResetAudio.Size = new Size(480, 36);
            btnResetAudio.BackColor = Color.FromArgb(42, 48, 70);
            btnResetAudio.ForeColor = Color.FromArgb(240, 245, 255);
            btnResetAudio.FlatStyle = FlatStyle.Flat;
            btnResetAudio.Click += BtnResetAudio_Click;
            this.Controls.Add(btnResetAudio);

            // 8. Launch / Apply Button
            startY += 48;
            btnLaunch = new Button();
            btnLaunch.Text = isOverlayMode ? "✔ APPLY SETTINGS & RESUME (ESC / F1)" : "▶ LAUNCH GRAVITY RUSH 2 (60 FPS)";
            btnLaunch.Location = new Point(labelX, startY);
            btnLaunch.Size = new Size(480, 48);
            btnLaunch.BackColor = Color.FromArgb(215, 150, 40);
            btnLaunch.ForeColor = Color.FromArgb(15, 17, 26);
            btnLaunch.Font = new Font("Segoe UI", 12f, FontStyle.Bold);
            btnLaunch.FlatStyle = FlatStyle.Flat;
            btnLaunch.Click += BtnLaunch_Click;
            this.Controls.Add(btnLaunch);

            // 9. Status Bar & Tips
            startY += 56;
            lblStatus = new Label();
            lblStatus.Text = isOverlayMode ? "Tip: In Sound Options, turn Controller Speaker OFF to hear Kat's voice." : "Press F1 anytime in-game to toggle this Control Panel live!";
            lblStatus.Font = new Font("Segoe UI", 8.5f, FontStyle.Italic);
            lblStatus.ForeColor = Color.FromArgb(160, 180, 210);
            lblStatus.Location = new Point(labelX, startY);
            lblStatus.AutoSize = true;
            this.Controls.Add(lblStatus);

            this.KeyPreview = true;
            this.KeyDown += ControlPanelForm_KeyDown;
            LoadCurrentSettings();
        }

        public void LoadCurrentSettings() {
            try {
                string cfgPath = SettingsManager.GetConfigPath();
                if (!File.Exists(cfgPath)) cfgPath = SettingsManager.GetBaseConfigPath();
                if (File.Exists(cfgPath)) {
                    string txt = File.ReadAllText(cfgPath);
                    var mFps = Regex.Match(txt, "\"vblank_frequency\"\\s*:\\s*(\\d+)");
                    if (mFps.Success) {
                        int v = int.Parse(mFps.Groups[1].Value);
                        if (v == 120) cbFps.SelectedIndex = 1;
                        else if (v == 144) cbFps.SelectedIndex = 2;
                        else if (v == 30) cbFps.SelectedIndex = 3;
                        else cbFps.SelectedIndex = 0; // 60
                    }
                    var mAudio = Regex.Match(txt, "\"audio_backend\"\\s*:\\s*(\\d+)");
                    if (mAudio.Success) {
                        int a = int.Parse(mAudio.Groups[1].Value);
                        cbAudioBackend.SelectedIndex = (a == 1) ? 0 : 1;
                    }
                    var mVol = Regex.Match(txt, "\"volume_slider\"\\s*:\\s*(\\d+)");
                    if (mVol.Success) {
                        int vol = int.Parse(mVol.Groups[1].Value);
                        if (vol <= 100) cbVolumeBoost.SelectedIndex = 0;
                        else if (vol <= 150) cbVolumeBoost.SelectedIndex = 1;
                        else if (vol <= 200) cbVolumeBoost.SelectedIndex = 2;
                        else cbVolumeBoost.SelectedIndex = 3;
                    }
                    var mScr = Regex.Match(txt, "\"full_screen_mode\"\\s*:\\s*\"([^\"]*)\"");
                    if (mScr.Success) {
                        string s = mScr.Groups[1].Value;
                        if (s.IndexOf("Windowed", StringComparison.OrdinalIgnoreCase) >= 0) cbScreenMode.SelectedIndex = 1;
                        else if (s.IndexOf("Fullscreen", StringComparison.OrdinalIgnoreCase) >= 0) cbScreenMode.SelectedIndex = 2;
                        else cbScreenMode.SelectedIndex = 0;
                    }
                }
                if (cbBgm != null) {
                    cbBgm.SelectedIndex = BgmPlayer.IsEnabled ? 0 : 1;
                }
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
            SettingsManager.UpdateConfig(60, 1, 200, "Windowed", 1920, 1080, 250);
            BgmPlayer.Start();
            if (cbBgm != null) cbBgm.SelectedIndex = 0;
            MessageBox.Show(
                "Audio sliders and settings have been restored to clean defaults!\n\n" +
                "• All sound sliders (Master, BGM, SFX, Voice) are reset to 100% full volume.\n" +
                "• OpenAL Soft surround engine configured with 2.0x volume gain.\n" +
                "• Background Orchestral Soundtrack active (Official Kohei Tanaka Theme).\n" +
                "• In-Game Tip: In Sound Options, ensure 'Controller Speaker' is OFF so dialogue routes to your headphones/speakers.",
                "Gravity Rush 2 Audio Restored",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
            lblStatus.Text = "Audio sliders successfully restored to authentic 100% default!";
        }

        private void ApplySettings() {
            int vblank = 60;
            if (cbFps.SelectedIndex == 1) vblank = 120;
            else if (cbFps.SelectedIndex == 2) vblank = 144;
            else if (cbFps.SelectedIndex == 3) vblank = 30;

            int audioBackend = (cbAudioBackend.SelectedIndex == 0) ? 1 : 0; // 1 = OpenAL, 0 = SDL

            int volumeSlider = 200;
            if (cbVolumeBoost.SelectedIndex == 0) volumeSlider = 100;
            else if (cbVolumeBoost.SelectedIndex == 1) volumeSlider = 150;
            else if (cbVolumeBoost.SelectedIndex == 2) volumeSlider = 200;
            else if (cbVolumeBoost.SelectedIndex == 3) volumeSlider = 300;

            string screenMode = "Windowed";
            if (cbScreenMode.SelectedIndex == 0) screenMode = "Borderless";
            else if (cbScreenMode.SelectedIndex == 2) screenMode = "Fullscreen";

            int resW = 1920, resH = 1080;
            if (cbResolution.SelectedIndex == 1) { resW = 2560; resH = 1440; }
            else if (cbResolution.SelectedIndex == 2) { resW = 3840; resH = 2160; }

            int rcas = 250;
            if (cbRcas.SelectedIndex == 1) rcas = 400;
            else if (cbRcas.SelectedIndex == 2) rcas = 150;
            else if (cbRcas.SelectedIndex == 3) rcas = 0;

            if (cbBgm != null) {
                BgmPlayer.IsEnabled = (cbBgm.SelectedIndex == 0);
            }

            SettingsManager.UpdateConfig(vblank, audioBackend, volumeSlider, screenMode, resW, resH, rcas);
        }

        private void BtnLaunch_Click(object sender, EventArgs e) {
            ApplySettings();

            if (isOverlayMode) {
                this.Hide();
                return;
            }

            // Launch Game
            string baseDir = SettingsManager.BaseDir;
            string[] gameCandidates = new string[] {
                Path.Combine(baseDir, "game", "eboot.bin"),
                Path.Combine(baseDir, "game", "merged_game", "eboot.bin"),
                Path.Combine(baseDir, "gr2_packages", "merged_game", "eboot.bin"),
                Path.Combine(baseDir, "..", "gr2_packages", "merged_game", "eboot.bin")
            };

            string foundEboot = null;
            foreach (var cand in gameCandidates) {
                if (File.Exists(cand)) { foundEboot = Path.GetFullPath(cand); break; }
            }

            if (foundEboot == null) {
                MessageBox.Show(
                    "The game executable (eboot.bin) was not found in 'game/'.\n\n" +
                    "Please run 'Install_Game_PKG.bat' to install from your PKG files first.",
                    "Gravity Rush 2 PC Port",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
                return;
            }

            string[] coreCandidates = new string[] {
                Path.Combine(baseDir, "core", "shadPS4.exe"),
                Path.Combine(baseDir, "exodus_core", "shadPS4.exe"),
                Path.Combine(baseDir, "shadPS4.exe")
            };

            string foundCore = null;
            string coreWorkingDir = null;
            foreach (var cand in coreCandidates) {
                if (File.Exists(cand)) {
                    foundCore = Path.GetFullPath(cand);
                    coreWorkingDir = Path.GetDirectoryName(foundCore);
                    break;
                }
            }

            if (foundCore == null) {
                MessageBox.Show("Core runtime (shadPS4.exe) was not found in 'core\\' or 'exodus_core\\'.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            string[] patchCandidates = new string[] {
                Path.Combine(coreWorkingDir, "user", "patches", "GravityDaze2-Orbis.xml"),
                Path.Combine(baseDir, "core", "user", "patches", "GravityDaze2-Orbis.xml"),
                Path.Combine(baseDir, "exodus_core", "user", "patches", "GravityDaze2-Orbis.xml")
            };

            string foundPatch = null;
            foreach (var cand in patchCandidates) {
                if (File.Exists(cand)) { foundPatch = Path.GetFullPath(cand); break; }
            }

            string args = string.Format("-g \"{0}\"", foundEboot);
            if (!string.IsNullOrEmpty(foundPatch)) {
                args += string.Format(" -p \"{0}\"", foundPatch);
            }
            args += " --show-fps";

            ProcessStartInfo psi = new ProcessStartInfo();
            psi.FileName = foundCore;
            psi.Arguments = args;
            psi.WorkingDirectory = coreWorkingDir;
            psi.UseShellExecute = false;
            psi.CreateNoWindow = false;

            try {
                this.Hide();
                runningProcess = Process.Start(psi);
                if (runningProcess != null) {
                    BgmPlayer.Start();

                    // Create in-game overlay instance
                    ControlPanelForm overlay = new ControlPanelForm(true, runningProcess);
                    hotkey = new GlobalHotkey(this.Handle, 0, (uint)Keys.F1, () => {
                        if (overlay.Visible) {
                            overlay.Hide();
                        } else {
                            overlay.LoadCurrentSettings();
                            overlay.Show();
                            overlay.BringToFront();
                            overlay.Focus();
                        }
                    });

                    // Wait for game exit in background
                    Timer exitTimer = new Timer();
                    exitTimer.Interval = 1000;
                    exitTimer.Tick += (s, args_) => {
                        if (runningProcess.HasExited) {
                            exitTimer.Stop();
                            BgmPlayer.Stop();
                            if (hotkey != null) hotkey.Unregister();
                            if (overlay != null && !overlay.IsDisposed) overlay.Dispose();
                            Application.Exit();
                        }
                    };
                    exitTimer.Start();
                }
            } catch (Exception ex) {
                this.Show();
                MessageBox.Show("Failed to launch game process:\n" + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
