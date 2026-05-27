#!/bin/bash
set -e

# If running locally (not on Kokoro), authenticate with gcloud.
if [ -z "${KOKORO_BUILD_ID}" ]; then
  if ! gcloud auth application-default print-access-token --quiet > /dev/null; then
      gcloud auth application-default login
  fi
fi

pip install -U keyring keyrings.google-artifactregistry-auth twine cibuildwheel

echo "Installing CPython Mac Frameworks..."
for pyver in "3.11.9" "3.12.4" "3.13.0" "3.14.3"; do
  echo "Downloading and installing Python ${pyver}..."
  curl -LO "https://www.python.org/ftp/python/${pyver}/python-${pyver}-macos11.pkg"
  sudo installer -pkg "python-${pyver}-macos11.pkg" -target /
  rm "python-${pyver}-macos11.pkg"
done

REPO_DIR=$(mktemp -d)
echo "Created temporary directory: ${REPO_DIR}"

# Ensure the temporary directory is removed on script exit
trap 'echo "Cleaning up temporary directory: ${REPO_DIR}"; rm -rf "${REPO_DIR}"' EXIT

if [ "${DRY_RUN}" = "true" ]; then
  echo "[DRY RUN] Using local Kokoro clone instead of cloning main."
  SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
  pushd "${SRC_DIR}"
  # Get the latest tag or fallback
  VERSION=$(git tag --sort=-v:refname 2>/dev/null | head -n 1 || true)
  if [ -z "${VERSION}" ]; then
    VERSION="0.1.2"
  fi
  popd
else
  pushd "${REPO_DIR}"
  git clone https://github.com/cel-expr/cel-python.git
  cd cel-python
  # Get the latest version tag
  VERSION=$(git tag --sort=-v:refname | head -n 1)
  SRC_DIR="${REPO_DIR}/cel-python"
  popd
fi

# Strip initial "v" if present
VERSION=${VERSION#v}
echo "Building release for version: ${VERSION}"

TMP_DIR=$(mktemp -d)
echo "Build directory: ${TMP_DIR}"

# Add trap cleanup for TMP_DIR as well
trap 'echo "Cleaning up temporary directories: ${REPO_DIR} ${TMP_DIR}"; rm -rf "${REPO_DIR}" "${TMP_DIR}"' EXIT

pushd "${TMP_DIR}"

cp -r "${SRC_DIR}"/{*,.*} . 2>/dev/null || true
cp "${SRC_DIR}"/release/* . 2>/dev/null || true
rm -rf cel_expr_python/*_test.py

# Check if pyproject.toml exists before running sed
if [ -f pyproject.toml ]; then
  sed -i "" "s/\$VERSION/${VERSION}/g" pyproject.toml || sed -i "s/\$VERSION/${VERSION}/g" pyproject.toml
fi

echo "Running cibuildwheel: ${CIBWHEEL_BIN}"
# Default CIBWHEEL_BIN if not set
if [ -z "${CIBWHEEL_BIN}" ]; then
  CIBWHEEL_BIN="python3 -m cibuildwheel"
fi
${CIBWHEEL_BIN} --platform macos --output-dir dist

if [ "${DRY_RUN}" = "true" ]; then
  echo "[DRY RUN] Skipping upload to PyPI exit gate."
else
  echo "Uploading to OSS Exit Gate for autopush to PyPI..."
  python3 -m twine upload --repository-url https://us-python.pkg.dev/oss-exit-gate-prod/cel-expr-python--pypi dist/*
fi

popd

echo "cel-expr-python ${VERSION} built and uploaded for release by OSS Exit Gate."