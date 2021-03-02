



STATIONID="KPALEHIG21"
STATIONKEY="rURDv0YC"

#export STAGING_DIR=/home/clayton/Projects/omega2/staging_dir
export STAGING_DIR=/root/remote/staging_dir
BASEDIR=$(STAGING_DIR)
ifeq ($(),"TRUE")
	TARGET=/target-mipsel_24kc_musl
	TOOLCHAIN=/toolchain-mipsel_24kc_gcc-7.3.0_musl
	../staging_dir/toolchain-mipsel_24kc_gcc-7.3.0_musl/bin/mipsel-openwrt-linux-musl-gcc
else #docker
	TOOLCHAIN=/host
	TARGET=/target-mipsel_24kc_musl
endif
SYSTEM=mipsel-openwrt-linux-
LIBDIR=$(BASEDIR)$(TARGET)/usr/lib/
INCDIR=$(BASEDIR)$(TARGET)/usr/include/
BINDIR=$(BASEDIR)$(TOOLCHAIN)/bin/
CC=$(BINDIR)$(SYSTEM)gcc




all: weatherstation


weatherstation.o: weatherstation.c
	$(CC) -c -g weatherstation.c -o $@ -I$(INCDIR) -I$(BASEDIR)/target-mipsel_24kc_musl/usr/include/ -DSTATIONID='${STATIONID}' -DSTATIONKEY='${STATIONKEY}'



weatherstation: weatherstation.o
	$(LD) -g weatherstation.c -o $@ -lusb-1.0 -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -L$(LIBDIR) -L$(LIBDIR)/usr/local/ 


clean:
	rm weatherstation.o weatherstation



