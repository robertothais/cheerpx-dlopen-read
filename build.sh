#!/bin/bash
set -e

cd "$(dirname "$0")"

IMAGE="cheerpx-dlopen-read"
EXT2="out.ext2"
CONTAINER="dlopen-read-export"

echo "[*] Building Docker image..."
docker build --platform linux/386 -t "$IMAGE" .

echo "[*] Creating container..."
docker rm -f "$CONTAINER" 2>/dev/null || true
docker create --platform linux/386 --name "$CONTAINER" "$IMAGE"

echo "[*] Exporting to ext2..."
docker export "$CONTAINER" | docker run --rm -i --privileged \
	-v "$(pwd)":/output \
	alpine:latest /bin/sh -c '
        set -e
        apk add --no-cache e2fsprogs
        mkdir -p /fs && cd /fs && tar -xf -
        mkfs.ext2 -F -b 4096 -d /fs /output/'"$EXT2"' 256M
    '

docker rm "$CONTAINER" >/dev/null
echo "[+] Created $EXT2"
