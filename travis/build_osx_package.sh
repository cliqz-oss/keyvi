#!/usr/bin/env bash
set -ev

echo $PATH
python --version

cd keyvi
scons -j 4 mode=release
cd ..

# use python from pyenv
PATH="$PYENV_ROOT/bin:$PYENV_ROOT/shims:$PATH"

cd pykeyvi
python setup.py bdist_wheel -d wheelhouse
sudo -H pip install wheelhouse/*.whl
py.test tests
cd ..
