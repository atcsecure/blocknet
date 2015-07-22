# uncomment this line if build with gui
DEFINES += QT_GUI

windows {

    #CONFIG += xproxy_enabled

    DEFINES += HAVE_CXX_STDHEADERS
    DEFINES -= __NO_SYSTEM_INCLUDES

    #mingw
    BOOST_LIB_SUFFIX=-mgw48-mt-1_55
    #vc
    #BOOST_LIB_SUFFIX=-vc120-mt-1_55

    BOOST_INCLUDE_PATH=D:/work/boost/boost_1_55_0
    BOOST_LIB_PATH=D:/work/boost/boost_1_55_0/stage/lib
    
    BDB_INCLUDE_PATH=D:/work/bitcoin/db-6.0.30/build_unix
    BDB_LIB_PATH=D:/work/bitcoin/db-6.0.30/build_unix
    #BDB_LIB_PATH=D:/work/bitcoin/db-6.0.30/build_windows/Win32/Release
    
    OPENSSL_INCLUDE_PATH=D:/work/openssl/openssl/include
    OPENSSL_LIB_PATH=D:/work/openssl/openssl/lib
    
    MINIUPNPC_INCLUDE_PATH=D:/work/bitcoin/
    MINIUPNPC_LIB_PATH=D:/work/bitcoin/miniupnpc

    LIBPNG_INCLUDE_PATH=c:/deps/libpng-1.6.9
    LIBPNG_LIB_PATH=c:/deps/libpng-1.6.9/.libs

    QRENCODE_INCLUDE_PATH=c:/deps/qrencode-3.4.3
    QRENCODE_LIB_PATH=c:/deps/qrencode-3.4.3/.libs
}

macx {
    BOOST_LIB_SUFFIX = -mt
    BOOST_LIB_PATH = /opt/local/lib
    BOOST_INCLUDE_PATH = /opt/local/include

    BDB_LIB_SUFFIX = -4.8
    BDB_LIB_PATH = /opt/local/lib/db48
    BDB_INCLUDE_PATH = /opt/local/include/db48
}
