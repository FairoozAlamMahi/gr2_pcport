using System;
using System.IO;
using System.Diagnostics;
using System.Windows.Forms;

namespace GravityRush2Launcher {
    static class Program {
        [STAThread]
        static void Main() {
            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            
            // Candidate game eboot locations
            string[] gameCandidates = new string[] {
                Path.Combine(baseDir, "game", "eboot.bin"),
                Path.Combine(baseDir, "game", "merged_game", "eboot.bin"),
                Path.Combine(baseDir, "gr2_packages", "merged_game", "eboot.bin"),
                Path.Combine(baseDir, "..", "gr2_packages", "merged_game", "eboot.bin")
            };

            string foundEboot = null;
            foreach (var cand in gameCandidates) {
                if (File.Exists(cand)) {
                    foundEboot = Path.GetFullPath(cand);
                    break;
                }
            }

            if (foundEboot == null) {
                DialogResult res = MessageBox.Show(
                    "Welcome to Gravity Rush 2 PC Port!\n\n" +
                    "The game executable (eboot.bin) was not found.\n\n" +
                    "To play:\n" +
                    "1. Run 'Install_Game_PKG.bat' to install from your PS4 PKG files, OR\n" +
                    "2. Place your decrypted game files into the 'game' folder.\n\n" +
                    "Would you like to open the game directory now?",
                    "Gravity Rush 2 PC Port",
                    MessageBoxButtons.YesNo,
                    MessageBoxIcon.Information);

                if (res == DialogResult.Yes) {
                    string gameDir = Path.Combine(baseDir, "game");
                    if (!Directory.Exists(gameDir)) {
                        Directory.CreateDirectory(gameDir);
                    }
                    Process.Start("explorer.exe", baseDir);
                }
                return;
            }

            // Candidate core runtime locations
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
                MessageBox.Show(
                    "Core runtime (shadPS4.exe) was not found in 'core\\' or 'exodus_core\\'.\n" +
                    "Please ensure all port files are extracted properly.",
                    "Gravity Rush 2 PC Port - Error",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return;
            }

            // Candidate patch locations
            string[] patchCandidates = new string[] {
                Path.Combine(coreWorkingDir, "user", "patches", "GravityDaze2-Orbis.xml"),
                Path.Combine(baseDir, "core", "user", "patches", "GravityDaze2-Orbis.xml"),
                Path.Combine(baseDir, "exodus_core", "user", "patches", "GravityDaze2-Orbis.xml")
            };

            string foundPatch = null;
            foreach (var cand in patchCandidates) {
                if (File.Exists(cand)) {
                    foundPatch = Path.GetFullPath(cand);
                    break;
                }
            }

            // Build launch arguments
            string args = string.Format("-g \"{0}\"", foundEboot);
            if (!string.IsNullOrEmpty(foundPatch)) {
                args += string.Format(" -p \"{0}\"", foundPatch);
            }
            args += " --show-fps";

            // Launch game silently with zero command window
            ProcessStartInfo psi = new ProcessStartInfo();
            psi.FileName = foundCore;
            psi.Arguments = args;
            psi.WorkingDirectory = coreWorkingDir;
            psi.UseShellExecute = false;
            psi.CreateNoWindow = true;

            try {
                using (Process proc = Process.Start(psi)) {
                    if (proc != null) {
                        proc.WaitForExit();
                    }
                }
            } catch (Exception ex) {
                MessageBox.Show("Failed to launch game process:\n" + ex.Message,
                    "Gravity Rush 2 PC Port - Error",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }
    }
}
