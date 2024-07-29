#!/bin/bash
DEST=/tmp/indi-cross
PATHS="
"

if [ $# != 1 ]; then
    echo "usage: $0 <hostname>" 2>&1
    exit 1
fi

mkdir -p $DEST/usr/lib/arm-linux-gnueabihf
rsync -a -L -v --filter '. -' $1:/. $DEST/. << 'EOF'
+ /lib
+ /lib/arm-linux-gnueabihf
+ /usr
+ /usr/lib
+ /usr/include
+ /usr/include/arm-linux-gnueabihf
+ /usr/lib/arm-linux-gnueabihf
+ **/arm-linux-gnueabihf/libdl.*
+ **/arm-linux-gnueabihf/libindi*
+ **/arm-linux-gnueabihf/libbz2.*
+ **/arm-linux-gnueabihf/libcairo*
+ **/arm-linux-gnueabihf/libudev*
+ **/arm-linux-gnueabihf/libusb*
+ **/arm-linux-gnueabihf/libusb*
+ **/arm-linux-gnueabihf/libnova*
+ **/arm-linux-gnueabihf/libz.*
+ **/arm-linux-gnueabihf/libjpeg*
+ **/arm-linux-gnueabihf/libfftw3*
+ **/arm-linux-gnueabihf/libtheoraenc*
+ **/arm-linux-gnueabihf/libtheoradec*
+ **/arm-linux-gnueabihf/libogg*
+ **/arm-linux-gnueabihf/libcurl-gnutls.*
+ **/arm-linux-gnueabihf/libbz2.*
+ **/arm-linux-gnueabihf/*cfitsio*
+ **/arm-linux-gnueabihf/*fftw3*
+ **/arm-linux-gnueabihf/pkgconfig
+ **/arm-linux-gnueabihf/pkgconfig/libindi*
+ **/arm-linux-gnueabihf/pkgconfig/*cfitsio*
+ **/arm-linux-gnueabihf/libcom_err.*
+ **/arm-linux-gnueabihf/libfontconfig.*
+ **/arm-linux-gnueabihf/libfreetype.*
+ **/arm-linux-gnueabihf/libgnutls.*
+ **/arm-linux-gnueabihf/libgssapi_krb5.*
+ **/arm-linux-gnueabihf/libidn2.*
+ **/arm-linux-gnueabihf/libk5crypto.*
+ **/arm-linux-gnueabihf/libkrb5.*
+ **/arm-linux-gnueabihf/liblber-2.4.*
+ **/arm-linux-gnueabihf/libldap_r-2.4.*
+ **/arm-linux-gnueabihf/libnettle.*
+ **/arm-linux-gnueabihf/libnghttp2.*
+ **/arm-linux-gnueabihf/libpixman-1.*
+ **/arm-linux-gnueabihf/libpng16.*
+ **/arm-linux-gnueabihf/libpsl.*
+ **/arm-linux-gnueabihf/librtmp.*
+ **/arm-linux-gnueabihf/libssh2.*
+ **/arm-linux-gnueabihf/libX11.*
+ **/arm-linux-gnueabihf/libxcb.*
+ **/arm-linux-gnueabihf/libxcb-render.*
+ **/arm-linux-gnueabihf/libxcb-shm.*
+ **/arm-linux-gnueabihf/libXext.*
+ **/arm-linux-gnueabihf/libXrender.*
+ **/arm-linux-gnueabihf/libunistring.*
+ **/arm-linux-gnueabihf/libhogweed.*
+ **/arm-linux-gnueabihf/libgmp.*
+ **/arm-linux-gnueabihf/libgcrypt.*
+ **/arm-linux-gnueabihf/libp11-kit.*
+ **/arm-linux-gnueabihf/libtasn1.*
+ **/arm-linux-gnueabihf/libkrb5support.*
+ **/arm-linux-gnueabihf/libkeyutils.*
+ **/arm-linux-gnueabihf/libsasl2.*
+ **/arm-linux-gnueabihf/libexpat.*
+ **/arm-linux-gnueabihf/libuuid.*
+ **/arm-linux-gnueabihf/libXau.*
+ **/arm-linux-gnueabihf/libXdmcp.*
+ **/arm-linux-gnueabihf/libgpg-error.*
+ **/arm-linux-gnueabihf/libffi.*
+ **/arm-linux-gnueabihf/libbsd.*
+ /usr/include/libindi**
+ /usr/include/libusb-1.0**
+ /usr/include/libnova**
+ /usr/include/*fitsio*
+ /usr/include/drvrsmem.h
+ /usr/include/longnam.h
+ /usr/include/arm-linux-gnueabihf/*fftw3*
+ /opt
+ /opt/vc**
- *
EOF
