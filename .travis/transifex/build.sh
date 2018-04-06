#!/bin/bash -e

echo "+docker run -e TRANSIFEX_API_TOKEN=[REDACTED] -v $(pwd):/yuzu alpine /bin/sh -e /yuzu/.travis/transifex/docker.sh"
docker run -e TRANSIFEX_API_TOKEN="${TRANSIFEX_API_TOKEN}" -v "$(pwd)":/yuzu alpine /bin/sh -e /yuzu/.travis/transifex/docker.sh
