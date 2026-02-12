#!/bin/bash
set -e

# Version of cel-expr-python to build.
# IMPORTANT: Update this to the latest version before building.
VERSION="0.0.1"

SRC_DIR=$(pwd)
echo "cel-expr-python source directory: ${SRC_DIR}"

TMP_DIR=$(mktemp -d)
echo "Build directory: ${TMP_DIR}"

pushd "${TMP_DIR}"

cp -r "${SRC_DIR}"/{*,.*} .
cp "${SRC_DIR}"/release/* .
rm -rf cel_expr_python/*_test.py

# Substitute $VERSION in pyproject.toml with the value of VERSION.
sed -i "s/\$VERSION/${VERSION}/g" pyproject.toml

echo "Running cibuildwheel: ${CIBWHEEL_BIN}"
python -m cibuildwheel "$@"

echo "Copying generated wheels to ${SRC_DIR}/wheelhouse"
mkdir -p "${SRC_DIR}"/wheelhouse
cp wheelhouse/cel_expr_python-*.whl "${SRC_DIR}"/wheelhouse/

echo "Cleaning up build directory: ${TMP_DIR}"
rm -rf "${TMP_DIR}"

popd

echo "Successfully built cel-expr-python wheels"
ls -l "${SRC_DIR}"/wheelhouse
