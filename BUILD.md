# HostingTest

A standalone Win32 C++ reconstruction project for Visual Studio 2022. This is a **partial reconstruction**, not a complete 1:1 reimplementation of the earlier executable.

## Build

Open `HostingTest.sln` in Visual Studio 2022 with the C++ Desktop development workload and Windows SDK installed. Build **Release | Win32**.

The output is `dist/Release/HostingTest.exe`. Required runtime assets (the dialog, bitmap, icon and version information) are embedded in the executable by `HostingTest.rc`; no separate asset directory is required alongside the executable. The `assets/` directory is retained as source material and does not get packaged with GitHub Releases.

## GitHub Release

The workflow at `.github/workflows/build.yml` builds on `windows-2022` and publishes **only `HostingTest.exe`** in each GitHub Release. Run it by pushing a `v*` tag or manually from **Actions → Build and Release → Run workflow** (enter `tag_name`). If a Release for that tag already exists, the workflow deletes its old attachments before publishing the new EXE.

```powershell
git add -A
git commit -m "Keep only native HostingTest project and release executable"
git push origin main
git tag v1.0.2
git push origin v1.0.2
```

Choose a tag that has not been used, or use manual dispatch to update an existing Release. The native build was not run here because MSBuild is unavailable on this system.
