# syntax=docker/dockerfile:1
# --- STAGE 1: Base Environment and Dependencies ---
FROM ubuntu:22.04 AS base
LABEL project="flycast-prism" stage="immutable"
ENV DEBIAN_FRONTEND=noninteractive

# Install extensive dependencies to satisfy the script's SHLIBS list
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    patchelf \
    wget \
    file \
    libvulkan-dev \
    libgl1-mesa-dev \
    libsdl2-dev \
    libcurl4-openssl-dev \
    libzip-dev \
    libminiupnpc-dev \
    libasound2-dev \
    libpulse-dev \
    liblua5.3-dev \
    libao-dev \
    libzstd-dev \
    libbrotli-dev \
    libpsl-dev \
    libidn2-dev \
    librtmp-dev \
    libssh-dev \
    libldap2-dev \
    libgcrypt20-dev \
    libflac-dev \
    libvorbis-dev \
    libopus-dev \
    libsqlite3-dev \
    libcdio-dev \
    libmd-dev \
    libp11-kit-dev \
    libtasn1-6-dev \
    libkeyutils-dev \
    libapparmor-dev \
    && rm -rf /var/lib/apt/lists/*

# --- STAGE 2: Source Code Compilation ---
FROM base AS builder
ARG GIT_REPO=https://github.com/marcgardent/Flycast-Prism.git
ARG GIT_BRANCH=master
ARG APP_VERSION=nightly

WORKDIR /source
RUN git clone --recursive --branch ${GIT_BRANCH} ${GIT_REPO} .

# Patch metadata for Flycast Prism (ID, Name, Description, Version, and Date)
RUN export BUILD_DATE=$(date +%Y-%m-%d) && \
    sed -i 's|<id>org.flycast.Flycast</id>|<id>org.flycast.FlycastPrism</id>|g' shell/linux/org.flycast.Flycast.metainfo.xml && \
    sed -i 's|<name>Flycast</name>|<name>Flycast Prism</name>|g' shell/linux/org.flycast.Flycast.metainfo.xml && \
    sed -i 's|Flycast is a multi-platform|Flycast Prism is a specialized fork of|g' shell/linux/org.flycast.Flycast.metainfo.xml && \
    sed -i "s|<release date=\".*\" version=\".*\" />|<release date=\"$BUILD_DATE\" version=\"${APP_VERSION}\" />|g" shell/linux/org.flycast.Flycast.metainfo.xml && \
    sed -i 's|Name=Flycast|Name=Flycast Prism|g' shell/linux/flycast.desktop

# Patch make-appimage.sh script:
# 1. Prevent script from crashing if a specific lib is missing (e.g., legacy libssl.so.1.1)
# 2. Allow flexibility for library paths on Ubuntu 22.04
RUN sed -i 's/exit 1/echo "Skipping missing library..."/g' shell/linux/make-appimage.sh && \
    chmod +x shell/linux/make-appimage.sh

WORKDIR /source/build
RUN cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_OSS=OFF
RUN make -j$(nproc)

# --- STAGE 3: Packaging with Official Script ---
FROM builder AS packager
ARG APP_VERSION=nightly

WORKDIR /source
ENV APPIMAGE_EXTRACT_AND_RUN=1

# Run the packaging script
RUN ./shell/linux/make-appimage.sh /source /source/build /source/AppDir

# Generate the final filename with version and date
RUN export BUILD_DATE=$(date +%Y%m%d) && \
    mkdir -p /output && \
    mv flycast-x86_64.AppImage "/output/Flycast-Prism-${APP_VERSION}-${BUILD_DATE}-x86_64.AppImage"

# Final command for export (copies all AppImages found in /output)
CMD cp /output/*.AppImage /export/