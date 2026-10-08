# UMIT

An Android ticketing prototype for event organizers and entry staff, built with C++ and Qt Quick.

## Problem

Event staff need to validate tickets and reject repeated entry. Multiple entrances need a shared record of which tickets have already been used.

## Solution

UMIT combines ticket issuance and entry checking in one app. It includes device-local accounts, editable profiles and avatars, Russian/Kazakh/English, light/dark themes, custom colors, and an app password.

**Current demo:** create a ticket and redeem its ID manually. The first redemption succeeds; a second is rejected within the same running app. Tickets are held in memory and reset when the process ends or the account session changes. Cross-device accounts, shared ticket state, and QR scanning are not implemented.

## How it uses Solana

UMIT connects to Phantom through native mobile deep links and reads the wallet's SOL balance on **Solana Devnet**. Wallet signing keys stay in Phantom.

**Nothing is written to Solana yet.** Ticket issuance and redemption are local. There is no deployed ticket program or transaction signing.

The planned Solana program will store event permissions and each ticket's event and used status. Authorized redemption will update this shared state atomically so separate entrances cannot redeem the same ticket twice. See [architecture](ARCHITECTURE.md) and [wallet integration](PHANTOM_DEVNET.md).

## How to run

1. Clone this repository into a path without non-ASCII characters, such as `C:\Dev\UMIT`.
2. Install Qt 6.12, Qt Creator, and the Android SDK/NDK/JDK required by that Qt kit.
3. Open the root `CMakeLists.txt`. Select an Android kit matching your phone; the Redmi A3 build uses **armeabi-v7a (armv7)**.
4. Set `TICKET_BUILD_MOBILE=ON`, `TICKET_BUILD_CLI=OFF`, and `BUILD_TESTING=OFF`.
5. Configure, build the APK, and install it, or run from Qt Creator on a device with USB debugging. **Run CMake alone does not build an APK.**
6. Register a local account, open Tickets, create a ticket, and redeem its ID twice. Install Phantom separately to test the wallet connection.

For the standalone core demo and tests, install CMake, Ninja, and a C++17 compiler, then run from the repository root:

```sh
cmake -S . -B build/core -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTICKET_BUILD_MOBILE=OFF -DTICKET_BUILD_CLI=ON -DBUILD_TESTING=ON
cmake --build build/core
ctest --test-dir build/core --output-on-failure
```

Run `build/core/ticket_demo` (`.exe` on Windows). This tests local rules, including concurrent redemption; it does not test a blockchain program. More build and desktop test instructions: [mobile/README.md](mobile/README.md).

Android is the current target. iOS has not been built or tested and requires macOS/Xcode.

## Team

**UMIT Edu** — [Zetz](https://github.com/Zetz798), project creator and developer.