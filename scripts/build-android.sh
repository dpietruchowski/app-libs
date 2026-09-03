#!/usr/bin/env bash
set -e

. "$(dirname "$0")/common.sh"

BUILD_TYPE=Release
CLEAR_BUILD=false
BUILD_TARGET=""
OFFICIAL=false

for arg in "$@"; do
    case "$arg" in
        clear)
            CLEAR_BUILD=true
            ;;
        debug)
            BUILD_TYPE=Debug
            ;;
        release)
            BUILD_TYPE=Release
            ;;
        aab)
            BUILD_TARGET="--target aab"
            ;;
        apk)
            BUILD_TARGET="--target apk"
            ;;
        official)
            OFFICIAL=true
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: $0 [clear] [debug|release] [apk|aab] [official]"
            exit 1
            ;;
    esac
done

if [ "$OFFICIAL" = true ]; then
    CODE=$VERSION_CODE
    NAME=$VERSION_NAME

    RELEASE_SUBJECT="Release ${NAME} (version code ${CODE})"

    if [ "$(git log -1 --pretty=%s)" = "$RELEASE_SUBJECT" ]; then
        if [ -n "$(git status --porcelain)" ]; then
            echo "Uncommitted changes on release commit ${NAME} (version code ${CODE}):"
            git status --short
            echo "Commit or stash them before building an official release."
            exit 1
        fi

        echo "Nothing changed since release ${NAME} (version code ${CODE}) — rebuilding the same version"
    else
        NEW_CODE=$((CODE + 1))
        BASE=${NAME%%-*}
        MAJOR=${BASE%%.*}
        PATCH=${BASE##*.}
        MINOR=${BASE#*.}
        MINOR=${MINOR%.*}
        NEW_NAME="${MAJOR}.${MINOR}.$((PATCH + 1))"

        sed -i "s/^VERSION_CODE=.*/VERSION_CODE=${NEW_CODE}/" "$APP_ENV"
        sed -i "s/^VERSION_NAME=.*/VERSION_NAME=${NEW_NAME}/" "$APP_ENV"

        git commit "$APP_ENV" -m "Release ${NEW_NAME} (version code ${NEW_CODE})"
        git tag "v${NEW_NAME}"

        echo "Official release ${NEW_NAME} (version code ${NEW_CODE}) — committed and tagged v${NEW_NAME}"
    fi
fi

if [ "$CLEAR_BUILD" = true ]; then
    rm -rf "$ANDROID_BUILD_DIR"
fi

mkdir -p "$ANDROID_BUILD_DIR"

ensure_android_image

android_run "$ANDROID_IMAGE_TAG" \
    sh -c "qt-cmake /home/user/project -G Ninja -B /home/user/build -DCMAKE_BUILD_TYPE=${BUILD_TYPE} && cmake --build /home/user/build --config ${BUILD_TYPE} ${BUILD_TARGET}"
