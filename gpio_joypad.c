#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <wiringPi.h>
#include <linux/input.h>
#include <linux/uinput.h>

// 10 ms = 100 event per sec. Should be enought for 60 fps games
#define LOOP_DELAY 10

#define PORT_ADC   1   // saradc_ch1
#define ADC_SW_1   21  // gpio pin for switch 1 
#define ADC_SW_2   22  // gpio pin for switch 2

#define ADC_THRESHOLD 40

#define	ADC_HAT0X_MAX 600
#define ADC_HAT0X_MIN 125
#define ADC_HAT0X_FLA 365

#define	ADC_HAT0Y_MAX 630
#define ADC_HAT0Y_MIN 141
#define ADC_HAT0Y_FLA 357

#define	ADC_HAT1X_MAX 635
#define ADC_HAT1X_MIN 160
#define ADC_HAT1X_FLA 356

#define	ADC_HAT1Y_MAX 625
#define ADC_HAT1Y_MIN 140
#define ADC_HAT1Y_FLA 369

#define ANALOG_MAX 32767
#define ANALOG_MID 0
#define ANALOG_MIN -32767

struct {
	int pin;       // GPIO Pin (wiring pi)
	int key;       // key to bind to
	int status;    // current status : 1 = released, 0 = pressed
	int initial;   // initial pin status (should always be 1)
        int activeVal; // value to set when active
	int eventType; // EV_KEY or EV_ABS
} io[] = {
	{  3	, BTN_DPAD_UP   , 1 , 1 , 1 , EV_KEY  }, // UP
        {  3	, ABS_HAT0Y     , 1 , 1 , -1, EV_ABS  }, // UP
	{  0	, BTN_DPAD_DOWN , 1 , 1 , 1 , EV_KEY  }, // DOWN
        {  0	, ABS_HAT0Y     , 1 , 1 , 1 , EV_ABS  }, // DOWN
	{ 23	, BTN_DPAD_LEFT , 1 , 1 , 1 , EV_KEY  }, // LEFT
        { 23	, ABS_HAT0X     , 1 , 1 , -1, EV_ABS  }, // LEFT
	{  2	, BTN_DPAD_RIGHT, 1 , 1 , 1 , EV_KEY  }, // RIGHT
        {  2	, ABS_HAT0X     , 1 , 1 , 1 , EV_ABS  }, // RIGHT
	{ 14	, BTN_X         , 1 , 1 , 1 , EV_KEY  }, // X
	{ 11	, BTN_A         , 1 , 1 , 1 , EV_KEY  }, // A
	{  6	, BTN_B         , 1 , 1 , 1 , EV_KEY  }, // B
	{ 26	, BTN_Y         , 1 , 1 , 1 , EV_KEY  }, // Y
	{  5	, BTN_TL        , 1 , 1 , 1 , EV_KEY  }, // L1
	{  7	, BTN_TL2       , 1 , 1 , 1 , EV_KEY  }, // L2
	{ 12	, BTN_TR        , 1 , 1 , 1 , EV_KEY  }, // R1
	{  1	, BTN_TR2       , 1 , 1 , 1 , EV_KEY  }, // R2
	{ 10	, BTN_START     , 1 , 1 , 1 , EV_KEY  }, // START
	{  4	, BTN_SELECT    , 1 , 1 , 1 , EV_KEY  }, // SELET
	{ 24	, KEY_VOLUMEUP  , 1 , 1 , 1 , EV_KEY  }, // Vol up
	{ 27	, KEY_VOLUMEDOWN, 1 , 1 , 1 , EV_KEY  }, // Vol down
	{ -1	, -1            ,- 1,-1 , 1 , EV_KEY  }  // DO NOT REMOVE
};

int Running = 1;
int UinputFd = -1;
struct uinput_user_dev UiJoypad;
struct input_event ev;


void cleanUp(void) {

    if (UinputFd >= 0 ) {
        ioctl(UinputFd, UI_DEV_DESTROY);    // remove custom uinput joypad;
        close(UinputFd);
    }
}

// Quick-n-dirty error reporter; print message, clean up and exit.
void err(char *msg) {
    printf("ERROR : '%s'.\n", msg);
    cleanUp();
    exit(1);
}

// Interrupt handler -- set global flag to abort main loop.
void signalHandler(int n) {
    Running = 0;
}

void sendEvent(int type, int code, int value) {
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.code = code;
    ev.value=value;

    if(write(UinputFd, &ev, sizeof(ev)) <0)
        err("sending button event failed");
}

// send converted analog value from adc to uinput
int sendAnalog(int sw1, int sw2, int *adc, int *lastadc, int evt, int hatlow, int hatmid, int hatmax, int reverse) {
    int state;
    *adc = adcRead(sw1, sw2);

    if (abs(*lastadc - *adc) > ADC_THRESHOLD) {
        state = translateAnalog(*adc, ADC_THRESHOLD, hatlow, hatmid, hatmax, reverse);
        sendEvent(EV_ABS, evt, state);
        *lastadc = *adc;
	printf("analog %i %i: %i\n",sw1,sw2, *adc);
	printf("transl %i %i: %i\n",sw1,sw2, state);
	return 1;
    } else {
        return 0;
    }
}

int translateAnalog(int value, int threshhold, int hatlow, int hatmid, int hatmax, int reverse) {
    int retval;

    if(value > hatmid-threshhold && value < hatmid+threshhold)
        retval = ANALOG_MID;

    if(value >= hatmid+threshhold)
        retval = ((value - hatmid) * ANALOG_MAX) / (hatmax - hatmid);

    if(value <= hatmid-threshhold)
        retval = ((value - hatmid) * ANALOG_MIN) / (hatlow - hatmid);

    if (reverse == 1)
        retval = retval * -1;

    return retval;
}

// read adc value from multiplexer
int adcRead(int sw1, int sw2) {
    int retval=0;

    digitalWrite(ADC_SW_1,sw1);
    digitalWrite(ADC_SW_2,sw2);

    return analogRead(PORT_ADC);
}


void init(void) {
    int i=0;

    // init uinput
    UinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(UinputFd < 0) {
        err("failed to open uinput");
    }

    if(ioctl(UinputFd, UI_SET_EVBIT, EV_KEY)     <0 ) err("ioctl enable key failed"); // activate key press / realease
    if(ioctl(UinputFd, UI_SET_EVBIT, EV_ABS)     <0 ) err("ioctl enable abs failed"); // activate absolute axis
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_X)     <0 ) err("ioctl enable X1 axis failed"); // activate analog X1 axis
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_Y)     <0 ) err("ioctl enable Y1 axis failed"); // activate analog Y1 axis
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_RX)    <0 ) err("ioctl enable X2 axis failed"); // activate analog X2 axis
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_RY)    <0 ) err("ioctl enable Y2 axis failed"); // activate analog Y2 axis
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_HAT0X) <0 ) err("ioctl enable HAT0X axis failed");
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_HAT0Y) <0 ) err("ioctl enable HAT0Y axis failed");


    // set-up gpio pin & used uinput code
    while( io[i].pin >= 0 ) {
        pinMode (io[i].pin, INPUT) ;          // init used gpio pin as input
        pullUpDnControl(io[i].pin, PUD_UP);   // init used gpio pin as pullup

        //printf("enabled pin %i => read = %i\n", io[i].pin, digitalRead(io[i].pin));

         if(io[i].key != ABS_HAT0X || io[i].key != ABS_HAT0Y)
	     if(ioctl(UinputFd, UI_SET_KEYBIT, io[i].key) <0 )   // enable used uinput key 
                 err("ioctl failed to enable key");

        i++;
    }

    // set switch pin for adc multiplexer
    pinMode(ADC_SW_1, OUTPUT);
    pinMode(ADC_SW_2, OUTPUT);

    // init uinput joypad device
    memset(&UiJoypad, 0, sizeof(UiJoypad));
    snprintf(UiJoypad.name, UINPUT_MAX_NAME_SIZE, "gpio-joypad");

    UiJoypad.absmax[ABS_X] = ANALOG_MAX;
    UiJoypad.absmin[ABS_X] = ANALOG_MIN;
    UiJoypad.absflat[ABS_X] = ANALOG_MID;

    UiJoypad.absmax[ABS_Y] = ANALOG_MAX;
    UiJoypad.absmin[ABS_Y] = ANALOG_MIN;
    UiJoypad.absflat[ABS_Y] = ANALOG_MID;

    UiJoypad.absmax[ABS_RX] = ANALOG_MAX;
    UiJoypad.absmin[ABS_RX] = ANALOG_MIN;
    UiJoypad.absflat[ABS_RX] = ANALOG_MID;

    UiJoypad.absmax[ABS_RY] = ANALOG_MAX;
    UiJoypad.absmin[ABS_RY] = ANALOG_MIN;
    UiJoypad.absflat[ABS_RY] = ANALOG_MID;

    UiJoypad.absmax[ABS_HAT0X] = 1;
    UiJoypad.absmin[ABS_HAT0X] = -1;
    UiJoypad.absflat[ABS_HAT0X] = 0;

    UiJoypad.absmax[ABS_HAT0Y] = 1;
    UiJoypad.absmin[ABS_HAT0Y] = -1;
    UiJoypad.absflat[ABS_HAT0Y] = 0;

    UiJoypad.id.bustype = BUS_USB;
    UiJoypad.id.vendor  = 0x1;
    UiJoypad.id.product = 0x1;
    UiJoypad.id.version = 1;

    if(write(UinputFd, &UiJoypad, sizeof(UiJoypad)) < 0) err("writing to uinput failed");
    if(ioctl(UinputFd, UI_DEV_CREATE) < 0) err("ioctl failed to create device");
}


int main (void)
{
    wiringPiSetup() ;
    init();
    delay(2000);
    int i, change=0;
    int adcx1, adcy1, adcx2, adcy2, last_adcx1=0, last_adcy1=0, last_adcx2, last_adcy2;

    while(Running) {
        i=0;

        // handle joypad buttons
        while( io[i].pin >= 0 ) {

            if(digitalRead(io[i].pin)!=io[i].initial && io[i].status == io[i].initial) {
                printf("Pressed key=%i (pin %i) value = %i!\n", io[i].key, io[i].pin, io[i].activeVal);
		sendEvent(io[i].eventType, io[i].key, io[i].activeVal);
                io[i].status = 0;   // ... mean value 0 for gpio pin
                change=1;
            }

            if(digitalRead(io[i].pin)==io[i].initial && io[i].status != io[i].initial) {
                printf("Released %i (%i)!\n", io[i].key, io[i].pin);
                sendEvent(io[i].eventType, io[i].key, 0);
                change=1;
                io[i].status = 1;   // ... mean value 1 for gpio pin
            }

            i++;
        }

        // handle analog stick
        if(sendAnalog(1,0,&adcx1,&last_adcx1,ABS_X,ADC_HAT0X_MIN, ADC_HAT0X_FLA, ADC_HAT0X_MAX,1))  change = 1;
        if(sendAnalog(0,1,&adcy1,&last_adcy1,ABS_Y,ADC_HAT0Y_MIN, ADC_HAT0Y_FLA, ADC_HAT0Y_MAX,1))  change = 1;
        if(sendAnalog(0,0,&adcx2,&last_adcx2,ABS_RX,ADC_HAT1X_MIN, ADC_HAT1X_FLA, ADC_HAT1X_MAX,1)) change = 1;
        if(sendAnalog(1,1,&adcy2,&last_adcy2,ABS_RY,ADC_HAT1Y_MIN, ADC_HAT1Y_FLA, ADC_HAT1Y_MAX,1)) change = 1;

        if(change == 1) {
            //printf("Status changed => SYN !\n");
            sendEvent(EV_SYN, 0, 0);
            change=0;
        }

        delay(LOOP_DELAY);
    }

    cleanUp();
    exit(0);
}
