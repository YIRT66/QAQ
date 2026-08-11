EvolveMusic - Qt WebView 6.10.0 MinGW direct installer
================================================================

For this machine:
  Qt kit:       E:\QT\6.10.0\mingw_64
  EvolveMusic:  F:\EvolveMusic
  Architecture: Windows x64 / MinGW

Recommended:
  1. Double-click install_qt_webview.bat
  2. After it says QT WEBVIEW INSTALL SUCCESS:
       cd /d F:\EvolveMusic
       scripts\build_windows.bat clean
  3. Then:
       scripts\run_windows.bat

Or use:
  install_webview_and_rebuild.bat

What this package does:
- Does NOT use Qt Maintenance Tool.
- Does NOT require a Qt account.
- Downloads the Qt WebView 6.10.0 MinGW x64 archive directly from the
  official download.qt.io online repository.
- Downloads the accompanying .sha1 file and verifies the archive before
  installing anything.
- Keeps temporary downloads on F:\QtWebViewInstallerCache when F: exists,
  to avoid consuming C: space.
- Finds the actual Qt root inside the archive automatically, then merges
  only that package into E:\QT\6.10.0\mingw_64.
- Does not install Qt WebEngine.

Extractor:
- Prefers 7z.exe / 7za.exe / 7zr.exe if present.
- Falls back to Windows tar.exe.
- If your tar.exe cannot unpack .7z, install 7-Zip and rerun the script.

After successful installation, you can delete:
  F:\QtWebViewInstallerCache

Important:
The WebView module is only the Qt side. On Windows, Qt WebView 6.10 uses
the Microsoft Edge WebView2 backend. Most current Windows systems with
Microsoft Edge already have the WebView2 Runtime.
