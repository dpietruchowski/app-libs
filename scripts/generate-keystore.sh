#!/usr/bin/env bash
set -e

. "$(dirname "$0")/common.sh"

KEYSTORE_DIR="$(dirname "$ANDROID_KEYSTORE")"
KEYSTORE_FILE="$(basename "$ANDROID_KEYSTORE")"
KEY_VALIDITY_DAYS=10000
KEY_DNAME="${ANDROID_KEY_DNAME:-CN=${APP_NAME} App, OU=Development, O=${APP_NAME}, L=Warsaw, ST=Masovian, C=PL}"

echo "🔐 Generating new keystore for Android signing..."

if [ -f "$ANDROID_KEYSTORE" ]; then
    echo "⚠️  Keystore already exists at: $ANDROID_KEYSTORE"
    read -p "Do you want to overwrite it? (yes/no): " CONFIRM
    if [ "$CONFIRM" != "yes" ]; then
        echo "❌ Aborted."
        exit 1
    fi
    rm -f "$ANDROID_KEYSTORE"
fi

mkdir -p "$KEYSTORE_DIR"

while true; do
    read -s -p "Enter keystore password: " KEYSTORE_PASS
    echo ""
    if [ -z "$KEYSTORE_PASS" ]; then
        echo "❌ Password cannot be empty."
        continue
    fi
    read -s -p "Confirm keystore password: " KEYSTORE_PASS_CONFIRM
    echo ""
    if [ "$KEYSTORE_PASS" != "$KEYSTORE_PASS_CONFIRM" ]; then
        echo "❌ Passwords do not match. Try again."
        continue
    fi
    break
done

echo "📝 You will be prompted for keystore information..."
echo ""

ensure_android_image

docker run --rm -i \
    -e KEYSTORE_PASS="$KEYSTORE_PASS" \
    -v "${PROJECT_DIR}/${KEYSTORE_DIR}:/home/user/keystore" \
    "${ANDROID_IMAGE_TAG}" \
    sh -c 'keytool -genkeypair \
        -v \
        -keystore /home/user/keystore/'"${KEYSTORE_FILE}"' \
        -alias '"${ANDROID_KEY_ALIAS}"' \
        -keyalg RSA \
        -keysize 2048 \
        -validity '"${KEY_VALIDITY_DAYS}"' \
        -storepass "$KEYSTORE_PASS" \
        -keypass "$KEYSTORE_PASS" \
        -dname "'"${KEY_DNAME}"'"'

echo ""
echo "✅ Keystore generated successfully!"
echo "📁 Location: $ANDROID_KEYSTORE"
echo "🔑 Alias: $ANDROID_KEY_ALIAS"
echo ""
echo "⚠️  Keep this keystore file secure and never commit it to version control!"
