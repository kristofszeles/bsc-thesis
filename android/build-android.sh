#!/usr/bin/env bash
# Fetch SDL2/SDL2_image/SDL2_net sources + Java shim into android/app/jni
# and android/app/src/main/java, then build the debug APK with Gradle.
# Mirrors .github/workflows/build-android-apk.yml.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JNI="$SCRIPT_DIR/app/jni"
JAVA_ORG="$SCRIPT_DIR/app/src/main/java"

clone_if_missing() {
    local branch="$1" url="$2" dest="$3"
    if [ -d "$dest/.git" ]; then
        echo "[skip] $dest already cloned"
    elif [ -d "$dest" ]; then
        echo "[warn] $dest exists but is not a git checkout; leaving it alone"
    else
        echo "[clone] $url ($branch) -> $dest"
        git clone --depth 1 --branch "$branch" "$url" "$dest"
    fi
}

mkdir -p "$JNI" "$JAVA_ORG"

clone_if_missing release-2.30.x https://github.com/libsdl-org/SDL.git       "$JNI/SDL"
clone_if_missing release-2.8.x  https://github.com/libsdl-org/SDL_image.git "$JNI/SDL_image"
clone_if_missing release-2.2.x  https://github.com/libsdl-org/SDL_net.git   "$JNI/SDL_net"

if [ ! -d "$JAVA_ORG/org" ]; then
    echo "[copy] SDL Java shim -> $JAVA_ORG/org"
    cp -a "$JNI/SDL/android-project/app/src/main/java/org" "$JAVA_ORG/"
else
    echo "[skip] $JAVA_ORG/org already present"
fi

BUILD_TARGET="${1:-assembleDebug}"
echo "[gradle] ./gradlew $BUILD_TARGET"
cd "$SCRIPT_DIR"
chmod +x ./gradlew
./gradlew --no-daemon "$BUILD_TARGET"
