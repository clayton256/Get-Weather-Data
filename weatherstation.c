/*
    Documentation at desert-home.com

    Experimentation with a USB interface to the Acu-Rite 5 in 1 Weatherstation
    specifically for the Raspberry Pi.  The code may work on other systems, but I
    don't have one of those.  Contrary to other folk's thinking, I don't care if
    this ever runs on a Windows PC or an Apple anything.

    I specifically used a model 2032 display with the sensor that I picked
    up at one of those warehouse stores.  The display has a usb plug on it and
    I thought it might be possible to read the usb port and massage the data myself.

    This code represents the result of that effort.

    I gathered ideas from all over the web.  I use the latest (for this second)
    libusb and about a thousand examples of various things that other people have
    done and posted about.  Frankly, I simply can't remember all of them, so please,
    don't be offended if you see your ideas somewhere in here and it isn't attributed.

    I simply lost track of where I found what.

    This module relies on libusb version 1.0.19 which, at this time, can only be
    compiled from source on the raspberry pi.

    Because there likely to be a version of libusb and the associated header file
    on a Pi, use the command line below to build it since the build of libusb-1.0.19
    places things in /usr/local

    cc -o weatherstation  weatherstation.c -L/usr/local/lib -lusb-1.0
    use ldd weatherstation to check which libraries are linked in.
    If you still have trouble with compilation, remember that cc has a -v
    parameter that can help you unwind what is happening.
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <libusb-1.0/libusb.h>
#include <curl/curl.h>

#define TRUE    1
#define FALSE   0

//this bit added by JZ
//time intervals in seconds to wait between updates
int timeint1 = 10; //report type 1
int timeint2 = 30; //report type 2
int timeint3 = 15; //put out data
int timeint4 = 600; //upload data

// The vendor id and product number for the AcuRite 5 in 1 weather head.
#define VENDOR 0x24c0
#define PRODUCT 0x0003

// I store things about the weather device USB connection here.
struct stationData
{
    libusb_device *device;
    libusb_device_handle *handle;
    int verbose;
} weatherStation;

#define WUNDERSTRSZ 64
struct stationWU
{
    // my PWS ID and password
    char stationID[WUNDERSTRSZ];
    char stationPassword[WUNDERSTRSZ];
};


// These are the sensors the the 5 in 1 weather head provides
struct weatherData {
    float   windSpeed;
    time_t  wsTime;
    int     windDirection;
    time_t  wdTime;
    float   temperature;
    time_t  tTime;
    int     humidity;
    time_t  hTime;
    int     rainCounter;
    time_t  rcTime;
    int     rainRaw;
    time_t  rrTime;
    float   barometer;
    time_t  bTime;
} weatherData;

// This is just a function prototype for the compiler
void closeUpAndLeave();

#if PLATFORM == 'Linux'
size_t strlcpy(char *dst, const char *src, size_t dstsize)
{
  size_t len = strlen(src);
  if(dstsize) {
    size_t bl = (len < dstsize-1 ? len : dstsize-1);
    ((char*)memcpy(dst, src, bl))[bl] = 0;
  }
  return len;
}
#endif

// I want to catch control-C and close down gracefully
void sig_handler(int signo)
{
  if (signo == SIGINT)
    fprintf(stderr,"Shutting down ...\n");
    closeUpAndLeave();
}

/*
This tiny thing simply takes the data and prints it so we can see it
*/
// Array to translate the integer direction provided to text
char *Direction[] = {
    "NW",
    "WSW",
    "WNW",
    "W",
    "NNW",
    "SW",
    "N",
    "SSW",
    "ENE",
    "SE",
    "E",
    "ESE",
    "NE",
    "SSE",
    "NNE",
    "S"
};

char *DirectionNum[] = {
    "315",
    "248",
    "293",
    "270",
    "358",
    "225",
    "0",
    "298",
    "68",
    "135",
    "90",
    "113",
    "45",
    "168",
    "23",
    "180"
};

void showit(){

    fprintf(stdout, "{\"windSpeed\":{\"WS\":\"%0.1f\",\"t\":\"%ld\"},"
                    "\"windDirection\":{\"WDS\":\"%s\",\"t\":\"%ld\"},"
                    "\"windDirection\":{\"WDD\":\"%s\",\"t\":\"%ld\"},"
                    "\"windDirRaw\":{\"WDR\":\"%d\",\"t\":\"%ld\"},"
                    "\"temperature\":{\"T\":\"%0.1f\",\"t\":\"%ld\"},"
                    "\"humidity\":{\"H\":\"%d\",\"t\":\"%ld\"},"
                    "\"rainCounter\":{\"RC\":\"%d\",\"t\":\"%ld\"},"
                    "\"rainRaw\":{\"RR\":\"%d\",\"t\":\"%ld\"},"
                    "\"Barometer\":{\"BP\":\"%0.1f\",\"t\":\"%ld\"}"
                    "}\n",
            weatherData.windSpeed, weatherData.wsTime,
            Direction[weatherData.windDirection],weatherData.wdTime,
            DirectionNum[weatherData.windDirection],weatherData.wdTime,
            weatherData.windDirection,weatherData.wdTime,
            weatherData.temperature, weatherData.tTime,
            (int)weatherData.humidity, weatherData.hTime,
            weatherData.rainCounter, weatherData.rcTime,
            weatherData.rainRaw, weatherData.rrTime,
            weatherData.barometer, weatherData.bTime
        );
    fflush(stdout);
    return;
}

int mccurl(struct weatherData * wx, struct stationWU * wu)
{
    CURL *      curl;
    CURLcode    retval;
    int         error = TRUE;
    time_t      now;
    struct tm * dt;
    char        url[2048];
    char        dest[70];

    time(&now);
    dt = gmtime(&now);

    strftime(dest,sizeof(dest)-1, "%FT%T", dt);
    char *urlfmt = "https://markandgrace.com/wx/index.php?view=upload&tm=%s&t1=%0.1f&rh=%d&wdspd=%0.1f&wddir=%s&rn=%0.1f&bp=%0.1f";
    snprintf(url, sizeof(url)-1,
            urlfmt,
            dest,
            wx->temperature,
            wx->humidity,
            wx->windSpeed,
            Direction[wx->windDirection],
            (wx->rainCounter*0.01),
            wx->barometer
        );
    fprintf(stderr, "strlen(url)=%ld\nurl=%s\n", strlen(url), url);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        retval = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fprintf(stderr,"CURL retval: %d\n", retval);
    }
    return 0;
}


int wucurl(struct weatherData * wx, struct stationWU * wu)
{
    CURL *      curl;
    CURLcode    retval;
    int         error = TRUE;
    time_t      now;
    struct tm * dt;
    char        url[2048];

    time(&now);
    dt = gmtime(&now);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    char *urlfmt = "http://weatherstation.wunderground.com/weatherstation/updateweatherstation.php?"
      "ID=%s&"
      "PASSWORD=%s"
      "&dateutc=now"
      //"&dateutc=%04d-%02d-%02dT%02d:%02d:%02d"
      "&windspeedmph=%0.1f"
      "&winddir=%s"
      "&tempf=%0.1f"
      "&rainin=%0.1f"
      "&humidity=%d"
      "baroinhg=%0.1f"
      "&softwaretype=mark-clayton.com";

 /* 1 ID
  * 2 passwd
  * 3 yr
  * 4 mon
  * 5 day
  * 6 hr
  * 7 min
  * 8 sec
  * 9 winspeed
  * 11 Outdoor temp
  * 12 rain in inches
  * 14 humidity
  */
    snprintf(url, sizeof(url)-1,
            urlfmt,
            wu->stationID,
            wu->stationPassword,
            //dt->tm_year,
            //dt->tm_mon,
            //dt->tm_mday,
            //dt->tm_hour,
            //dt->tm_min,
            //dt->tm_sec,
            wx->windSpeed,
            DirectionNum[wx->windDirection],
            wx->temperature,
            (wx->rainCounter*0.01),
            wx->humidity,
            wx->barometer
        );
    fprintf(stderr, "strlen(url)=%ld\nurl=%s\n", strlen(url), url);

    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        retval = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fprintf(stderr,"CURL retval: %d\n", retval);
    }
}

int store_sqlite(struct weatherData * wxdata)
{
    int ret = 0;

    return ret;
}
/*
This code translates the data from the 5 in 1 sensors to something
that can be used by a human.
*/
float getWindSpeed(char *data){
    int leftSide = (data[3] & 0x1f) << 3;
    int rightSide = (data[4] & 0x70) >> 4;
    float speed = (leftSide | rightSide) / 2.0;
    return(speed);
}
int getWindDirection(char *data){
    return(data[4] & 0x0f);
}
float getTemp(char *data){
    // This item spans bytes, have to reconstruct it
    int leftSide = (data[4] & 0x0f) << 7;
    int rightSide = data[5] & 0x7f;
    float combined = leftSide | rightSide;
    return((combined - 400) / 10.0);
}
int getHumidity(char *data){
    int howWet = data[6] &0x7f;
    return(howWet);
}
int getRainCount(char* data, int* rawCount, int noisy){
    static int  did_daily_reset = 0;
    time_t utcnow;
    struct tm* locnow;
    int count = *rawCount;
    int raincount = 0;
    int reading = (data[6] &0x7f);

    if(0 == reading && 0 < count) /* console value went to zero, counter reset? */
    {
        count = 0;
    }

    if(0 == count) //first starting up
        count = reading;

    raincount = reading - count;

    // reset daily rain count to zero right after midnight
    time(&utcnow);
    locnow = localtime(&utcnow);
    if(0 == locnow->tm_hour & 0 == did_daily_reset)
    {
        if(noisy) fprintf(stderr, "Did daily rain reset\n");
        did_daily_reset = 1;
        count = raincount;
    }
    if(1 == locnow->tm_hour & 1 == did_daily_reset)
    {
        if(noisy) fprintf(stderr, "Ready for tomorrows daily rain reset\n");
        did_daily_reset = 0;
    }

    *rawCount = count;
    return(raincount);
}

float getConsoleTemp(char* data, int noisy)
{
    unsigned int  left = (data[21] & 0x00);
    unsigned int  lefts = (data[21]<<8);
    unsigned int  right = (data[22] & 0x00FF);
    unsigned int  reading = lefts | right;
    reading &= 0x0000FFFF;
    fprintf(stderr, "CT = %X %X %X %X %ld", left, lefts, right, reading, sizeof(unsigned int));
    float temp = reading / 511.13;
    fprintf(stderr, "\nConsole Temp = %0.2f\n", temp);
    return temp;
}

float getBaroPress(char* data, int noisy)
{
    unsigned int  left = (data[23] & 0x00);
    unsigned int  lefts = (data[23]<<8);
    unsigned int  right = (data[24] & 0x00FF);
    unsigned int  reading = lefts | right;
    reading &= 0x0000FFFF;
    fprintf(stderr, "BP = %X %X %X %X %ld", left, lefts, right, reading, sizeof(unsigned int));
    float bar = 6.23 * (reading) - 20402;
    bar /= 100.0; // convert to mbar from pascals
    fprintf(stderr, "\nBP = %0.2f\n", bar);
    return bar;
}

// Now that I have the data from the station, do something useful with it.
void decode(char *data, int length, int noisy){
    //int i;
    //for(i=0; i<length; i++){
    //    fprintf(stderr,"%02X ",data[i]);
    //}
    //fprintf(stderr,"\n");
    time_t seconds = time (NULL);
    //There are two varieties of data, both of them have wind speed
    // first variety of the data
    if ((data[2] & 0x0f) == 1){ // this has wind speed, direction and rainfall
        if(noisy)
            fprintf(stderr,"Wind Speed: %.1f ",getWindSpeed(data));
        weatherData.windSpeed = getWindSpeed(data);
        weatherData.wsTime = seconds;
        if(noisy)
            fprintf(stderr,"Wind Direction: %s ",Direction[getWindDirection(data)]);
        weatherData.wdTime = seconds;
        weatherData.windDirection = getWindDirection(data);
        weatherData.rainCounter = getRainCount(data, &(weatherData.rainRaw), noisy);
        weatherData.rcTime = seconds;
        weatherData.rrTime = seconds;
        if(noisy){
            fprintf(stderr,"\nRain Counter: %d ",weatherData.rainCounter);
            fprintf(stderr,"\n");
        }
    }
    // this is the other variety
    if ((data[2] & 0x0f) == 8){ // this has wind speed, temp and relative humidity
        if(noisy)
            fprintf(stderr,"Wind Speed: %.1f ",getWindSpeed(data));
        weatherData.windSpeed = getWindSpeed(data);
        weatherData.wsTime = seconds;
        if(noisy)
            fprintf(stderr,"Temperature: %.1f ",getTemp(data));
        weatherData.temperature = getTemp(data);
        weatherData.tTime = seconds;
        if(noisy){
            fprintf(stderr,"Humidity: %d ", getHumidity(data));
            //fprintf(stderr,"\n");
        }
        weatherData.humidity = getHumidity(data);
        weatherData.hTime = seconds;
    }
}

void decode2(char *data, int length, int noisy)
{
    time_t seconds = time (NULL);

    getConsoleTemp(data, noisy);
    weatherData.barometer = getBaroPress(data, noisy); // convert to mbar from pascals
    weatherData.barometer += 21.1; //adjust for altitude, but what alt????
    weatherData.barometer /= 33.86389;
    weatherData.bTime = seconds;
    if(noisy){
        fprintf(stderr,"Baro: %0.2f ", weatherData.barometer);
        fprintf(stderr,"\n");
    }
    return;
}


/*
This code is related to dealing with the USB device
*/
// This searches the USB bus tree to find the device
int findDevice(libusb_device **devs)
{
    libusb_device *dev;
    int err = 0, i = 0, j = 0;
    uint8_t path[8];

    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0) {
            fprintf(stderr,"Couldn't get device descriptor, %s\n", libusb_strerror(err));
            return(1);
        }

        fprintf(stderr,"%04x:%04x (bus %d, device %d)",
            desc.idVendor, desc.idProduct,
            libusb_get_bus_number(dev), libusb_get_device_address(dev));

        //r = libusb_get_port_numbers(dev, path, sizeof(path));
        //if (r > 0) {
        //  fprintf(stderr," path: %d", path[0]);
        //  for (j = 1; j < r; j++)
        //      fprintf(stderr,".%d", path[j]);
        //}
        fprintf(stderr,"\n");

        if (desc.idVendor == VENDOR && desc.idProduct == PRODUCT){
            fprintf(stderr,"Found the one I want\n");
            weatherStation.device = dev;
            return (1);
        }
    }
    return(0);
}

// to handle testing and try to be clean about closing the USB device,
// I'll catch the signal and close off.
void closeUpAndLeave(){
    //OK, done with it, close off and let it go.
    fprintf(stderr,"Done with device, release and close it\n");
    int err = libusb_release_interface(weatherStation.handle, 0); //release the claimed interface
    if(err) {
        fprintf(stderr,"Couldn't release interface, %s\n", libusb_strerror(err));
        exit(1);
    }
    libusb_close(weatherStation.handle);
    libusb_exit(NULL);
    exit(0);
}

// This is where I read the USB device to get the latest data.
unsigned char data[50]; // where we want the data to go
int getit(int whichOne, int noisy){
    int actual; // how many bytes were actually read

    // The second parameter is bmRequestType and is a bitfield
    // See http://www.beyondlogic.org/usbnutshell/usb6.shtml
    // for the definitions of the various bits.  With libusb, the
    // #defines for these are at:
    // http://libusb.sourceforge.net/api-1.0/group__misc.html#gga0b0933ae70744726cde11254c39fac91a20eca62c34d2d25be7e1776510184209
    actual = libusb_control_transfer(weatherStation.handle,
                    LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_IN,
                    //These bytes were stolen with a USB sniffer
                    0x01,0x0100+whichOne,0,
                    data, 50, 10000);
    if (actual < 0){
        fprintf(stderr,"Read didn't work for report %d, %s\n", whichOne, libusb_strerror(actual));
    }
    else {
        // If you want both of the reports that the station provides,
        // just allow for it.  Right this second, I've found every thing
        // I need in report 1.  When I look further at report 2, this will
        // change
        fprintf(stderr,"R%d:%d:", whichOne, actual);
        int i;
        for(i=0; i<actual; i++){
            fprintf(stderr,"%02X ",data[i]);
        }
        if(1 == whichOne)fprintf(stderr,"\n");
        if (whichOne == 1)
            // The actual data starts after the first byte
            // The first byte is the report number returned by
            // the usb read.
            decode(&data[1], actual-1, noisy);
        if (whichOne == 2) {
            decode2(data, actual-1, noisy);
        }
    }
}
// I do several things here that aren't strictly necessary.  As I learned about
// libusb, I tried things and also used various techniques to learn about the
// weatherstation's implementation.  I left a lot of it in here in case I needed to
// use it later.  Someone may find it useful to hack into some other device.
int main(int argc, char **argv)
{
    char *usage = {"usage: %s -u -n\n"};
    int libusbDebug = 0; //This will turn on the DEBUG for libusb
    int noisy = 0;  //This will print the packets as they come in
    int quiet = 1;
    libusb_device **devs;
    int r, err, c;
    ssize_t cnt;
    struct stationWU wu;

    memset(&weatherStation, '\0', sizeof(struct stationData));
    memset(&wu, '\0', sizeof(struct stationWU));
    memset(&weatherData, '\0', sizeof(struct weatherData));
    weatherData.rainRaw = 0; //first starting up
    strlcpy(wu.stationID, STATIONID, strlen(STATIONID)+1);
    //safestrlcpy(wu.stationID, STATIONID, strlen(STATIONID)+1, WUNDERSTRSZ);
    //safestrlcpy(wu.stationPassword, STATIONKEY, strlen(STATIONKEY)+1, WUNDERSTRSZ);
    strlcpy(wu.stationPassword, STATIONKEY, strlen(STATIONKEY)+1);

    while ((c = getopt (argc, argv, "unqh")) != -1)
        switch (c){
            case 'u':
                libusbDebug = 1;
                break;
            case 'n':
                noisy = 1;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'h':
                fprintf(stderr, usage, argv[0]);
            case '?':
                exit(1);
            default:
                exit(1);
       }
    fprintf (stderr,"libusbDebug = %d, noisy = %d\n", libusbDebug, noisy);

    if (signal(SIGINT, sig_handler) == SIG_ERR)
        fprintf(stderr,"Couldn't set up signal handler\n");
    err = libusb_init(NULL);
    if (err < 0){
        fprintf(stderr,"Couldn't init usblib, %s\n", libusb_strerror(err));
        exit(1);
    }
    // This is where you can get debug output from libusb.
    // just set it to LIBUSB_LOG_LEVEL_DEBUG
    if (libusbDebug)
        libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_DEBUG);
    else
        libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_INFO);


    cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0){
        fprintf(stderr,"Couldn't get device list, %s\n", libusb_strerror(err));
        exit(1);
    }
    // got get the device; the device handle is saved in weatherStation struct.
    if (!findDevice(devs)){
        fprintf(stderr,"Couldn't find the device\n");
        exit(1);
    }
    // Now I've found the weather station and can start to try stuff
    // So, I'll get the device descriptor
    struct libusb_device_descriptor deviceDesc;
    err = libusb_get_device_descriptor(weatherStation.device, &deviceDesc);
    if (err){
        fprintf(stderr,"Couldn't get device descriptor, %s\n", libusb_strerror(err));
        exit(1);
    }
    fprintf(stderr,"got the device descriptor back\n");

    // Open the device and save the handle in the weatherStation struct
    err = libusb_open(weatherStation.device, &weatherStation.handle);
    if (err){
        fprintf(stderr,"Open failed, %s\n", libusb_strerror(err));
        exit(1);
    }
    fprintf(stderr,"I was able to open it\n");
    // There's a bug in either the usb library, the linux driver or the
    // device itself.  I suspect the usb driver, but don't know for sure.
    // If you plug and unplug the weather station a few times, it will stop
    // responding to reads.  It also exhibits some strange behaviour to
    // getting the configuration.  I found out after a couple of days of
    // experimenting that doing a clear-halt on the device while before it
    // was opened it would clear the problem.  So, I have one here and a
    // little further down after it has been opened.
    fprintf(stderr,"trying clear halt on endpoint %X ... ", 0x81);
    err = libusb_clear_halt(weatherStation.handle, 0x81);
    if (err){
        fprintf(stderr,"clear halt crapped, %s  Bug Detector\n", libusb_strerror(err));;
    }
    else {
        fprintf(stderr,"OK\n");
    }

    // Now that it's opened, I can free the list of all devices
    libusb_free_device_list(devs, 1); // Documentation says to get rid of the list
                                      // Once I have the device I need
    fprintf(stderr,"Released the device list\n");
    // Now I have to check to see if the kernal using udev has attached
    // a driver to the device.  If it has, it has to be detached so I can
    // use the device.
    if(libusb_kernel_driver_active(weatherStation.handle, 0) == 1) { //find out if kernel driver is attached
        fprintf(stderr,"Kernal driver active\n");
        if(libusb_detach_kernel_driver(weatherStation.handle, 0) == 0) //detach it
            fprintf(stderr,"Kernel Driver Detached!\n");
    }

    int activeConfig;
    err =libusb_get_configuration(weatherStation.handle, &activeConfig);
    if (err){
        fprintf(stderr,"Can't get current active configuration, %s\n", libusb_strerror(err));;
        exit(1);
    }
    fprintf(stderr,"Currently active configuration is %d\n", activeConfig);

    if(activeConfig != 1){
        err = libusb_set_configuration(weatherStation.handle, 1);
        if (err){
            fprintf(stderr,"Cannot set configuration, %s\n", libusb_strerror(err));;
            exit(1);
        }
    fprintf(stderr,"Just did the set configuration\n");
    }

    err = libusb_claim_interface(weatherStation.handle, 0); //claim interface 0 (the first) of device (mine had jsut 1)
    if(err) {
        fprintf(stderr,"Cannot claim interface, %s\n", libusb_strerror(err));
        exit(1);
    }
    fprintf(stderr,"Claimed Interface\n");
    fprintf(stderr,"Number of configurations: %d\n",deviceDesc.bNumConfigurations);
    struct libusb_config_descriptor *config;
    libusb_get_config_descriptor(weatherStation.device, 0, &config);
    fprintf(stderr,"Number of Interfaces: %d\n",(int)config->bNumInterfaces);
    // I know, the device only has one interface, but I wanted this code
    // to serve as a reference for some future hack into some other device,
    // so I put this loop to show the other interfaces that may
    // be there.  And, like most of this module, I stole the ideas from
    // somewhere, but I can't remember where (I guess it's google overload)
    const struct libusb_interface *inter;
    const struct libusb_interface_descriptor *interdesc;
    const struct libusb_endpoint_descriptor *epdesc;
    int i, j, k;
    for(i=0; i<(int)config->bNumInterfaces; i++) {
        inter = &config->interface[i];
        fprintf(stderr,"Number of alternate settings: %d\n", inter->num_altsetting);
        for(j=0; j < inter->num_altsetting; j++) {
            interdesc = &inter->altsetting[j];
            fprintf(stderr,"Interface Number: %d\n", (int)interdesc->bInterfaceNumber);
            fprintf(stderr,"Number of endpoints: %d\n", (int)interdesc->bNumEndpoints);
            for(k=0; k < (int)interdesc->bNumEndpoints; k++) {
                epdesc = &interdesc->endpoint[k];
                fprintf(stderr,"Descriptor Type: %d\n",(int)epdesc->bDescriptorType);
                fprintf(stderr,"Endpoint Address: 0x%2X\n",(int)epdesc->bEndpointAddress);
                // Below is how to tell which direction the
                // endpoint is supposed to work.  It's the high order bit
                // in the endpoint address.  I guess they wanted to hide it.
                fprintf(stderr," Direction is ");
                if ((int)epdesc->bEndpointAddress & LIBUSB_ENDPOINT_IN != 0)
                    fprintf(stderr," In (device to host)");
                else
                    fprintf(stderr," Out (host to device)");
                fprintf(stderr,"\n");
            }
        }
    }
    fprintf(stderr,"trying clear halt on endpoint %X ... ", (int)epdesc->bEndpointAddress);
    err = libusb_clear_halt(weatherStation.handle, (int)epdesc->bEndpointAddress);
    if (err){
        fprintf(stderr,"clear halt crapped, %s  SHUCKS\n", libusb_strerror(err));;
        closeUpAndLeave();
    }
    else {
        fprintf(stderr,"OK\n");
    }
/*
    if (daemonize) {
        devnull = open(_PATH_DEVNULL, O_RDWR, 0);
        if (devnull == -1)
            err(1, "open(%s)", _PATH_DEVNULL);
    }

    if (!drop_privs())
        err(1, "cannot drop privileges");

    if (daemonize) {
        if (rdaemon(devnull) == -1)
            err(1, "cannot daemonize");
        openlog(__progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);
    }

    / * Use logmsg for output from here on. * /
*/


    // So, for the weather station we now know it has one endpoint and it is set to
    // send data to the host.  Now we can experiment with that.
    //
    // I don't want to just hang up and read the reports as fast as I can, so
    // I'll space them out a bit.  It's weather, and it doesn't change very fast.
    int tickcounter= 0;
    while(1){
        sleep(1);
        if(tickcounter++ % timeint1 == 0){
            getit(1, noisy);
        }
        if(tickcounter % timeint2 == 0){
            getit(2, noisy);
        }
        if ((tickcounter % timeint3 == 0) & !quiet){
            showit();
        }
        if (tickcounter % timeint4 == 0){
            wucurl(&weatherData, &wu);
            mccurl(&weatherData, &wu);
        }
    }
}
