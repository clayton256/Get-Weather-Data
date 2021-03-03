



STATIONID="KPALEHIG21"
STATIONKEY="rURDv0YC"

ifeq ($(),"TRUE")
	SYSTEM=mipsel-openwrt-linux-
	#export STAGING_DIR=/home/clayton/Projects/omega2/staging_dir
	export STAGING_DIR=/root/remote/staging_dir
	BASEDIR=$(STAGING_DIR)
	LIBS=-lmbedtls -lmbedx509 -lmbedcrypto 
	LIBDIR=$(BASEDIR)$(TARGET)/usr/lib/
	INCDIR=$(BASEDIR)$(TARGET)/usr/include/
	BINDIR=$(BASEDIR)$(TOOLCHAIN)/bin/
	CC=$(BINDIR)$(SYSTEM)gcc
	ifeq ($(),"TRUE")
		TARGET=/target-mipsel_24kc_musl
		TOOLCHAIN=/toolchain-mipsel_24kc_gcc-7.3.0_musl
		../staging_dir/toolchain-mipsel_24kc_gcc-7.3.0_musl/bin/mipsel-openwrt-linux-musl-gcc
	else #docker
		TOOLCHAIN=/host
		TARGET=/target-mipsel_24kc_musl
	endif
else
	SYSTEM=/usr/
	LIBS=
	LIBDIR=/usr/lib/x86_64-linux-gnu/
endif




all: weatherstation


weatherstation: weatherstation.c
	$(CC) -v -g weatherstation.c -o $@ -I$(INCDIR) -I$(BASEDIR)/target-mipsel_24kc_musl/usr/include/ -DSTATIONID='${STATIONID}' -DSTATIONKEY='${STATIONKEY}' -lusb-1.0 -lcurl $(LIBS) -L$(LIBDIR) -L$(LIBDIR)/usr/local/



clean:
	rm weatherstation.o weatherstation



