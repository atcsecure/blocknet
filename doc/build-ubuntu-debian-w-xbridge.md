UNIX BUILD NOTES
================

Build Instructions: Ubuntu & Debian

Build requirements:
-------------------
```bash
sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils
```

BOOST
-----
```bash
sudo apt-get install libboost-all-dev
```

BDB
---
For Ubuntu only: db4.8 packages are available here. You can add the repository and install using the following commands:
```bash
sudo apt-get install software-properties-common
sudo add-apt-repository ppa:bitcoin/bitcoin
sudo apt-get update
sudo apt-get install libdb4.8-dev libdb4.8++-dev
```

Ubuntu and Debian have their own libdb-dev and libdb++-dev packages, but these will install BerkeleyDB 5.1 or later, which break binary wallet compatibility with the distributed executables which are based on BerkeleyDB 4.8

MINIUPNPC
---------
```bash
sudo apt-get install libminiupnpc-dev
```

QT5
---
```bash
sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler
sudo apt-get install qt5-default -y
```

QRENCODE
--------
```bash
sudo apt-get install libqrencode-dev (optional)
```

secp256k1
---------
```bash
sudo apt-get install libsecp256k1-dev
```

OR 
```bash
git clone https://github.com/bitcoin/bitcoin
cd /path/to/bitcoin-sources/src/sect256k1
./autogen.sh
./configure --enable-static --disable-shared --enable-module-recovery
make
sudo make install
```

BUILD
=====
```bash
 git clone https://github.com/atcsecure/blocknet.git
 cd /path/to/blocknet
 git checkout xbridge-new-2
 cp config.orig.pri config.pri
 /path/to/qmake blocknet-qt.pro (etc. /usr/lib/x86_64-linux-gnu/qt5/bin on ubuntu)
 make
```
