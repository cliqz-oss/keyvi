from setuptools import setup, Extension
from setuptools.command.install import install
from Cython.Distutils import build_ext
import os
import sys
import pkg_resources

# workaround for autwrap bug (includes incompatible boost)
autowrap_data_dir = "autowrap_includes"

dictionary_sources = os.path.abspath('../keyvi')

mode = 'release'
#mode = 'debug'

additional_compile_flags = []
if (mode == 'debug'):
    additional_compile_flags.append("-O0")
    additional_compile_flags.append("-ggdb3")
    additional_compile_flags.append("-fstack-protector")

linklibraries = ["tpie",
             "boost_program_options",
             "boost_iostreams",
             "boost_filesystem",
             "boost_system",
             "boost_regex",
             "boost_thread",
             "z",
             "snappy"
             ]

if (sys.platform == 'darwin'):
    additional_compile_flags.append("-DOS_MACOSX")
    linklibraries.remove('boost_thread')
    linklibraries.append('boost_thread-mt')

ext_modules = [Extension('pykeyvi',
                        include_dirs = [autowrap_data_dir,
                                        os.path.join(dictionary_sources, 'src/cpp'),
                                        os.path.join(dictionary_sources, '3rdparty/rapidjson/include'),
                                        os.path.join(dictionary_sources, '3rdparty/msgpack-c/include'),
                                        os.path.join(dictionary_sources, '3rdparty/utf8'),
                                        os.path.join(dictionary_sources, '3rdparty/misc'),
                                        os.path.join(dictionary_sources, '3rdparty/tpie/build/install/include')],
                        language = 'c++',
                        sources = ['src/pykeyvi.cpp'],
                        extra_compile_args=['-std=c++11'] + additional_compile_flags,
                        library_dirs = [os.path.join(dictionary_sources, '3rdparty/tpie/build/install/lib')],
                        libraries = linklibraries)]
PACKAGE_NAME = 'pykeyvi'

setup(
    name = PACKAGE_NAME,
    version = '0.0.5',
    description = 'Python bindings for keyvi',
    author = 'Hendrik Muhs',
    cmdclass = {'build_ext': build_ext},
    ext_modules = ext_modules,
    zip_safe = False
)