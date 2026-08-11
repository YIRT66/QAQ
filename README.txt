EvolveMusic v0.11.3 SOURCE + PATH FIX

Root cause:
The earlier incremental Cloud Playback update contained CMake references to:
  core/CloudPolicyClient.h
  core/CloudPolicyClient.cpp

but those two files were accidentally omitted from that incremental ZIP.

This package fixes both issues:
1. Installs the missing CloudPolicyClient.h/.cpp files.
2. Refreshes CloudMusicClient.h/.cpp.
3. Installs the absolute-source-root CMakeLists.txt.
4. Removes stale build-mingw.

Usage:
- Extract the ZIP anywhere.
- Double-click INSTALL_FIX.bat.
- Then run:
    cd /d F:\EvolveMusic
    scripts\build_windows.bat clean

Required configure lines:
  EvolveMusic v0.11.3 SOURCE+PATH FIX: ACTIVE
  EvolveMusic source root: F:/EvolveMusic
  EvolveMusic binary root: F:/EvolveMusic/build-mingw
