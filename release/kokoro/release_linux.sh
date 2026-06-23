#!/bin/bash
# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

if ! command -v pip3 &> /dev/null || ! command -v curl &> /dev/null || ! command -v docker &> /dev/null; then
  echo "Installing basic dependencies..."
  apt-get update && apt-get install -y python3-pip curl

  if ! command -v docker &> /dev/null; then
    echo "Installing docker CLI..."
    ARCH=$(uname -m)
    if [ "$ARCH" = "x86_64" ]; then
      DOCKER_ARCH="x86_64"
    elif [ "$ARCH" = "aarch64" ]; then
      DOCKER_ARCH="aarch64"
    else
      echo "Unsupported arch: $ARCH"
      exit 1
    fi
    curl -fsSL "https://download.docker.com/linux/static/stable/${DOCKER_ARCH}/docker-24.0.7.tgz" -o docker.tgz
    tar xzvf docker.tgz --strip-components=1 docker/docker
    mv docker /usr/local/bin/
    rm -f docker.tgz
  fi
fi

# Avoid virtualenv/pip trying to download/upgrade tools from PyPI on host
export VIRTUALENV_NO_DOWNLOAD=1
export PIP_DISABLE_PIP_VERSION_CHECK=1
export PIP_BREAK_SYSTEM_PACKAGES=1
export PIP_DEFAULT_TIMEOUT=60

if [ "$(uname -m)" = "aarch64" ]; then
  export CIBW_ARCHS="aarch64"
else
  export CIBW_ARCHS="x86_64"
fi

# Pass these environment variables to the cibuildwheel Docker container
export CIBW_ENVIRONMENT="VIRTUALENV_NO_DOWNLOAD=1 PIP_DISABLE_PIP_VERSION_CHECK=1 PIP_DEFAULT_TIMEOUT=120"
export CIBW_DEPENDENCY_VERSIONS="latest"
export CIBW_CONTAINER_ENGINE_EXTRA_ARGS="--network=host"

# If running locally (not on Kokoro), authenticate with gcloud.
if [ -z "${KOKORO_BUILD_ID}" ]; then
  if ! gcloud auth application-default print-access-token --quiet > /dev/null; then
      gcloud auth application-default login
  fi
fi

# We use --no-cache-dir to force pip to download packages fresh and bypass the local
# cache. In a sandboxed build environment, writing to the default cache directory
# (~/.cache/pip) can encounter permission/sandbox restrictions or lead to stale
# dependency resolution. Disabling the cache ensures a reliable, reproducible install.
pip install --no-cache-dir -U keyring keyrings.google-artifactregistry-auth twine
curl -fsSL https://github.com/pypa/cibuildwheel/archive/refs/tags/v4.1.0.tar.gz -o cibuildwheel-4.1.0.tar.gz
pip install --no-cache-dir cibuildwheel-4.1.0.tar.gz
rm -f cibuildwheel-4.1.0.tar.gz

# ==============================================================================
# FUTURE-PROOF RUNTIME PATCHING OF CIBUILDWHEEL
# ==============================================================================
# To run cibuildwheel on Google's sandboxed RBE/Kokoro infrastructure, we must:
#   1. Bypass RBE's stdout proxy buffering deadlock (requires 32KB padding).
#   2. Bypass RBE's stdin EOF deadlock during copy-in (requires 'docker cp'
#      since we use disable_host_mount: True in pyproject.toml).
#
# Since cibuildwheel is installed fresh from PyPI on every build (ensuring we get
# the latest security and feature updates), we apply these patches at runtime.
#
# Why this patching strategy is future-proof and safe:
#   - Strict Validation: The Python patcher strictly validates that all target
#     code blocks exist before applying replacements. If cibuildwheel's internal
#     code changes in a future release, the patcher will FAIL LOUDLY and exit the
#     build immediately (sys.exit(1)) rather than silently running a broken,
#     hanging build.
#   - Stable Boundaries: The copy_into patch uses a robust regular expression
#     anchored to class method boundaries (def copy_into -> def copy_out). These
#     are stable, long-standing internal APIs of cibuildwheel's OCIContainer.
#   - Core Protocol Stability: The buffering patches target the core protocol
#     used to communicate with the container's persistent bash shell. This
#     protocol is fundamental to cibuildwheel and highly unlikely to change.
# ==============================================================================
OCI_PATH=$(python3 -c "import cibuildwheel.oci_container; print(cibuildwheel.oci_container.__file__)")
echo "Patching cibuildwheel at $OCI_PATH..."

cat << 'EOF' > patch_oci.py
import sys
import re

path = sys.argv[1]
with open(path, 'r') as f:
    content = f.read()

# 1. Force a 32KB flush at the end of every command execution
target_write = 'printf "%04d%s\\n" $? {end_of_message}'
replacement_write = 'printf "%04d%s\\n%32768s\\n" $? {end_of_message} " "'
if target_write in content:
    content = content.replace(target_write, replacement_write)
    print("Patched write loop.")
else:
    print("ERROR: Could not find write loop target in oci_container.py! The cibuildwheel version might have changed.")
    sys.exit(1)

# 2. Read and discard the 32KB padding to keep the stream clean
target_read = """                # add the last line to output, without the footer
                output_io.write(line[0:footer_offset])
                output_io.flush()
                break"""

replacement_read = """                # add the last line to output, without the footer
                output_io.write(line[0:footer_offset])
                output_io.flush()
                # Read and discard the 32KB padding line to clear the stream!
                self.bash_stdout.readline()
                break"""

if target_read in content:
    content = content.replace(target_read, replacement_read)
    print("Patched read loop.")
else:
    print("ERROR: Could not find read loop target in oci_container.py! The cibuildwheel version might have changed.")
    sys.exit(1)

# 3. Patch the entire copy_into method using a unique regex to use native 'docker cp'.
# This bypasses the RBE stdin EOF deadlock when copying the project into the container.
pattern = re.compile(r'    def copy_into\(self,.*?\).*?:.*?    def copy_out', re.DOTALL)

replacement_copy = """    def copy_into(self, from_path: Path, to_path: PurePath) -> None:
        if from_path.is_dir():
            self.call(["mkdir", "-p", to_path])
            subprocess.run(
                f"tar -c {self.host_tar_format} -f - . | {self.engine.name} exec -i {self.name} tar --no-same-owner -xC {shell_quote(to_path)} -f -",
                shell=True,
                check=True,
                cwd=from_path,
            )
        else:
            self.call(["mkdir", "-p", to_path.parent])
            # Use native docker cp to copy the file, avoiding stdin EOF deadlocks in RBE
            subprocess.run(
                [
                    self.engine.name,
                    "cp",
                    str(from_path),
                    f"{self.name}:{to_path}",
                ],
                check=True,
            )

    def copy_out"""

if pattern.search(content):
    content = pattern.sub(replacement_copy, content)
    print("Patched copy_into method using unique regex.")
else:
    print("ERROR: Could not find copy_into method boundary in oci_container.py! The cibuildwheel version might have changed.")
    sys.exit(1)

with open(path, 'w') as f:
    f.write(content)

print("Successfully patched oci_container.py!")
EOF

python3 patch_oci.py "$OCI_PATH"
rm patch_oci.py

# Verify that the patched file is syntactically valid Python
echo "Verifying patched oci_container.py syntax..."
python3 -m py_compile "$OCI_PATH" || { echo "ERROR: Patched oci_container.py is corrupted!"; exit 1; }

REPO_DIR=""
TMP_DIR=""
cleanup() {
  echo "Cleaning up temporary directories..."
  [ -n "${REPO_DIR}" ] && rm -rf "${REPO_DIR}"
  [ -n "${TMP_DIR}" ] && rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

REPO_DIR=$(mktemp -d)
echo "Created temporary directory: ${REPO_DIR}"

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

# Create the build directory inside the workspace volume (SRC_DIR)
# instead of the ephemeral /tmp, so that the sibling container can
# access it natively via volume propagation
TMP_DIR="${SRC_DIR}/build_area"
mkdir -p "${TMP_DIR}"
echo "Build directory: ${TMP_DIR}"
export TMPDIR="${TMP_DIR}/tmp"
mkdir -p "${TMPDIR}"

pushd "${TMP_DIR}"

cp -r "${SRC_DIR}"/{*,.*} . 2>/dev/null || true
cp -r "${SRC_DIR}"/release/* . 2>/dev/null || true
rm -rf cel_expr_python/*_test.py

echo "Downloading bazelisk on host..."
curl -LO https://github.com/bazelbuild/bazelisk/releases/download/v1.19.0/bazelisk-linux-amd64
curl -LO https://github.com/bazelbuild/bazelisk/releases/download/v1.19.0/bazelisk-linux-arm64
chmod +x bazelisk-linux-amd64 bazelisk-linux-arm64

echo "Debugging network..."
curl -I https://pypi.org/simple/setuptools/ || echo "curl failed"
python3 -c "import urllib.request; print('urllib status:', urllib.request.urlopen('https://pypi.org/simple/setuptools/', timeout=10).status)" || echo "urllib failed"

echo "Downloading build dependencies on host..."
mkdir -p build_deps
pip download --no-cache-dir --only-binary=:all: --dest build_deps "setuptools>=40.8.0" "wheel"
if [ "$(uname -m)" = "aarch64" ]; then
  PLATFORM_SUFFIX="aarch64"
else
  PLATFORM_SUFFIX="x86_64"
fi
pip download --no-cache-dir --only-binary=:all: --dest build_deps --python-version 3.9 --platform "manylinux2014_${PLATFORM_SUFFIX}" "virtualenv" "typing-extensions>=4.13.2"

if [ -f pyproject.toml ]; then
  sed -i "s/\$VERSION/${VERSION}/g" pyproject.toml
fi

echo "Running cibuildwheel..."
# Default CIBWHEEL_BIN if not set
if [ -z "${CIBWHEEL_BIN}" ]; then
  CIBWHEEL_BIN="python3 -m cibuildwheel"
fi
${CIBWHEEL_BIN} --platform linux --output-dir dist

if [ "${DRY_RUN}" = "true" ]; then
  echo "[DRY RUN] Skipping upload to PyPI exit gate."
else
  echo "Uploading to OSS Exit Gate for autopush to PyPI..."
  python3 -m twine upload --repository-url https://us-python.pkg.dev/oss-exit-gate-prod/cel-expr-python--pypi dist/*
fi

popd

echo "cel-expr-python ${VERSION} built and uploaded for release by OSS Exit Gate."
