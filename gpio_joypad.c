#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <wiringPi.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include <fcntl.h>
#include <unistd.h>

#include <signal.h>

#include "gpio_joypad.h"

int Running = 1;
int UinputFd = -1;
int SoundMuted = 0;
int currentContrast = 0;
int currentBrightness = 0;
struct uinput_user_dev UiJoypad;
struct input_event ev;
FILE *displayBrightness = NULL;
FILE *displayContrast = NULL;


void stdMessage(char *message) {
    printf("gpio-joypad : %s\n", message);
}

void cleanUp(void) {

    stdMessage("cleaning up before closing...");
    
    if (UinputFd >= 0 ) {
        ioctl(UinputFd, UI_DEV_DESTROY);    // remove custom uinput joypad;
        close(UinputFd);
    }
    
    saveDisplaySettings();
    stdMessage("done !");
}

// Quick-n-dirty error reporter; print message, clean up and exit.
void err(char *msg) {
    printf("ERROR : '%s'.\n", msg);
    cleanUp();
    exit(1);
}

// Interrupt handler -- set global flag to abort main loop.
void signalHandler(int n) {
    stdMessage("interrupted !");
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

    if ( (*adc >= 0) && (abs(*lastadc - *adc) > ADC_THRESHOLD) ) {
//	printf("last / cur %i / %i\n",*lastadc,*adc);
        *lastadc = *adc;
        state = translateAnalog(*adc, ADC_MID_MARGIN, hatlow, hatmid, hatmax, reverse);
        sendEvent(EV_ABS, evt, state);
//	printf("analog %i %i: %i\n",sw1,sw2, *adc);
//	printf("transl %i %i: %i\n",sw1,sw2, state);
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

    if (retval > ANALOG_MAX) retval = ANALOG_MAX;
    if (retval < ANALOG_MIN) retval = ANALOG_MIN;

//    printf("translate value / final = %i / %i\n", value, retval);

    return retval;
}

// read adc value from multiplexer
int adcRead(int sw1, int sw2) {
    int retval=0;

    digitalWrite(ADC_SW_1,sw1);
    digitalWrite(ADC_SW_2,sw2);

    return analogRead(PORT_ADC);
}


void handleVolume() {
    // volup (18) => up volume with amixer
    if(io[18].clicked == 1) {
        system("/usr/bin/amixer -c 1 -q set Master 1%+;/usr/bin/amixer -c 1 -q set Speaker 1%+");
        io[18].clicked = 0;
    }
    // voldown (19) => down volume with amixer
    if(io[19].clicked == 1) {
        system("/usr/bin/amixer -c 1 -q set Master 1%-;/usr/bin/amixer -c 1 -q set Speaker 1%-");
        io[19].clicked = 0;
    }
}


void openDisplaySettingsFiles() {
    stdMessage("Search for existing setting..."); 
    displayContrast     = fopen("/media/boot/contrast"  , "r");
    displayBrightness   = fopen("/media/boot/brightness", "r");
   
    if (displayContrast) {
        stdMessage("found contast setting");
        fscanf(displayContrast,"%d",&currentContrast);
        fclose(displayContrast);
    }
    
    if (displayBrightness) {
        stdMessage("found brightness setting");
        fscanf(displayBrightness,"%d ",&currentBrightness);
        fclose(displayBrightness);
    }
}


void saveDisplaySettings() {

    stdMessage("save settings");
    displayBrightness   = fopen("/media/boot/brightness", "w");
    displayContrast     = fopen("/media/boot/contrast"  , "w");
    
    if (displayContrast) {
        fprintf(displayContrast, "%d", currentContrast);
        fclose(displayContrast);
    }
    
    if (displayBrightness) {
        fprintf(displayBrightness, "%d",currentBrightness);
        fclose(displayBrightness);
    }
}


void writeToSysfs(char *sysfs, int value) {
    FILE *ptr=NULL;
    ptr = fopen(sysfs, "w");
    
    if(ptr) {
        fprintf(ptr, "%d\n", value);
        fclose(ptr);
    }
}


void handleDisplaySettings() {
    
    // start + up => brightness +
    if(io[16].clicked == 1 && io[0].clicked == 1) {
        currentBrightness+=10;
        io[0].clicked=0;
        if(currentBrightness >= 250) currentBrightness = 250;
        
        writeToSysfs("/sys/class/video/vpp_brightness",currentBrightness);
    }
    
    // start + down => brightness -
    if(io[16].clicked == 1 && io[2].clicked == 1) {
        currentBrightness-=10;
        io[2].clicked=0;
        if(currentBrightness <= -250) currentBrightness = -250;
        
        writeToSysfs("/sys/class/video/vpp_brightness",currentBrightness);
    }


    // start + right => contrast +
    if(io[16].clicked == 1 && io[6].clicked == 1) {
        currentContrast+=10;
        io[6].clicked=0;
        if(currentContrast >= 250) currentContrast = 250;
        
        writeToSysfs("/sys/class/video/vpp_contrast",currentContrast);  
    }
    
    // start + left => contrast -
    if(io[16].clicked == 1 && io[4].clicked == 1) {
        currentContrast-=10;
        io[4].clicked=0;
        if(currentContrast <= 250) currentContrast = -250;
        
        writeToSysfs("/sys/class/video/vpp_contrast",currentContrast);
    }    
}


void init(void) {
    int i=0;

    signal(SIGINT, signalHandler); 

    // init uinput
    UinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(UinputFd < 0) {
        err("failed to open uinputfor gamepad");
    }

    // gamepad
    if(ioctl(UinputFd, UI_SET_EVBIT, EV_KEY)     <0 ) err("ioctl enable key failed"); // activate key press / realease
    if(ioctl(UinputFd, UI_SET_EVBIT, EV_ABS)     <0 ) err("ioctl enable abs failed"); // activate absolute axis
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_X)     <0 ) err("ioctl enable X1 axis failed"); // activate analog X1 axis
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_Y)     <0 ) err("ioctl enable Y1 axis failed"); // activate analog Y1 axis
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_RX)    <0 ) err("ioctl enable X2 axis failed"); // activate analog X2 axis
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_RY)    <0 ) err("ioctl enable Y2 axis failed"); // activate analog Y2 axis
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_HAT0X) <0 ) err("ioctl enable HAT0X axis failed");
    if(ioctl(UinputFd, UI_SET_ABSBIT, ABS_HAT0Y) <0 ) err("ioctl enable HAT0Y axis failed");

    // set-up gpio pin & used uinput codefor gamepad
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
    
    openDisplaySettingsFiles();
}


int main (void)
{
    wiringPiSetup() ;
    init();
    delay(2000);
    int i, change=0;
    int adcx1=0, adcy1=0, adcx2=0, adcy2=0, last_adcx1=ADC_HAT0X_FLA, last_adcy1=ADC_HAT0Y_FLA, last_adcx2=ADC_HAT1X_FLA, last_adcy2=ADC_HAT1Y_FLA;

    while(Running) {
        i=0;

        // handle joypad buttons
        while( io[i].pin >= 0 ) {

            if(digitalRead(io[i].pin)!=io[i].initial && io[i].status == io[i].initial) {
//                printf("Pressed key=%i (pin %i) value = %i!\n", io[i].key, io[i].pin, io[i].activeVal);
		sendEvent(io[i].eventType, io[i].key, io[i].activeVal);
                io[i].status = 0;   // ... mean value 0 for gpio pin
                io[i].clicked = 1;
                change=1;
            }

            if(digitalRead(io[i].pin)==io[i].initial && io[i].status != io[i].initial) {
//                printf("Released %i (%i)!\n", io[i].key, io[i].pin);
                sendEvent(io[i].eventType, io[i].key, 0);
                change=1;
                io[i].status = 1;   // ... mean value 1 for gpio pin
            }

            i++;
        }

        handleVolume();
        handleDisplaySettings();

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
