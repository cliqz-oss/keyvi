> Keyvi is developed and maintained by Cliqz Engineering Team and Hendrik Muhs. Cliqz is a provider of innovative, privacy-focused browser technologies with integrated quick-search functionality and anti-tracking.

##
[![Travis](https://travis-ci.org/cliqz-oss/keyvi.svg?branch=master  "Travis build status")](https://travis-ci.org/cliqz-oss/keyvi)
[![C++](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](/keyvi/README.md)
[![PythonVersions](https://img.shields.io/pypi/pyversions/pykeyvi.svg)](https://pypi.python.org/pypi/pykeyvi/)
[![PythonImpl](https://img.shields.io/pypi/implementation/pykeyvi.svg)](https://pypi.python.org/pypi/pykeyvi/)
[![PythonFormat](https://img.shields.io/pypi/format/pykeyvi.svg)](https://pypi.python.org/pypi/pykeyvi/)
[![PyPIVersion](https://img.shields.io/pypi/v/pykeyvi.svg)](https://pypi.python.org/pypi/pykeyvi/)
[![Coveralls](https://coveralls.io/repos/cliqz-oss/keyvi/badge.svg?branch=master&service=github)](https://coveralls.io/github/cliqz-oss/keyvi?branch=master)

## DEPRECATED
Hey there, fellow Keyvi lovers! This is to inform you that Keyvi has found a new home, and will continue to be developed under the fork at https://github.com/KeyviDev/keyvi. Please go there to get the latest and greatest keyvi packed with new, exciting features and bugfixes.

This repo is kept for historical reasons, and will not be actively maintained.

##
![Keyvi](/doc/images/keyvi-small.png)

Keyvi - the short form for "Key value index" - defines a special subtype of the popular key value store (KVS) technologies. As you can imagine from the name, keyvi is an immutable key value store, therefore an index not a store. Keyvi's strengths: high compression ratio and extreme scalability. So if you need online read/writes keyvi is not for you, however, if your use case is mostly reads and infrequent writes you might be interested in checking keyvi out.

## Introduction
  * [BBuzz2016 talk](https://www.youtube.com/watch?v=GBjisdmHe4g)
  * [Announcement blog post](https://cliqz.com/en/aboutus/blog/keyvi)
  * [Search Meetup Munich Slidedeck](http://www.slideshare.net/HendrikMuhs/keyvi-the-key-value-index-cliqz)

## Install

### Quick

Precompiled binary wheels are available for OS X and Linux on [PyPi](https://pypi.python.org/pypi/pykeyvi). To install use:

    pip install pykeyvi

### From Source

The core part is a C++ header-only library, but the TPIE 3rdparty library needs to be compiled once. The commandline
tools are also part of the C++ code. For instructions check the [Readme](/keyvi/README.md) file.

For the python extension pykeyvi check the [Readme](/pykeyvi/README.md) file in the pykeyvi subfolder.


## Usage

  * Howtos
    * [Compiling Dictionaries/Indexes](/doc/usage/Building%20keyvi%20dictionaries.md)
    * Pykeyvi
      * [Compiling](/doc/usage/Building%20keyvi%20dictionaries%20with%20python.md)
  * [Crashcourse](/doc/usage/Crashcourse.md)
  * [Using pykeyvi with EMR (mrjob or pyspark)](/doc/usage/Using%20pykeyvi%20in%20EMR.md)  

## Internals
  
  * [Construction Basics](/doc/algorithm/Construction-Basics.md)
  * [Persistence Basics](/doc/algorithm/Persistence-Basics.md)
  * [Minimization](/doc/algorithm/Minimization.md)
  * [Scaling](/doc/algorithm/Scaling.md)
  * [Extensibility](/doc/algorithm/Extensibility.md)

If you like to go deep down in the basics, keyvi is inspired by the following 2 papers:

  * Sparse Array (See Storing a Sparse Table, Robert E. Tarjan et al. http://infolab.stanford.edu/pub/cstr/reports/cs/tr/78/683/CS-TR-78-683.pdf)
  * Incremental, which means minimization is done on the fly (See Incremental Construction of Minimal Acyclic Finite-State Automata, J. Daciuk et al.: http://www.mitpressjournals.org/doi/pdf/10.1162/089120100561601)
  
## Licence and 3rdparty dependencies

keyvi is licenced under apache license 2.0, see [licence](LICENSE) for details.

In addition keyvi uses 3rdparty libraries which define their own licence. Please check their respective licence. 
The 3rdparty libraries can be found at [keyvi/3rdparty](/keyvi/3rdparty).


## Contributing

* Bug reports, feature requests and general question can be added as an Issue. 

* PRs are welcome. 

* Questions? Concerns? Feel free to [contact us](mailto:cliqz-oss@cliqz.com?subject=keyvi).
