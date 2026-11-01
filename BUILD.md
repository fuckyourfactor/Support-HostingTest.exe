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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build-exact.ps1 -Configuration Release
```

The script resolves its project location from its own path, avoiding quoted directory paths supplied by MSBuild. Both the script and the GitHub workflow use `tools/hash-utils.ps1`, which computes SHA-256 with the .NET cryptography APIs instead of relying on `Get-FileHash` being available in the PowerShell session.

The original SHA-256 is `dfd183ccccf82d684ed6e4952aa071c251419024c2bbb78755563009f9f62c81`. `tools/build-exact.ps1` verifies this hash before and after copying.

The full resource inventory, string table, disassembly listing, PE metadata, and native-dialog control layout are in `assets/` and `analysis/`. Original resources are preserved even when they are only available in their raw binary form.

## GitHub Releases

The `.github/workflows/build.yml` workflow builds on `windows-2022` and publishes a GitHub Release automatically when pushing a tag beginning with `v`. It also supports manual execution with **Actions → Build and Release → Run workflow**, where `tag_name` must be set to the desired release tag (e.g. `v1.0.1`). There are no GitHub Actions artifacts.

```powershell
git add .
git commit -m "Fix PowerShell hashing and release workflow"
git push origin main
git tag v1.0.1
git push origin v1.0.1
```

Replace `main` with your branch name if different. Use a new tag if a tag has already been released. The workflow needs repository Actions enabled and permission to publish releases (`contents: write`, provided by the workflow; repository policies must also allow it).

Each GitHub Release contains:

- `HostingTest-Exact.exe` (original binary, byte-for-byte identical, renamed for clarity)
- `HostingTest-Reconstructed.exe` (compiled C++ reconstruction, **not** full 1:1)
- `HostingTest-Exact.zip` (original executable, extracted assets, PE metadata)
- `HostingTest-Reconstructed.zip` (compiled native executable, extracted assets, PE metadata)
- `SHA256SUMS.txt` (checksums for the four release files)

The Windows build has not been run in this Linux environment; GitHub Actions will perform the actual MSBuild and PowerShell steps.
