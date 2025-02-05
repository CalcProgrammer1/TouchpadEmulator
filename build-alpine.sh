#!/usr/bin/env bash
VERSION_PATTERN="__VERSION__"
INFILE_PATH=APKBUILD.in

PACKAGE_VERSION=0.1."$(git rev-list --count HEAD)"_git"$(git show -s --format=%cd --date=format:'%Y%m%d')"
echo $PACKAGE_VERSION

sed -e "s/${VERSION_PATTERN}/${PACKAGE_VERSION}/g" ${INFILE_PATH} > APKBUILD

abuild

rm APKBUILD
