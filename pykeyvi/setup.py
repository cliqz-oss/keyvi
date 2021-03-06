from setuptools import setup, Extension
import distutils.command.build as _build
import distutils.command.build_ext as _build_ext
import distutils.command.sdist as _sdist
import os
import sys
import subprocess
import multiprocessing
import shutil
import glob

from contextlib import contextmanager
from os import path

pykeyvi_pyx = 'pykeyvi.pyx'
pykeyvi_cpp = 'pykeyvi.cpp'


def generate_pykeyvi_source():
    addons = glob.glob('src/addons/*')
    pxds = glob.glob('src/pxds/*')
    converters = 'src/converters'
    converter_files = glob.glob(path.join(converters, '*'))
    max_modification_time = max([path.getmtime(fn) for fn in addons + pxds + converter_files])

    if not path.exists(pykeyvi_cpp) or max_modification_time > path.getmtime(pykeyvi_cpp):
        import autowrap.Main
        autowrap.Main.run(pxds, addons, [converters], pykeyvi_pyx)


@contextmanager
def symlink_keyvi():
    if not path.exists('keyvi'):
        os.symlink('../keyvi', 'keyvi')
        keyvi_source_path = os.path.realpath(os.path.join(os.getcwd(), "../keyvi"))
        pykeyvi_source_path = os.path.join(os.getcwd(),"keyvi")
        yield (pykeyvi_source_path, keyvi_source_path)
        os.unlink('keyvi')
    else:
        yield None, None


with symlink_keyvi() as (pykeyvi_source_path, keyvi_source_path):
    # workaround for autowrap bug (includes incompatible boost)
    autowrap_data_dir = "autowrap_includes"

    dictionary_sources = path.abspath('keyvi')
    tpie_build_dir = path.join(dictionary_sources, '3rdparty/tpie/build')
    tpie_install_prefix = 'install'
    tpie_include_dir = path.join(tpie_build_dir, tpie_install_prefix, 'include')
    tpie_lib_dir = path.join(tpie_build_dir, tpie_install_prefix, 'lib')

    additional_compile_flags = []

    # workaround for https://bitbucket.org/pypy/pypy/issues/2626/invalid-conversion-from-const-char-to-char
    if os.environ.get('PYTHON_VERSION', '') == 'pypy2':
        additional_compile_flags.append('-fpermissive')

    # re-map the source files in the debug symbol tables to there original location so that stepping in a debugger works
    if pykeyvi_source_path is not None:
        additional_compile_flags.append('-fdebug-prefix-map={}={}'.format(pykeyvi_source_path, keyvi_source_path))


    linklibraries_static_or_dynamic = [
        "boost_program_options",
        "boost_iostreams",
        "boost_filesystem",
        "boost_system",
        "boost_regex",
        "boost_thread",
        "snappy"
    ]

    linklibraries = [
        "tpie",
        "z"
    ]

    mac_os_static_libs_dir = 'mac_os_static_libs'

    extra_link_arguments = []
    link_library_dirs = [tpie_lib_dir]

    if sys.platform == 'darwin':
        additional_compile_flags.append("-DOS_MACOSX")
        additional_compile_flags.append('-mmacosx-version-min=10.9')
        linklibraries_static_or_dynamic.remove('boost_thread')
        linklibraries_static_or_dynamic.append('boost_thread-mt')
        link_library_dirs.append(mac_os_static_libs_dir)
        extra_link_arguments.append('-L{}'.format(mac_os_static_libs_dir))

    #########################
    # Custom 'build' command
    #########################

    custom_user_options = [('mode=',
                            None,
                            "build mode."),
                           ('staticlinkboost',
                            None,
                            "special mode to statically link boost."),
                           ]


    class custom_opts:

        parent = None

        def initialize_options(self):
            self.parent.initialize_options(self)
            self.mode = 'release'
            self.staticlinkboost = False

        def run(self):
            global additional_compile_flags
            global linklibraries
            global linklibraries_static_or_dynamic
            global extra_link_arguments
            global ext_modules
            print ("Building in {0} mode".format(self.mode))

            if self.mode == 'debug':
                additional_compile_flags.append("-O0")
                additional_compile_flags.append("-ggdb3")
                additional_compile_flags.append("-fstack-protector")
            else:
                additional_compile_flags.append("-O3")

            if self.mode == 'coverage':
                additional_compile_flags.append("--coverage")
                linklibraries.append("gcov")

            # check linking
            if self.staticlinkboost:
                # set static
                extra_link_arguments = ['-Wl,-Bstatic']
                for lib in linklibraries_static_or_dynamic:
                    extra_link_arguments.append("-l{}".format(lib))
                # reset to dynamic
                extra_link_arguments.append('-Wl,-Bdynamic')
                extra_link_arguments.append('-static-libstdc++')
                extra_link_arguments.append('-static-libgcc')
                # workaround: link librt explicitly
                linklibraries.append("rt")
            else:
                # no static linking, add the libs to dynamic linker
                linklibraries += linklibraries_static_or_dynamic

            # patch the compile flags
            for ext_m in ext_modules:
                flags = getattr(ext_m, 'extra_compile_args') + additional_compile_flags
                setattr(ext_m, 'extra_compile_args', flags)
                setattr(ext_m, 'libraries', linklibraries)
                args = getattr(ext_m, 'extra_link_args') + extra_link_arguments
                setattr(ext_m, 'extra_link_args', args)

            self.parent.run(self)


    class build(custom_opts, _build.build):
        parent = _build.build
        user_options = _build.build.user_options + custom_user_options


    class sdist(_sdist.sdist):
        def run(self):
            generate_pykeyvi_source()
            _sdist.sdist.run(self)


    class build_ext(_build_ext.build_ext):
        def run(self):
            generate_pykeyvi_source()

            if sys.platform == 'darwin':
                if not os.path.exists(mac_os_static_libs_dir):
                    os.makedirs(mac_os_static_libs_dir)

                for lib in linklibraries_static_or_dynamic:
                    lib_file_name = 'lib{}.a'.format(lib)
                    src_file = path.join('/usr/local/lib', lib_file_name)
                    dst_file = path.join(mac_os_static_libs_dir, lib_file_name)
                    shutil.copyfile(src_file, dst_file)

            if not path.exists(path.join(tpie_lib_dir, 'libtpie.a')):
                try:
                    cpu_count = multiprocessing.cpu_count()
                except:
                    cpu_count = 1

                CMAKE_CXX_FLAGS = '-fPIC -std=c++11'
                if sys.platform == 'darwin':
                    CMAKE_CXX_FLAGS += ' -mmacosx-version-min=10.9'

                tpie_build_cmd = 'mkdir -p {}'.format(tpie_build_dir)
                tpie_build_cmd += ' && cd {}'.format(tpie_build_dir)
                tpie_build_cmd += ' && cmake -D CMAKE_BUILD_TYPE:STRING=Release ' \
                                  ' -D TPIE_PARALLEL_SORT=1 -D COMPILE_TEST=OFF -D CMAKE_CXX_FLAGS="{CXX_FLAGS}"' \
                                  ' -D CMAKE_INSTALL_PREFIX={INSTALL_PREFIX} ..'.format(
                    CXX_FLAGS=CMAKE_CXX_FLAGS, INSTALL_PREFIX=tpie_install_prefix)
                tpie_build_cmd += ' && make -j {}'.format(cpu_count)
                tpie_build_cmd += ' && make install'

                subprocess.call(tpie_build_cmd, shell=True)

            os.environ['ARCHFLAGS'] = '-arch x86_64'
            _build_ext.build_ext.run(self)


    ext_modules = [Extension('pykeyvi',
                             include_dirs=[autowrap_data_dir,
                                           tpie_include_dir,
                                           path.join(dictionary_sources, 'include/keyvi'),
                                           path.join(dictionary_sources, '3rdparty/rapidjson/include'),
                                           path.join(dictionary_sources, '3rdparty/msgpack-c/include'),
                                           path.join(dictionary_sources, '3rdparty/utf8'),
                                           path.join(dictionary_sources, '3rdparty/misc'),
                                           path.join(dictionary_sources, '3rdparty/xchange/src')],
                             language='c++',
                             sources=[pykeyvi_cpp],
                             extra_compile_args=['-std=c++11', '-msse4.2'] + additional_compile_flags,
                             extra_link_args=extra_link_arguments,
                             library_dirs=link_library_dirs,
                             libraries=linklibraries)]

    PACKAGE_NAME = 'pykeyvi'

    version = '0.2.4'

    install_requires = [
        'msgpack-python',
    ]

    setup(
        name=PACKAGE_NAME,
        version=version,
        description='Python package for keyvi',
        author='Hendrik Muhs',
        author_email='hendrik.muhs@gmail.com',
        license="ASL 2.0",
        cmdclass={'build_ext': build_ext, 'sdist': sdist, 'build': build},
        scripts=['bin/keyvi'],
        packages=['keyvicli'],
        ext_modules=ext_modules,
        zip_safe=False,
        url='https://github.com/cliqz-oss/keyvi',
        download_url='https://github.com/cliqz-oss/keyvi/tarball/v{}'.format(version),
        keywords=['FST'],
        classifiers=[
            'Programming Language :: C++',
            'Programming Language :: Cython',
            'Programming Language :: Python',
            'Programming Language :: Python :: 2.7',
            'Programming Language :: Python :: 3.4',
            'Programming Language :: Python :: 3.5',
            'Programming Language :: Python :: Implementation :: CPython',
            'Programming Language :: Python :: Implementation :: PyPy',
            'Operating System :: MacOS :: MacOS X',
            'Operating System :: Unix',
        ],
        install_requires=install_requires,
    )
