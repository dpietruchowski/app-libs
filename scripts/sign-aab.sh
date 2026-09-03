#!/usr/bin/env bash
set -e

. "$(dirname "$0")/common.sh"

echo "📦 Signing AAB using jarsigner..."
echo "📌 Version: $VERSION_NAME"

KEYSTORE="/home/user/project/${ANDROID_KEYSTORE}"
UNSIGNED_AAB="/home/user/build/src/android-build/build/outputs/bundle/release/android-build-release.aab"
SIGNED_NAME="${APP_ID}-${VERSION_NAME}-release-signed.aab"
SIGNED_AAB="/home/user/build/${SIGNED_NAME}"

read -s -p "Enter keystore password: " KEYSTORE_PASS
echo ""
if [ -z "$KEYSTORE_PASS" ]; then
    echo "❌ Password cannot be empty."
    exit 1
fi

ensure_android_image

echo "🔑 Signing AAB..."
android_run -e KEYSTORE_PASS="$KEYSTORE_PASS" "$ANDROID_IMAGE_TAG" \
    sh -c 'JARSIGNER=$(dirname $(readlink -f $(command -v java)))/jarsigner && \
           ${JARSIGNER} \
               -keystore '"${KEYSTORE}"' \
               -storepass:env KEYSTORE_PASS \
               -sigalg SHA256withRSA -digestalg SHA-256 \
               -signedjar '"${SIGNED_AAB}"' \
               '"${UNSIGNED_AAB}"' \
               '"${ANDROID_KEY_ALIAS}"

echo "✅ AAB signed: ${ANDROID_BUILD_DIR}/${SIGNED_NAME}"

echo "🔍 Verifying signature..."
if android_run "$ANDROID_IMAGE_TAG" \
    sh -c "JARSIGNER=\$(dirname \$(readlink -f \$(command -v java)))/jarsigner && \
           \${JARSIGNER} -verify -verbose -certs ${SIGNED_AAB}" > /dev/null; then
    echo '✅ Signature verification succeeded!'
else
    echo '❌ Signature verification FAILED!'
    exit 1
fi
