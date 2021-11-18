#!/usr/bin/env bash

VERSION_TAG=$1

if [ -z "$VERSION_TAG" ]; then
    echo "Usage: $0 <version>"
    exit 1
fi

if ! git diff --exit-code; then
    echo "Please commit changes first"
    exit 1
fi

if ! git diff --cached --exit-code; then
    echo "Please commit changes first"
    exit 1
fi

if [ $(git rev-parse --abbrev-ref HEAD) != "main" ]; then
    echo "Can only release from 'main' branch"
    exit 1
fi

jq ".version = \"$VERSION_TAG\"" library.json | sponge library.json
git add library.json
git commit -m "Prepare release $VERSION_TAG"

git tag -a -f "$VERSION_TAG" -m "Release $VERSION_TAG"
git push -f origin "$VERSION_TAG" main
