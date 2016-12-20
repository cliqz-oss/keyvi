#!/bin/bash

set -ev

git clone https://github.com/yyuu/pyenv.git ~/.pyenv
PYENV_ROOT="$HOME/.pyenv"
PATH="$PYENV_ROOT/bin:$PATH"
eval "$(pyenv init -)"

case "${PYTHON_VERSION}" in
  27)
      curl -O https://bootstrap.pypa.io/get-pip.py
      python get-pip.py --user
      ;;
  33)
      pyenv install 3.3.6
      pyenv global 3.3.6
      ;;
  34)
      pyenv install 3.4.5
      pyenv global 3.4.5
      ;;
  35)
      pyenv install 3.5.2
      pyenv global 3.5.2
      ;;
  *)
      echo "PYTHON_VERSION not set"
esac

pyenv rehash
python --version