#!/bin/bash

echo "🧹 Starting clean-up..."

# 1. Remove the builder image (Stage 2 & 3)
# This forces Docker to re-evaluate the build from Stage 2 onwards
docker rmi flycast-prism-builder:latest --force 2>/dev/null

# 2. Prune build cache BUT keep the labeled stage
# This clears disk space used by failed builds or old layers
echo "🗑️  Pruning build cache..."
docker builder prune --filter "label!=stage=immutable" -f

# 3. Optional: Prune dangling images
docker image prune -f

echo "✨ Clean-up complete. Your Stage 1 (Dependencies) is preserved."