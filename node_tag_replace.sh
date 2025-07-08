#!/usr/bin/env bash
#
# rename_tag.sh  –  safe, project-wide search-and-replace for node tags
#
# usage:  ./rename_tag.sh OLD_TAG NEW_TAG
#

set -euo pipefail

########## 1. sanity checks ####################################################
if [[ $# -ne 2 ]]; then
    printf 'usage: %s OLD_TAG NEW_TAG\n' "${0##*/}" >&2
    exit 1
fi

old=$1
new=$2
#!/bin/bash

# Protect against trivial foot-guns
if [[ $old == "$new" ]]; then
    echo "Old tag and new tag are identical; nothing to do." >&2
    exit 0
fi

# Warn if working tree is dirty (git users)
if command -v git >/dev/null && git rev-parse --is-inside-work-tree &>/dev/null; then
    if ! git diff --quiet --ignore-submodules --exit-code; then
        echo "⚠️  Git workspace has uncommitted changes." >&2
        read -p "Proceed anyway? [y/N] " yn
        [[ $yn =~ ^[Yy]$ ]] || exit 1
    fi
fi

########## 2. pick the files to touch ##########################################
# Edit only text-like project files; avoid .git, build output, binaries, etc.
# Adjust the find(1) expression to match your layout if needed.
mapfile -t files < <(
  grep -RIl --exclude-dir={.git,build,obj} -e "$old" .
)

if (( ${#files[@]} == 0 )); then
    echo "No occurrences of '$old' found." >&2
    exit 0
fi

########## 3. replace in-place with backup #####################################
for f in "${files[@]}"; do
    # Make a .bak copy (so you can diff/undo easily)
    cp -- "$f" "$f.bak"

    # Word-boundary replacement; change to plain s///g if you sometimes embed tags
    sed -i "s/\b${old}\b/${new}/g" -- "$f"
done

########## 4. report ###########################################################
echo "Replaced '$old' → '$new' in ${#files[@]} files."
echo "Backups retained with .bak suffix."
echo "Review the changes (git diff / meld / diff -u *.bak) and then remove .bak files when satisfied."
