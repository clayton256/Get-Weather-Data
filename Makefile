


all: weatherstation


weatherstation: weatherstation.c
	gcc -g weatherstation.c -o $@ -lusb-1.0 -lcurl -L/usr/local/lib -L/usr/local/ -DSTATIONID='"KPALEHIG21"' -DSTATIONKEY='"rURDv0YC"'
