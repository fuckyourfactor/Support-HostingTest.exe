# HostingTest reconstruction project

Open `HostingTest.sln` in Visual Studio 2022 with Desktop development with C++ and the Windows SDK installed. Select `Release | Win32` and choose **Build Solution**.

Outputs:

- `dist/Release/Exact/HostingTest.exe` is a verified byte-for-byte copy of the supplied executable, not a source-code rebuild. This is the only version for which identical behavior, PE headers, embedded resources, and version metadata are guaranteed.
- `dist/Release/Reconstructed/HostingTest.exe` is an independently compiled Windows C++ reconstruction that loads the original dialog template, bitmap, and icon. Basic TCP/UDP diagnostics are implemented, but the original network protocol and complete MFC behavior have not been reconstructed and are **not 1:1**.
- Both outputs include an `assets/` directory with the original raw resources and converted BMP, ICO, and CUR files, plus PE metadata.

To regenerate the resources from the original executable, run:

```powershell
python tools/extract_assets.py original/HostingTest.exe assets
```

To produce the exact binary package without Visual Studio, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-exact.ps1 -Configuration Release
```

The script resolves its project location from its own path and no longer accepts quoted directory paths from MSBuild; this fixes the `Test-Path : Illegal characters in path` failure on GitHub Actions.

The original SHA-256 is `dfd183ccccf82d684ed6e4952aa071c251419024c2bbb78755563009f9f62c81`. `tools/build-exact.ps1` verifies this hash before and after copying.

The full resource inventory, string table, disassembly listing, PE metadata, and native-dialog control layout are in `assets/` and `analysis/`. Original resources are preserved even when they are only available in their raw binary form.

## GitHub Releases

`.github/workflows/build.yml` builds on `windows-2022` and publishes a GitHub Release **only when pushing a tag beginning with `v`**, such as `v1.0.0`. It does not upload GitHub Actions artifacts.

```powershell
git add .
git commit -m "Fix exact build and publish GitHub releases"
git tag v1.0.0
git push origin main
git push origin v1.0.0
```

Replace `main` with your branch name if different. Use a new tag if `v1.0.0` already exists. The workflow needs repository Actions enabled and permission to publish releases (`contents: write`, provided by the workflow; repository policies must also allow it).

Each GitHub Release contains:

- `HostingTest-Exact.exe` (original binary, byte-for-byte identical, renamed for clarity)
- `HostingTest-Reconstructed.exe` (compiled C++ reconstruction, **not** full 1:1)
- `HostingTest-Exact.zip` (original executable, extracted assets, PE metadata)
- `HostingTest-Reconstructed.zip` (compiled native executable, extracted assets, PE metadata)
- `SHA256SUMS.txt` (checksums for the four release files)

The Windows build has not been run in this Linux environment; GitHub Actions will perform the actual MSBuild and PowerShell steps.
