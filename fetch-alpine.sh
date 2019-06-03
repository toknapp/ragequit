#!/bin/bash

set -o nounset -o pipefail -o errexit

URL=http://dl-cdn.alpinelinux.org/alpine/v3.9/releases/x86_64/alpine-miniroot-3.9.4-x86_64.tar.gz
TARBALL=$(basename "$URL")

# https://alpinelinux.org/downloads/
PUB=https://alpinelinux.org/keys/ncopa.asc
PUB_KEYID=293ACD0907D9495A

for u in $URL $URL.sha256 $URL.asc; do
    if [ ! -f $(basename "$u") ]; then
        curl --output $(basename "$u") "$u"
    fi
done

sha256sum -c "$TARBALL.sha256" 1>&2

TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

curl -s "$PUB" | gpg --homedir=$TMP --import

gpg --homedir=$TMP --trusted-key=$PUB_KEYID \
    --verify "$TARBALL.asc" "$TARBALL"

echo "$TARBALL"
