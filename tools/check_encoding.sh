#!/bin/sh
# Every tracked file of this project must be valid UTF-8.
#
# This is not housekeeping. A file in another encoding breaks grep silently: the tool
# stops at the first invalid byte and reports nothing, so a search for a definition that
# is right there comes back empty. It cost two wrong conclusions in one day.
#
# The vendor libraries under Libraries/ are left alone: they are third-party files and
# their copyright signs are their own business.
set -eu

cd "$(dirname "$0")/.."
# Files git knows to be binary are skipped: .gitattributes says which.
bad=$(git ls-files | grep -v 'Libraries/' | while read -r f; do
    git check-attr binary -- "$f" | grep -q 'binary: set' && continue
    python3 - "$f" <<'PY' || echo "$f"
import io, sys
try:
    io.open(sys.argv[1], encoding='utf-8').read()
except (UnicodeDecodeError, IsADirectoryError):
    sys.exit(1)
PY
done)

if [ -n "$bad" ]; then
    echo "not valid UTF-8:" >&2
    echo "$bad" >&2
    exit 1
fi
echo "all tracked sources are valid UTF-8"
