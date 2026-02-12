To build the py_cel wheel locally for testing:

- Install `cibuildwheel`

  ```
  pip install cibuildwheel
  ```

- [Optional] Use `podman`

  If you are using podman instead of docker, run

  ```
  export CIBW_CONTAINER_ENGINE="podman"
  ```

- Navigate to the source directory

  Go to the parent directory of the one containing this file, e.g.

  ```
  cd cel-git-repo
  ```

- Update release version
  Edit `release/build_wheel.sh`: update `VERSION`

- Run `release/build_wheel.sh`

  ```
  release/build_wheel.sh
  ```

  Optionally, provide the signature of a specific target configuration, e.g.

  ```
  release/build_wheel.sh --only "cp311-manylinux_x86_64"
  ```

- Install the resulting wheel:

  ```
  pip install --force-reinstall wheelhouse/py_cel-*.whl
  ```

- Verify that the wheel is working correctly

  ```
  python release/py_cel_basic_test.py
  ```