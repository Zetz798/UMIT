# UMIT mobile app

UMIT 0.3 uses a C++ core and Qt Quick/QML UI. Android is the current target; iOS is planned and has not been built or tested.

## Build Android

Install Qt 6.12 with an Android kit and its compatible SDK, NDK, and JDK. On Windows, use ASCII-only paths for the project, SDK, and build directory.

1. Open the root `CMakeLists.txt` in Qt Creator.
2. Choose the kit matching the phone's Android ABI. The Redmi A3 configuration uses **armeabi-v7a**, even though its processor supports 64-bit instructions.
3. Use a separate build directory for each kit, for example `build/android-armv7-debug`.
4. Set these CMake options:

   ```text
   TICKET_BUILD_MOBILE=ON
   TICKET_BUILD_CLI=OFF
   BUILD_TESTING=OFF
   ```

5. Configure, then build/deploy `ticket_mobile` through Qt Creator. To package an already configured build from a terminal:

   ```sh
   cmake --build build/android-armv7-debug --target apk --parallel 4
   ```

The debug APK is normally under `build/android-armv7-debug/mobile/android-build/build/outputs/apk/debug/`. Configure/Run CMake alone does not create it. APKs and generated build files are not stored in this repository.

The development setup used Qt 6.12.0, NDK 27.2.12479018, and JDK 21. See [Windows troubleshooting](../ANDROID_BUILD_FIX.md) and the [Qt Android setup guide](https://doc.qt.io/qt-6/android-getting-started.html).

## Desktop preview and tests

UMIT can also run as a native Windows application for recording a demo of the local ticket flow. Open the project with a Qt 6.12 Desktop MinGW kit and build `ticket_mobile`. The desktop app supports the same local accounts, tickets, and settings. **Phantom connection is currently mobile-only, and neither build sends Solana transactions.**

To package a Windows build without requiring Qt Creator on the destination machine, use `windeployqt` from the matching Qt installation. Keep the executable and the deployed DLL, plugin, and QML directories together. For example, from a Qt/MinGW build environment:

```powershell
cmake -S . -B build/desktop-demo-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64 -DTICKET_BUILD_MOBILE=ON -DTICKET_BUILD_CLI=OFF -DBUILD_TESTING=OFF -DUMIT_UI_VERIFY=OFF
cmake --build build/desktop-demo-release --target ticket_mobile --parallel 4
New-Item -ItemType Directory -Force build/windows-package
Copy-Item build/desktop-demo-release/mobile/ticket_mobile.exe build/windows-package/UMIT.exe
windeployqt --release --compiler-runtime --qmldir mobile build/windows-package/UMIT.exe
```

Adjust the Qt installation path for your machine. Record issuance and first/repeated redemption as a **local demo**; there is no ticket transaction to show in Explorer yet.

Open the same project with a matching Qt 6.12 Desktop kit. Enable `TICKET_BUILD_MOBILE`, `TICKET_BUILD_CLI`, and `BUILD_TESTING`; the Qt Test module is required. Build, then run:

```sh
ctest --test-dir build/desktop --output-on-failure
```

Replace `build/desktop` with your configured build directory. On Windows, Qt and the matching compiler runtime directories must be on `PATH`. CTest covers the ticket core, Phantom protocol, settings/app lock, and local accounts. Wallet connection itself requires a mobile device; desktop tests do not verify a live Phantom exchange.

Optional `UMIT_UI_VERIFY=ON` enables desktop-only UI checks. Set `UMIT_UI_VERIFY_DIR` to an output directory before launching `ticket_mobile`; the checks use isolated temporary account data and capture screenshots. These hooks are excluded from Android builds.

## Try the demo

1. Open Account and register a local account.
2. Open Tickets, issue a ticket, and redeem its ID: entry is granted once.
3. Redeem the same ID again: entry is denied.
4. Connect Phantom on Android and check the Devnet balance; test rejecting the connection and losing network access too.
5. Try profile editing, the three languages, themes, app lock, and logout.

Accounts and profiles persist only on this installation. Tickets live in memory and are cleared on account session changes. Different devices do not share ticket state. There is no camera/QR flow or on-chain redemption yet. Device installation, Android photo selection, and the full Phantom round trip still need a recorded end-to-end device check.

## Branding and support

`assets/logo.png` supplies the app logo and Android launcher icon. After replacing it, rerun CMake and rebuild. Public support contacts can be set in `app_config.json`; empty fields disable their contact buttons. See [branding instructions](assets/README.md).

## iOS

Building iOS requires macOS, Xcode, a Qt iOS kit, and signing configuration. The shared source and callback URL template are present, but there is no verified iOS build or IPA.
