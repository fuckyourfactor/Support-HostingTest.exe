# HostingTest reconstruction project

Open `HostingTest.sln` in Visual Studio 2022 with Desktop development with C++ and the Windows SDK installed. Select `Release | Win32` and choose **Build Solution**.

Outputs:

- `dist/Release/Exact/HostingTest.exe` is a verified byte-for-byte copy of the uploaded original, not a source-code rebuild. This is the only version for which identical behavior, PE headers, embedded resources and version metadata are guaranteed.
- `dist/Release/Reconstructed/HostingTest.exe` is an independently compiled Windows C++ reconstruction that loads the **original dialog template**, bitmap and icon. Basic TCP/UDP diagnostics are implemented, but the original network protocol and complete MFC behavior have not been reconstructed and are **not 1:1**.
- Both outputs include an `assets/` directory with the original raw resources and converted BMP, ICO and CUR files, plus PE metadata. The same original resources remain embedded in the exact binary.

To regenerate the extracted resources from the original executable, run:

```powershell
python tools/extract_assets.py original/HostingTest.exe assets
```

To produce the exact binary package without Visual Studio, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-exact.ps1
```

The original SHA-256 is `dfd183ccccf82d684ed6e4952aa071c251419024c2bbb78755563009f9f62c81`. `tools/build-exact.ps1` verifies this hash both before and after copying.

The full resource inventory, string table, disassembly listing, PE metadata and native-dialog control layout are in `assets/` and `analysis/`. Original resources are preserved even when they are only available in their raw binary form.

The native project has not been compiled or run in this environment (Visual Studio is not available here). The GitHub Actions Windows workflow compiles both configurations and checks the hash of the exact output.
