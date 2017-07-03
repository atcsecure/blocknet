TEMPLATE = app
TARGET = blocknet-qt
VERSION = 1.0.0

DEFINES += QT_GUI BOOST_THREAD_USE_LIB BOOST_SPIRIT_THREADSAFE

QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

CONFIG += no_include_pwd

CONFIG(release, debug|release): DEFINES += NDEBUG

# UNCOMMENT THIS SECTION TO BUILD ON WINDOWS
# Change paths if needed, these use the foocoin/deps.git repository locations

!include($$PWD/config.pri) {
   error(Failed to include config.pri)
 }

INCLUDEPATH += src src/json src/qt

LIBS += \
    $$join(BOOST_LIB_PATH,,-L,) \
    $$join(BDB_LIB_PATH,,-L,) \
    $$join(OPENSSL_LIB_PATH,,-L,) \
    $$join(QRENCODE_LIB_PATH,,-L,)

LIBS += \
    -lssl \
    -lcrypto \
    -ldb_cxx$$BDB_LIB_SUFFIX \
    -lpthread

windows {
    LIBS += \
        -lshlwapi \
        -lws2_32 \
        -lole32 \
        -loleaut32 \
        -luuid \
        -lgdi32
}

unix:!macx {
    LIBS += \
        -lboost_system \
        -lboost_filesystem \
        -lboost_program_options \
        -lboost_thread \
        -lboost_date_time
}

OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build

# use: qmake "RELEASE=1"
contains(RELEASE, 1) {
    # Mac: compile for maximum compatibility (10.5, 32-bit)
    macx:QMAKE_CXXFLAGS += -mmacosx-version-min=10.5 -arch x86_64 -isysroot /Developer/SDKs/MacOSX10.5.sdk

    !windows:!macx {
        # Linux: static link
        LIBS += -Wl,-Bstatic
    }
}

!win32 {
# for extra security against potential buffer overflows: enable GCCs Stack Smashing Protection
QMAKE_CXXFLAGS *= -fstack-protector-all --param ssp-buffer-size=1
QMAKE_LFLAGS *= -fstack-protector-all --param ssp-buffer-size=1
# We need to exclude this for Windows cross compile with MinGW 4.2.x, as it will result in a non-working executable!
# This can be enabled for Windows, when we switch to MinGW >= 4.4.x.
}
# for extra security on Windows: enable ASLR and DEP via GCC linker flags
win32:QMAKE_LFLAGS *= -Wl,--dynamicbase -Wl,--nxcompat
QMAKE_CXXFLAGS = -fpermissive -std=c++11

# use: qmake "USE_QRCODE=1"
# libqrencode (http://fukuchi.org/works/qrencode/index.en.html) must be installed for support
contains(USE_QRCODE, 1) {
    message(Building with QRCode support)
    DEFINES += USE_QRCODE
    LIBS += -lqrencode
}

# use: qmake "USE_UPNP=1" ( enabled by default; default)
#  or: qmake "USE_UPNP=0" (disabled by default)
#  or: qmake "USE_UPNP=-" (not supported)
# miniupnpc (http://miniupnp.free.fr/files/) must be installed for support
contains(USE_UPNP, -) {
    message(Building without UPNP support)
} else {
    message(Building with UPNP support)
    count(USE_UPNP, 0) {
        USE_UPNP=1
    }
    DEFINES += USE_UPNP=$$USE_UPNP STATICLIB
	DEFINES += USE_UPNP=$$USE_UPNP MINIUPNP_STATICLIB
    INCLUDEPATH += $$MINIUPNPC_INCLUDE_PATH
    LIBS += $$join(MINIUPNPC_LIB_PATH,,-L,) -lminiupnpc
    win32:LIBS += -liphlpapi
}

# use: qmake "USE_DBUS=1"
contains(USE_DBUS, 1) {
    message(Building with DBUS (Freedesktop notifications) support)
    DEFINES += USE_DBUS
    QT += dbus
}

# use: qmake "USE_IPV6=1" ( enabled by default; default)
#  or: qmake "USE_IPV6=0" (disabled by default)
#  or: qmake "USE_IPV6=-" (not supported)
contains(USE_IPV6, -) {
    message(Building without IPv6 support)
} else {
    message(Building with IPv6 support)
    count(USE_IPV6, 0) {
        USE_IPV6=1
    }
    DEFINES += USE_IPV6=$$USE_IPV6
}

contains(BITCOIN_NEED_QT_PLUGINS, 1) {
    DEFINES += BITCOIN_NEED_QT_PLUGINS
    QTPLUGIN += qcncodecs qjpcodecs qtwcodecs qkrcodecs qtaccessiblewidgets
}

INCLUDEPATH += src/leveldb/include src/leveldb/helpers
unix: LIBS += $$PWD/src/leveldb/libleveldb.a $$PWD/src/leveldb/libmemenv.a
LIBS += \
    -L$$PWD/src/leveldb \
    -lleveldb \
    -lsecp256k1

SOURCES += src/txdb-leveldb.cpp \
    src/bloom.cpp \
    src/hash.cpp \
    src/aes_helper.c \
    src/blake.c \
    src/bmw.c \
    src/cubehash.c \
    src/echo.c \
    src/groestl.c \
    src/jh.c \
    src/keccak.c \
    src/luffa.c \
    src/shavite.c \
    src/simd.c \
    src/skein.c \
	src/fugue.c \
	src/hamsi.c \
	src/shabal.c \
    src/whirlpool.c \
    src/qt/messagedialog/messagedelegate.cpp \
    src/qt/messagedialog/messagedialog.cpp \
    src/qt/messagedialog/messagesmodel.cpp \
    src/qt/messagedialog/userdelegate.cpp \
    src/qt/messagedialog/usersmodel.cpp \
    src/lz4/lz4.c \
    src/message.cpp \
    src/messagedb.cpp \
    src/xbridgeconnector.cpp \
    src/qt/xbridgeui/xbridgetransactionsview.cpp \
    src/xbridge/xbridgetransaction.cpp \
    src/xbridge/xbridgetransactionmember.cpp \
    src/qt/xbridgeui/xbridgetransactionsmodel.cpp \
    src/qt/xbridgeui/xbridgetransactiondialog.cpp \
    src/qt/xbridgeui/xbridgeaddressbookmodel.cpp \
    src/qt/xbridgeui/xbridgeaddressbookview.cpp \
    src/xbridge/util/logger.cpp \
    src/xbridge/util/settings.cpp \
    src/xbridge/util/txlog.cpp \
    src/xbridge/config.cpp \
    src/xbridge/xbridgeexchange.cpp \
    src/xbridge/xbridgeapp.cpp \
    src/xbridge/xbridge.cpp \
    src/xbridge/xbridgesession.cpp \
    src/xbridge/util/xutil.cpp \
    src/xbridge/bitcoinrpcconnector.cpp \
    src/xbridge/xbitcoinsecret.cpp \
    src/xbridge/xbitcointransaction.cpp \
    src/xbridge/xkey.cpp \
    src/xbridge/xpubkey.cpp \
    src/xbridge/support/cleanse.cpp \
    src/xbridge/crypto/sha256.cpp \
    src/xbridge/random.cpp \
    src/xbridge/crypto/sha512.cpp \
    src/xbridge/xbitcoinaddress.cpp \
    src/xbridge/xbridgesessionbtc.cpp \
    src/rpc/bitcoinrpc.cpp \
    src/rpc/rpcblockchain.cpp \
    src/rpc/rpcdump.cpp \
    src/rpc/rpcmasternode.cpp \
    src/rpc/rpcmining.cpp \
    src/rpc/rpcnet.cpp \
    src/rpc/rpcrawtransaction.cpp \
    src/rpc/rpcwallet.cpp \
    src/rpc/rpcxbridge.cpp \
    src/masternode/masternodeconfig.cpp \
    src/masternode/masternodeman.cpp \
    src/masternode/masternode.cpp \
    src/masternode/activemasternode.cpp \
    src/masternode/masternode-sync.cpp \
    src/masternode/masternode-payments.cpp \
    src/masternode/netfulfilledman.cpp \
    src/datasigner.cpp

!win32 {
    # we use QMAKE_CXXFLAGS_RELEASE even without RELEASE=1 because we use RELEASE to indicate linking preferences not -O preferences
    genleveldb.commands = cd $$PWD/src/leveldb && CC=$$QMAKE_CC CXX=$$QMAKE_CXX $(MAKE) OPT=\"$$QMAKE_CXXFLAGS $$QMAKE_CXXFLAGS_RELEASE\" libleveldb.a libmemenv.a
} else {
    # make an educated guess about what the ranlib command is called
    isEmpty(QMAKE_RANLIB) {
        QMAKE_RANLIB = $$replace(QMAKE_STRIP, strip, ranlib)
    }
    LIBS += -lshlwapi
    genleveldb.commands = cd $$PWD/src/leveldb && CC=$$QMAKE_CC CXX=$$QMAKE_CXX TARGET_OS=OS_WINDOWS_CROSSCOMPILE $(MAKE) OPT=\"$$QMAKE_CXXFLAGS $$QMAKE_CXXFLAGS_RELEASE\" libleveldb.a libmemenv.a && $$QMAKE_RANLIB $$PWD/src/leveldb/libleveldb.a && $$QMAKE_RANLIB $$PWD/src/leveldb/libmemenv.a
}
genleveldb.target = $$PWD/src/leveldb/libleveldb.a
genleveldb.depends = FORCE

unix {
    PRE_TARGETDEPS += $$PWD/src/leveldb/libleveldb.a
    QMAKE_EXTRA_TARGETS += genleveldb
    # Gross ugly hack that depends on qmake internals, unfortunately there is no other way to do it.
    QMAKE_CLEAN += $$PWD/src/leveldb/libleveldb.a; cd $$PWD/src/leveldb ; $(MAKE) clean
}

# regenerate src/build.h
#!windows|contains(USE_BUILD_INFO, 1) {
#    genbuild.depends = FORCE
#    genbuild.commands = cd $$PWD; /bin/sh share/genbuild.sh $$OUT_PWD/build/build.h
#    genbuild.target = $$OUT_PWD/build/build.h
#    PRE_TARGETDEPS += $$OUT_PWD/build/build.h
#    QMAKE_EXTRA_TARGETS += genbuild
#    DEFINES += HAVE_BUILD_INFO
#}

contains(USE_O3, 1) {
    message(Building O3 optimization flag)
    QMAKE_CXXFLAGS_RELEASE -= -O2
    QMAKE_CFLAGS_RELEASE -= -O2
    QMAKE_CXXFLAGS += -O3
    QMAKE_CFLAGS += -O3
}

*-g++-32 {
    message("32 platform, adding -msse2 flag")

    QMAKE_CXXFLAGS += -msse2
    QMAKE_CFLAGS += -msse2
}

greaterThan(QT_MAJOR_VERSION, 4) {
    win32:QMAKE_CXXFLAGS_WARN_ON = \
        -fdiagnostics-show-option \
        -Wall \
        -Wextra \
        -Wno-ignored-qualifiers \
        -Wformat \
        -Wformat-security \
        -Wno-unused-parameter \
        -Wstack-protector
    macx:QMAKE_CXXFLAGS_WARN_ON = \
        -fdiagnostics-show-option \
        -Wall \
        -Wextra \
        -Wformat \
        -Wformat-security \
        -Wno-unused-parameter \
        -Wstack-protector
}
lessThan(QT_MAJOR_VERSION, 5) {
    QMAKE_CXXFLAGS_WARN_ON = \
        -fdiagnostics-show-option \
        -Wall \
        -Wextra \
        -Wno-ignored-qualifiers \
        -Wformat \
        -Wformat-security \
        -Wno-unused-parameter \
        -Wstack-protector
}

# Input
DEPENDPATH += src src/json src/qt
HEADERS += src/qt/bitcoingui.h \
    src/qt/transactiontablemodel.h \
    src/qt/addresstablemodel.h \
    src/qt/optionsdialog.h \
    src/qt/coincontroldialog.h \
    src/qt/coincontroltreewidget.h \
    src/qt/sendcoinsdialog.h \
    src/qt/addressbookpage.h \
    src/qt/signverifymessagedialog.h \
    src/qt/aboutdialog.h \
    src/qt/editaddressdialog.h \
    src/qt/bitcoinaddressvalidator.h \
    src/alert.h \
    src/addrman.h \
    src/base58.h \
    src/bignum.h \
    src/checkpoints.h \
    src/compat.h \
    src/coincontrol.h \
    src/sync.h \
    src/util.h \
    src/uint256.h \
    src/kernel.h \
    src/scrypt.h \
    src/pbkdf2.h \
    src/serialize.h \
    src/strlcpy.h \
    src/main.h \
    src/miner.h \
    src/net.h \
    src/key.h \
    src/db.h \
    src/txdb.h \
    src/walletdb.h \
    src/script.h \
    src/init.h \
    src/irc.h \
    src/mruset.h \
    src/json/json_spirit_writer_template.h \
    src/json/json_spirit_writer.h \
    src/json/json_spirit_value.h \
    src/json/json_spirit_utils.h \
    src/json/json_spirit_stream_reader.h \
    src/json/json_spirit_reader_template.h \
    src/json/json_spirit_reader.h \
    src/json/json_spirit_error_position.h \
    src/json/json_spirit.h \
    src/qt/clientmodel.h \
    src/qt/guiutil.h \
    src/qt/transactionrecord.h \
    src/qt/guiconstants.h \
    src/qt/optionsmodel.h \
    src/qt/monitoreddatamapper.h \
    src/qt/transactiondesc.h \
    src/qt/transactiondescdialog.h \
    src/qt/bitcoinamountfield.h \
    src/wallet.h \
    src/keystore.h \
    src/qt/transactionfilterproxy.h \
    src/qt/transactionview.h \
    src/qt/walletmodel.h \
    src/qt/overviewpage.h \
    src/qt/csvmodelwriter.h \
    src/crypter.h \
    src/qt/sendcoinsentry.h \
    src/qt/qvalidatedlineedit.h \
    src/qt/bitcoinunits.h \
    src/qt/qvaluecombobox.h \
    src/qt/askpassphrasedialog.h \
    src/protocol.h \
    src/qt/notificator.h \
	src/qt/statisticspage.h \
    src/qt/qtipcserver.h \
    src/allocators.h \
    src/ui_interface.h \
    src/qt/rpcconsole.h \
    src/version.h \
    src/netbase.h \
    src/clientversion.h \
    src/bloom.h \
    src/checkqueue.h \
    src/hash.h \
    src/hashblock.h \
    src/limitedmap.h \
    src/sph_blake.h \
    src/sph_bmw.h \
    src/sph_cubehash.h \
    src/sph_echo.h \
    src/sph_groestl.h \
    src/sph_jh.h \
    src/sph_keccak.h \
    src/sph_luffa.h \
    src/sph_shavite.h \
    src/sph_simd.h \
    src/sph_skein.h \
	src/sph_fugue.h \
	src/sph_hamsi.h \
	src/sph_shabal.h \
	src/sph_whirlpool.h \
    src/sph_types.h \
    src/threadsafety.h \
    src/txdb-leveldb.h \
    src/qt/macnotificationhandler.h \    
    src/qt/blockbrowser.h \
    src/qt/messagedialog/messagedialog.h \
    src/util/verify.h \
    src/qt/messagedialog/messagesmodel.h \
    src/qt/messagedialog/messagedelegate.h \
    src/message.h \
    src/messagedb.h \
    src/qt/messagedialog/message_metatype.h \
    src/lz4/lz4.h \
    src/qt/messagedialog/userdelegate.h \
    src/qt/messagedialog/usersmodel.h \
    src/lz4/lz4.h \
    src/FastDelegate.h \
    src/xbridgeconnector.h \
    src/qt/xbridgeui/xbridgetransactionsview.h \
    src/xbridge/xbridgetransaction.h \
    src/xbridge/xbridgetransactiondescr.h \
    src/xbridge/xbridgetransactionmember.h \
    src/qt/xbridgeui/xbridgetransactionsmodel.h \
    src/qt/xbridgeui/xbridgetransactiondialog.h \
    src/qt/xbridgeui/xbridgeaddressbookmodel.h \
    src/qt/xbridgeui/xbridgeaddressbookview.h \
    src/xbridge/util/logger.h \
    src/xbridge/util/settings.h \
    src/xbridge/util/txlog.h \
    src/xbridge/config.h \
    src/xbridge/xbridgeexchange.h \
    src/xbridge/xbridgewallet.h \
    src/xbridge/xbridgeapp.h \
    src/xbridge/xbridge.h \
    src/xbridge/xbridgepacket.h \
    src/xbridge/version.h \
    src/xbridge/xbridgesession.h \
    src/xbridge/util/xutil.h \
    src/xbridge/xuiconnector.h \
    src/xbridge/bitcoinrpcconnector.h \
    src/xbridge/xbitcoinsecret.h \
    src/xbridge/xbitcointransaction.h \
    src/xbridge/xkey.h \
    src/xbridge/xpubkey.h \
    src/xbridge/support/cleanse.h \
    src/xbridge/crypto/sha256.h \
    src/xbridge/crypto/common.h \
    src/xbridge/ptr.h \
    src/xbridge/random.h \
    src/xbridge/crypto/sha512.h \
    src/xbridge/xbitcoinaddress.h \
    src/xbridge/xbridgesessionbtc.h \
    src/rpc/bitcoinrpc.h \
    src/masternode/masternodeconfig.h \
    src/masternode/masternodeman.h \
    src/masternode/masternode.h \
    src/masternode/activemasternode.h \
    src/masternode/masternode-sync.h \
    src/masternode/masternode-payments.h \
    src/tinyformat.h \
    src/masternode/netfulfilledman.h \
    src/datasigner.h


SOURCES += src/qt/bitcoin.cpp src/qt/bitcoingui.cpp \
    src/qt/transactiontablemodel.cpp \
    src/qt/addresstablemodel.cpp \
    src/qt/optionsdialog.cpp \
    src/qt/sendcoinsdialog.cpp \
    src/qt/coincontroldialog.cpp \
    src/qt/coincontroltreewidget.cpp \
    src/qt/addressbookpage.cpp \
    src/qt/signverifymessagedialog.cpp \
    src/qt/aboutdialog.cpp \
    src/qt/editaddressdialog.cpp \
    src/qt/bitcoinaddressvalidator.cpp \
    src/alert.cpp \
    src/version.cpp \
    src/sync.cpp \
    src/util.cpp \
    src/netbase.cpp \
    src/key.cpp \
    src/script.cpp \
    src/main.cpp \
    src/miner.cpp \
	src/qt/statisticspage.cpp \
    src/init.cpp \
    src/net.cpp \
    src/irc.cpp \
    src/checkpoints.cpp \
    src/addrman.cpp \
    src/db.cpp \
    src/walletdb.cpp \
    src/qt/clientmodel.cpp \
    src/qt/guiutil.cpp \
    src/qt/transactionrecord.cpp \
    src/qt/optionsmodel.cpp \
    src/qt/monitoreddatamapper.cpp \
    src/qt/transactiondesc.cpp \
    src/qt/transactiondescdialog.cpp \
    src/qt/bitcoinstrings.cpp \
    src/qt/bitcoinamountfield.cpp \
    src/wallet.cpp \
    src/keystore.cpp \
    src/qt/transactionfilterproxy.cpp \
    src/qt/transactionview.cpp \
    src/qt/walletmodel.cpp \
    src/qt/overviewpage.cpp \
    src/qt/csvmodelwriter.cpp \
    src/crypter.cpp \
    src/qt/sendcoinsentry.cpp \
    src/qt/qvalidatedlineedit.cpp \
    src/qt/bitcoinunits.cpp \
    src/qt/qvaluecombobox.cpp \
    src/qt/askpassphrasedialog.cpp \
    src/protocol.cpp \
    src/qt/notificator.cpp \
    src/qt/qtipcserver.cpp \
    src/qt/rpcconsole.cpp \
    src/noui.cpp \
    src/kernel.cpp \
    src/scrypt-arm.S \
    src/scrypt-x86.S \
    src/scrypt-x86_64.S \
    src/scrypt.cpp \
    src/pbkdf2.cpp \
    src/qt/blockbrowser.cpp

RESOURCES += \
    src/qt/bitcoin.qrc

FORMS += \
    src/qt/forms/coincontroldialog.ui \
    src/qt/forms/sendcoinsdialog.ui \
    src/qt/forms/addressbookpage.ui \
    src/qt/forms/signverifymessagedialog.ui \
    src/qt/forms/aboutdialog.ui \
    src/qt/forms/editaddressdialog.ui \
    src/qt/forms/transactiondescdialog.ui \
    src/qt/forms/overviewpage.ui \
    src/qt/forms/sendcoinsentry.ui \
    src/qt/forms/askpassphrasedialog.ui \
    src/qt/forms/rpcconsole.ui \
    src/qt/forms/optionsdialog.ui \
	src/qt/forms/statisticspage.ui \
    src/qt/forms/blockbrowser.ui \
    src/qt/messagedialog/messagedialog.ui


contains(USE_QRCODE, 1) {
HEADERS += src/qt/qrcodedialog.h
SOURCES += src/qt/qrcodedialog.cpp
FORMS += src/qt/forms/qrcodedialog.ui
}

CODECFORTR = UTF-8

# for lrelease/lupdate
# also add new translations to src/qt/bitcoin.qrc under translations/
TRANSLATIONS = $$files(src/qt/locale/bitcoin_*.ts)

isEmpty(QMAKE_LRELEASE) {
    win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
}
isEmpty(QM_DIR):QM_DIR = $$PWD/src/qt/locale
# automatically build translations, so they can be included in resource file
TSQM.name = lrelease ${QMAKE_FILE_IN}
TSQM.input = TRANSLATIONS
TSQM.output = $$QM_DIR/${QMAKE_FILE_BASE}.qm
TSQM.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
TSQM.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += TSQM

# "Other files" to show in Qt Creator
OTHER_FILES += \
    doc/*.rst doc/*.txt doc/README README.md res/bitcoin-qt.rc

# platform specific defaults, if not overridden on command line
#isEmpty(BOOST_LIB_SUFFIX) {
#    macx:BOOST_LIB_SUFFIX = -mt
#    windows:BOOST_LIB_SUFFIX = -mgw48-mt-s-1_55
#}

isEmpty(BOOST_THREAD_LIB_SUFFIX) {
    BOOST_THREAD_LIB_SUFFIX = $$BOOST_LIB_SUFFIX
}

isEmpty(BDB_LIB_PATH) {
    macx:BDB_LIB_PATH = /opt/local/lib/db48
}

isEmpty(BDB_LIB_SUFFIX) {
    macx:BDB_LIB_SUFFIX = -4.8
}

isEmpty(BDB_INCLUDE_PATH) {
    macx:BDB_INCLUDE_PATH = /opt/local/include/db48
}

isEmpty(BOOST_LIB_PATH) {
    macx:BOOST_LIB_PATH = /opt/local/lib
}

isEmpty(BOOST_INCLUDE_PATH) {
    macx:BOOST_INCLUDE_PATH = /opt/local/include
}

windows:DEFINES += WIN32
windows:RC_FILE = src/qt/res/bitcoin-qt.rc

windows:!contains(MINGW_THREAD_BUGFIX, 0) {
    # At least qmake's win32-g++-cross profile is missing the -lmingwthrd
    # thread-safety flag. GCC has -mthreads to enable this, but it doesn't
    # work with static linking. -lmingwthrd must come BEFORE -lmingw, so
    # it is prepended to QMAKE_LIBS_QT_ENTRY.
    # It can be turned off with MINGW_THREAD_BUGFIX=0, just in case it causes
    # any problems on some untested qmake profile now or in the future.
    DEFINES += _MT BOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN
    QMAKE_LIBS_QT_ENTRY = -lmingwthrd $$QMAKE_LIBS_QT_ENTRY
}

!windows:!macx {
    DEFINES += LINUX
    LIBS += -lrt
}

macx:HEADERS += src/qt/macdockiconhandler.h src/qt/macnotificationhandler.h
macx:OBJECTIVE_SOURCES += src/qt/macdockiconhandler.mm src/qt/macnotificationhandler.mm
macx:LIBS += -framework Foundation -framework ApplicationServices -framework AppKit
macx:DEFINES += MAC_OSX MSG_NOSIGNAL=0
macx:ICON = src/qt/res/icons/blocknet.icns
macx:TARGET = "blocknet-Qt"
macx:QMAKE_CFLAGS_THREAD += -pthread
macx:QMAKE_LFLAGS_THREAD += -pthread
macx:QMAKE_CXXFLAGS_THREAD += -pthread

# Set libraries and includes at end, to use platform-defined defaults if not overridden
INCLUDEPATH += $$BOOST_INCLUDE_PATH $$BDB_INCLUDE_PATH $$OPENSSL_INCLUDE_PATH $$QRENCODE_INCLUDE_PATH
LIBS += $$join(BOOST_LIB_PATH,,-L,) $$join(BDB_LIB_PATH,,-L,) $$join(OPENSSL_LIB_PATH,,-L,) $$join(QRENCODE_LIB_PATH,,-L,)
LIBS += -lssl -lcrypto -ldb_cxx$$BDB_LIB_SUFFIX
# -lgdi32 has to happen after -lcrypto (see  #681)
windows:LIBS += -lws2_32 -lshlwapi -lmswsock -lole32 -loleaut32 -luuid -lgdi32
LIBS += \
    -lboost_system$$BOOST_LIB_SUFFIX \
    -lboost_filesystem$$BOOST_LIB_SUFFIX \
    -lboost_program_options$$BOOST_LIB_SUFFIX \
    -lboost_thread$$BOOST_THREAD_LIB_SUFFIX \
    -lboost_date_time$$BOOST_THREAD_LIB_SUFFIX

windows:LIBS += -lboost_chrono$$BOOST_LIB_SUFFIX

contains(RELEASE, 1) {
    !windows:!macx {
        # Linux: turn dynamic linking back on for c/c++ runtime libraries
        LIBS += -Wl,-Bdynamic
    }
}

system($$QMAKE_LRELEASE -silent $$_PRO_FILE_)
