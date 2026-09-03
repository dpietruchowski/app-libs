#!/usr/bin/env bash
set -e

. "$(dirname "$0")/common.sh"

echo "📦 Signing APK using apksigner..."
echo "📌 Version: $VERSION_NAME"

KEYSTORE="/home/user/project/${ANDROID_KEYSTORE}"
UNSIGNED_APK="/home/user/build/src/android-build/build/outputs/apk/release/android-build-release-unsigned.apk"
SIGNED_NAME="${APP_ID}-${VERSION_NAME}-release-signed.apk"
SIGNED_APK="/home/user/build/${SIGNED_NAME}"

APKSIGNER_PATH="/opt/android-sdk/build-tools/${ANDROID_SDK_BUILD_TOOLS}/apksigner"

read -s -p "Enter keystore password: " KEYSTORE_PASS
echo ""
if [ -z "$KEYSTORE_PASS" ]; then
    echo "❌ Password cannot be empty."
    exit 1
fi

ensure_android_image

echo "🔑 Signing APK..."
android_run -e KEYSTORE_PASS="$KEYSTORE_PASS" "$ANDROID_IMAGE_TAG" \
    sh -c 'cd /home/user/build && \
           '"${APKSIGNER_PATH}"' sign \
               --ks '"${KEYSTORE}"' \
               --ks-key-alias '"${ANDROID_KEY_ALIAS}"' \
               --ks-pass env:KEYSTORE_PASS \
               --out '"${SIGNED_APK}"' \
               '"${UNSIGNED_APK}"

echo "✅ APK signed: ${ANDROID_BUILD_DIR}/${SIGNED_NAME}"

echo "🔍 Verifying signature..."
if android_run "$ANDROID_IMAGE_TAG" \
    sh -c "${APKSIGNER_PATH} verify --verbose ${SIGNED_APK}"; then
    echo '✅ Signature verification succeeded!'
else
    echo '❌ Signature verification FAILED!'
    exit 1
fi
