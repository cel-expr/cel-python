#!/bin/bash
set -e

# Version of pycel to build.
# IMPORTANT: Update this to the latest version before building.
VERSION="0.0.1"


# Find the installation directory of cibuildwheel
CIBWHEEL_DIR=$(pip show cibuildwheel | grep Location: | awk '{print $2}')
# Derive the cibuildwheel binary path from the site-packages directory.
# Example: "/.../lib/python3.11/site-packages" -> "/.../bin/cibuildwheel"
CIBWHEEL_BIN=$(echo "${CIBWHEEL_DIR}" | sed 's/\/lib\/python[0-9.]*\/site-packages/\/bin/')/cibuildwheel

SRC_DIR=$(pwd)
echo "PyCEL source directory: ${SRC_DIR}"

TMP_DIR=$(mktemp -d)
echo "Build directory: ${TMP_DIR}"

pushd "${TMP_DIR}"

cp -r "${SRC_DIR}"/{*,.*} .
cp "${SRC_DIR}"/release/* .

# Substitute $VERSION in pyproject.toml with the value of VRS.
sed -i "s/\$VERSION/${VERSION}/g" pyproject.toml

echo "Running cibuildwheel: ${CIBWHEEL_BIN}"
"${CIBWHEEL_BIN}"

echo "Copying generated wheels to ${SRC_DIR}/wheelhouse"
mkdir -p "${SRC_DIR}"/wheelhouse
cp wheelhouse/py_cel-*.whl "${SRC_DIR}"/wheelhouse/

echo "Cleaning up build directory: ${TMP_DIR}"
rm -rf "${TMP_DIR}"

popd

echo "Successfully built py_cel wheels"
ls -l "${SRC_DIR}"/wheelhouse
