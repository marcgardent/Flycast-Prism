#!/bin/bash

# --- Configuration ---
IMAGE_NAME="flycast-prism-builder"
OUTPUT_DIR="$(pwd)/dist"
VERSION=${1:-"nightly-prism-r1-$(date +%d%m)"}
BRANCH=${2:-"pull-requests/discard-dummy-textures"}
REPO="https://github.com/marcgardent/Flycast-Prism.git"

# --- Cleanup and Preparation ---
echo "☕ Preparing the build environment..."
mkdir -p "$OUTPUT_DIR"
export DOCKER_BUILDKIT=1

# --- Step 1: Build ---
echo "🏗️  Building Docker image (Branch: $BRANCH, Version: $VERSION)..."
docker build \
  --build-arg APP_VERSION="$VERSION" \
  --build-arg GIT_BRANCH="$BRANCH" \
  --build-arg GIT_REPO="$REPO" \
  -t "$IMAGE_NAME" .

if [ $? -ne 0 ]; then
    echo "❌ Build failed."
    exit 1
fi

# --- Step 2: Extraction ---
echo "📦 Extracting AppImage to $OUTPUT_DIR..."
docker run --rm -v "$OUTPUT_DIR":/export "$IMAGE_NAME"

if [ $? -eq 0 ]; then
    echo "✅ Success! Your AppImage is ready in: $OUTPUT_DIR"
    ls -lh "$OUTPUT_DIR"
else
    echo "❌ Error during extraction."
fi