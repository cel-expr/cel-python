#!/bin/bash
set -e

# Resolve the absolute path to the repository root (two levels up from release/kokoro)
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${REPO_ROOT}"

# 1. Process Arguments
PYTHON_VERSION=$1
if [ -z "${PYTHON_VERSION}" ]; then
  PYTHON_VERSION="3.11"
fi

echo "=== Configured Python Version: ${PYTHON_VERSION} ==="

BUILD_STATUS=0

# 2. Backup MODULE.bazel
echo "--- Backing up MODULE.bazel ---"
cp MODULE.bazel MODULE.bazel.bak

# Ensure restore happens on script exit
trap 'if [ -f MODULE.bazel.bak ]; then echo "--- Restoring MODULE.bazel ---"; mv MODULE.bazel.bak MODULE.bazel; fi' EXIT

# 3. Adjust Python version in MODULE.bazel dynamically
echo "--- Dynamically Adjusting Python Version in MODULE.bazel ---"
# Use sed to replace the version compatibility across macOS and Linux
sed -i "" "s/python_version = \"3.11\"/python_version = \"${PYTHON_VERSION}\"/g" MODULE.bazel || sed -i "s/python_version = \"3.11\"/python_version = \"${PYTHON_VERSION}\"/g" MODULE.bazel

# 4. Fetch dependencies with retries (for BCR network stability)
echo "--- Fetching Dependencies ---"
FETCH_RETRIES=5
FETCH_RETRY_DELAY_S=10
ATTEMPTS=0

while [ ${ATTEMPTS} -lt ${FETCH_RETRIES} ]; do
  ATTEMPTS=$((ATTEMPTS + 1))
  echo "Fetch attempt ${ATTEMPTS} of ${FETCH_RETRIES}..."

  if bazel fetch //... > fetch.log 2>&1; then
    echo "Fetch succeeded!"
    rm -f fetch.log
    break
  else
    if grep -iq "timeout\|timed" fetch.log; then
      echo "Fetch failed with timeout. Retrying in ${FETCH_RETRY_DELAY_S} seconds..."
      sleep ${FETCH_RETRY_DELAY_S}
    else
      echo "Fetch failed with non-timeout error."
      cat fetch.log
      rm -f fetch.log
      exit 1
    fi
  fi
done

# 5. Bazel Build & Test (verify compilation)
echo "--- Bazel Build ---"
bazel build //...

echo "--- Bazel Test ---"
bazel test //...

echo "--- Build Success ---"