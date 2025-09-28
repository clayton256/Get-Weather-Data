

PLATFORM=$(shell uname -s)

STATIONID="KPALEHIG21"
STATIONKEY="rURDv0YC"

ifeq ($(PLATFORM),"TRUE")
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
endif
ifeq ($(PLATFORM),Linux)
	SYSTEM=/usr/
	LIBS=
	LIBDIR=/usr/lib/x86_64-linux-gnu/
endif
ifeq ($(PLATFORM), Darwin)
	SYSTEM=/usr/
	LIBS=
	LIBDIR=/opt/homebrew/lib
	INCDIR=/opt/homebrew/include/
endif


all: weatherstation


weatherstation: weatherstation.c
	$(CC) -v -g3 weatherstation.c -o $@ -Wno-deprecated -I$(INCDIR) -I$(BASEDIR)/target-mipsel_24kc_musl/usr/include/ -DSTATIONID='${STATIONID}' -DSTATIONKEY='${STATIONKEY}' -DPLATFORM='${PLATFORM}' -lusb-1.0 -lcurl $(LIBS) -L$(LIBDIR) -L$(LIBDIR)


linux-install:
	echo

macos-install:
	plutil -lint com.mark-clayton.weatherstation
	launchctl unload com.mark-clayton.weatherstation
	cp com.mark-clayton.weatherstation.plist /Library/LaunchDaemons
	launchctl load com.mark-clayton.weatherstation

clean:
	rm weatherstation.o weatherstation



