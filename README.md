# HostingTest

This repository contains only the **native, partially reconstructed** Win32 application. It does **not** include the original executable and is not a 1:1 reimplementation of its original network handshake.

## Build locally

Open `HostingTest.sln` in Visual Studio 2022 with the Desktop development with C++ workload and Windows SDK installed. Select **Release | Win32** and build. Output: `dist/Release/HostingTest.exe`.

The project uses size-oriented MSVC/LINK settings: `/O1`, `/Os`, `/GL` + `/LTCG`, `/Gy`, `/Gw`, `/GF`, `/Oi`, `/Oy`, `/OPT:REF`, `/OPT:ICF=9`, and non-incremental linking. It retains static CRT (`/MT`) so the executable does not need a separate Visual C++ redistributable. The network and UI code uses Win32 calls instead of `std::thread` and `std::wstring`. The original dialog template, bitmap, icon, and version metadata remain embedded in the executable; runtime asset files are unnecessary.

A **local Visual Studio build is uncompressed**. GitHub Actions builds the same code and additionally attempts UPX compression. This separation avoids making the ordinary local build depend on a downloaded tool.

## GitHub Release

The GitHub Actions workflow triggers on pushed `v*` tags or by manual `workflow_dispatch` with a tag name.

1. Compiles `Release | Win32` from source.
2. Downloads official **UPX 5.3.0** and checks its pinned SHA-256.
3. Packs `HostingTest.exe` using `--best --lzma`, preserving icon resources and version metadata.
4. Uses `upx --test`, verifies version metadata and checks that the compressed file is actually smaller. If any of these checks fail, it releases the working uncompressed build instead.
5. Removes previous attachments from an existing release and publishes **only `HostingTest.exe`**.

For a new release:

```powershell
git add -A
git commit -m "Optimize executable size"
git push origin main
git tag v1.0.5
git push origin v1.0.5
```

Alternatively use **Actions → Build and Release → Run workflow**.
