#!/usr/bin/env bash

LIBS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_DIR="$(cd "$LIBS_DIR/.." && pwd)"
APP_ENV="$PROJECT_DIR/app.env"

if [ ! -f "$APP_ENV" ]; then
    echo "Missing $APP_ENV (APP_NAME, APP_ID, APP_TARGET, VERSION_NAME, VERSION_CODE, ...)"
    exit 1
fi

set -a
. "$APP_ENV"
set +a

for required in APP_NAME APP_ID APP_TARGET VERSION_NAME VERSION_CODE; do
    if [ -z "${!required}" ]; then
        echo "app.env: $required is not set"
        exit 1
    fi
done

ANDROID_BUILD_DIR="${ANDROID_BUILD_DIR:-build-android}"
ANDROID_IMAGE_TAG="${ANDROID_IMAGE_TAG:-app-libs-qt6-android:6.10-api36}"
ANDROID_SDK_BUILD_TOOLS="${ANDROID_SDK_BUILD_TOOLS:-36.0.0}"

ensure_android_image() {
    docker build -t "$ANDROID_IMAGE_TAG" - < "$LIBS_DIR/docker/Dockerfile.android"
}

android_run() {
    docker run --rm \
        -v "${PROJECT_DIR}:/home/user/project:ro" \
        -v "${PROJECT_DIR}/${ANDROID_BUILD_DIR}:/home/user/build" \
        "$@"
}

cd "$PROJECT_DIR"
