#!/usr/bin/env bash
set -ev

cd pykeyvi/
ln -s ../keyvi keyvi
cd ..

coveralls   -r . -b build/ -i keyvi --gcov /usr/bin/gcov-4.8 --gcov-options '\-lp' \
            -e build/keyvi/3rdparty -e keyvi/3rdparty \
            -E '.*/tests/*.cpp' \
            -E '.*/src/cpp/keyvicompiler/keyvicompiler.cpp' \
            -E '.*/src/cpp/keyviinspector/keyviinspector.cpp' \
            -E '.*/src/cpp/keyvimerger/keyvimerger.cpp' \
            --dump keyvi.cov_report > /dev/null

# workaround for coverage measurement: symlink cpp source:
#cd pykeyvi/src
#ln -s ../../keyvi/src/cpp/ .
#cd ../..
#
#coveralls   -r pykeyvi -b pykeyvi --gcov /usr/bin/gcov-4.8 --gcov-options '\-lp \-s '"$PWD"'/pykeyvi/keyvi' \
#            -E '.*3rdparty' -E '.*/pykeyvi.cpp' -E '.*autowrap.*' \
#            --dump pykeyvi.cov_report --follow-symlinks > /dev/null

export COVERALLS_REPO_TOKEN=${COVERALLS_REPO_TOKEN}

coveralls-merge  keyvi.cov_report
