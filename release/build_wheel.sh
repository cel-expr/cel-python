#!/bin/bash
# To build the cel_expr_python module locally:
#
# - Install `cibuildwheel`
#
#   ```
#   pip install cibuildwheel
#   ```
#
# - [Optional] Use `podman`
#
#   If you are using podman instead of docker, run
#
#   ```
#   export CIBW_CONTAINER_ENGINE="podman"
#   ```
#
# - Navigate to the source directory
#
#   Go to the parent directory of the one containing this file, e.g.
#
#   ```
#   cd cel-git-repo
#   ```
#
# - Update release version
#   Edit `release/build_wheel.sh`: update `VERSION`
#
# - Run `release/build_wheel.sh`
#
#   ```
#   release/build_wheel.sh
#   ```
#
#   Optionally, provide the signature of a specific target configuration, e.g.
#
#   ```
#   release/build_wheel.sh --only "cp311-manylinux_x86_64"
#   ```
#
# - Install the resulting wheel:
#
#   ```
#   pip install --force-reinstall wheelhouse/py_cel-*.whl
#   ```
#
# - Verify that the wheel is working correctly
#
#   ```
#   python release/py_cel_basic_test.py
#   ```

set -e

# Version of cel-expr-python to build.
# IMPORTANT: Update this to the latest version before building.
VERSION="0.0.1"

SRC_DIR=$(pwd)
echo "cel-expr-python source directory: ${SRC_DIR}"

TMP_DIR=$(mktemp -d ../cel-python-build-XXXXXX)
echo "Build directory: ${TMP_DIR}"

pushd "${TMP_DIR}"

if [[ "$OSTYPE" == "darwin"* ]]; then
  cp -a "${SRC_DIR}"/. .
else
  cp -r "${SRC_DIR}/." .
fi
cp release/* .
rm -rf cel_expr_python/*_test.py

# Substitute $VERSION in pyproject.toml with the value of VERSION.
if [[ "$OSTYPE" == "darwin"* ]]; then
  sed -i '' "s/\$VERSION/${VERSION}/g" pyproject.toml
else
  python -c "import sys; content = open('pyproject.toml').read(); open('pyproject.toml', 'w').write(content.replace('\$VERSION', sys.argv[1]))" "${VERSION}"
fi

echo "Running cibuildwheel: ${CIBWHEEL_BIN:-python -m cibuildwheel}"
${CIBWHEEL_BIN:-PYTHON_BIN="python"
if command -v python3 &> /dev/null; then
  PYTHON_BIN="python3"
fi

"$PYTHON_BIN" -m cibuildwheel} "$@"

echo "Copying generated wheels to ${SRC_DIR}/wheelhouse"
mkdir -p "${SRC_DIR}"/wheelhouse
cp wheelhouse/cel_expr_python-*.whl "${SRC_DIR}"/wheelhouse/

echo "Cleaning up build directory: ${TMP_DIR}"
rm -rf "${TMP_DIR}"

popd

echo "Successfully built cel-expr-python wheels"
ls -l "${SRC_DIR}"/wheelhouse

# Keep the window open if run in a separate terminal.
echo ""
read -p "Press enter to exit..."
