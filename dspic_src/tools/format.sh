#!/bin/sh
# Formats every C source and header in the project to the house style:
# clang-format (.clang-format at the project root), then tools/style.py.
# Run from anywhere; needs clang-format 16+ and python3.
set -e
cd "$(dirname "$0")/.."
FILES=$(find . -name '*.[ch]' -not -path './build/*' -not -path './dist/*' -not -path './nbproject/*' \
        -not -name '*_host.c' | sort)
for f in $FILES; do
    # clang-format works on LF text; restore CRLF where the file had it.
    if grep -q "$(printf '\r')" "$f"; then
        tr -d '\r' < "$f" | clang-format --style=file --assume-filename="$f" | sed 's/$/\r/' > "$f.tmp"
    else
        clang-format --style=file --assume-filename="$f" < "$f" > "$f.tmp"
    fi
    mv "$f.tmp" "$f"
done
python3 tools/style.py $FILES
