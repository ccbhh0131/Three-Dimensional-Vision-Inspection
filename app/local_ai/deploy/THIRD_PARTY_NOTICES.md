# Runtime licenses and source access

Vision3D Local AI worker and deployment scripts: Apache-2.0; see
licenses/Vision3D-Apache-2.0.txt. Worker source and CMake build instructions:
https://github.com/ccbhh0131/Three-Dimensional-Vision-Inspection/tree/dev/stage7-local-ai/app/local_ai

## Qt Core 6.5.3

Copyright The Qt Company Ltd. and other contributors. Unmodified Open Source
Qt6Core.dll is dynamically linked under LGPL-3.0. LGPL-3.0 and GPL-3.0 texts and
the source license collection are in licenses/qt. Qt Core attribution records,
copyright notices and referenced third-party license texts are in
licenses/qt-third-party. This conservatively includes Qt Core source-tree
notices for other platforms; it does not imply those platform binaries ship.

Corresponding QtBase source (same 6.5.3 version), available without charge:
https://download.qt.io/archive/qt/6.5/6.5.3/submodules/qtbase-everywhere-src-6.5.3.zip
Source mirror/tag and build files: https://github.com/qt/qtbase/tree/v6.5.3

You may replace the dynamically loaded Qt6Core.dll with an interface-compatible
modified build, and reverse-engineer this application to debug modifications to
the LGPL library. No signature check, activation or other restriction prevents
this. Preserve these notices and source-access information when conveying the
package; make the corresponding library source available to recipients under
the LGPL/GPL terms. No Qt SDK, Creator or build tools are bundled.

## Microsoft Visual C++ runtime

Copyright Microsoft Corporation. All rights reserved. The four unmodified x64
DLLs (msvcp140.dll, msvcp140_1.dll, vcruntime140.dll, vcruntime140_1.dll) are copied
from the installed Visual Studio 2022 VC/Redist/MSVC release CRT directory,
not debug_nonredist or System32. They are deployed app-locally with this program.
They are not covered by the application's Apache license. Applicable Microsoft
license terms are included in licenses/microsoft; distribution is subject to
the Distributable Code conditions of that license.

Official permitted-file list:
https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution
Official app-local deployment documentation:
https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files

Windows 10/11 supplies Universal CRT and the other Windows system DLLs. No
Windows system DLL, redistributable installer, SDK or development tool is copied
into the package. App-local runtime security updates require replacing these
DLLs with an approved newer redistributable build when updating this package.

## Syncthing 2.1.5

Copyright The Syncthing Authors. Mozilla Public License 2.0 and bundled
third-party notices: licenses/syncthing/LICENSE.txt and AUTHORS.txt.
Unmodified official Windows amd64 release executable, reused from the verified
archive with SHA-256:
39571e4d0900c2a2cab14c0b170f49751340a869e49734ccc8079d9b98a7974b

Release: https://github.com/syncthing/syncthing/releases/tag/v2.1.5
Corresponding source: https://github.com/syncthing/syncthing/tree/v2.1.5

No real device certificate, private key, existing configuration, database or
user model is included. Host identities are generated only after extraction.
