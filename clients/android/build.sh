#!/usr/bin/env bash
# Build the Elf Launcher Android app. Bundles the current page as the offline fallback.
# Needs keystore/keystore.properties (not committed) for a signed release APK.
set -euo pipefail
cd "$(dirname "$0")"
ver=$(sed -n 's/.*versionName = "\(.*\)"/\1/p' app/build.gradle.kts)
mkdir -p app/src/main/assets/web dist
cp ../../index.html app/src/main/assets/web/index.html
[ -f local.properties ] || echo "sdk.dir=${ANDROID_SDK_ROOT:-/workspace/android-sdk}" > local.properties
./gradlew -q assembleRelease
cp app/build/outputs/apk/release/app-release.apk "dist/ElfLauncher-$ver.apk"
ls -la "dist/ElfLauncher-$ver.apk"; sha256sum "dist/ElfLauncher-$ver.apk"
