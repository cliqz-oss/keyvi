#!/usr/bin/env bash
set -ev

cd keyvi
scons -j 4 mode=release
release/dictionaryfsa_unittests/dictionaryfsa_unittests
cd ..

# use python from pyenv
PYENV_ROOT="$HOME/.pyenv"
PATH="$PYENV_ROOT/bin:$PYENV_ROOT/shims:$PATH"

export TMPDIR=/Volumes/ram-disk

cd pykeyvi
python setup.py bdist_wheel -d wheelhouse
sudo -H pip install wheelhouse/*.whl
py.test tests
cd ..
