#ifndef GPIO_JOYPAD_H
#define GPIO_JOYPAD_H

#define LOOP_DELAY 12  // delay between two gpio polling

#define PORT_ADC   1   // saradc_ch1
#define ADC_SW_1   21  // gpio pin for switch 1 
#define ADC_SW_2   22  // gpio pin for switch 2

#define ADC_THRESHOLD 8     // 8 do not consider any anylog change below this threshold
#define ADC_MID_MARGIN 40   // distance from FLA value that is consideered as middle

#define	ADC_HAT0X_MAX 534
#define ADC_HAT0X_MIN 106
#define ADC_HAT0X_FLA 327

#define	ADC_HAT0Y_MAX 600
#define ADC_HAT0Y_MIN 123
#define ADC_HAT0Y_FLA 341

#define	ADC_HAT1X_MAX 400
#define ADC_HAT1X_MIN 90
#define ADC_HAT1X_FLA 145

#define	ADC_HAT1Y_MAX 560
#define ADC_HAT1Y_MIN 20
#define ADC_HAT1Y_FLA 240

#define ANALOG_MAX 32767
#define ANALOG_MID 0
#define ANALOG_MIN -32767

struct {
	int pin;       // GPIO Pin (wiring pi)
	int key;       // key to bind to
	int status;    // current status : 1 = released, 0 = pressed
	int initial;   // initial pin status (should always be 1)
    int activeVal; // value to set when active
    int clicked;   // track a clicked event to handle special combos
	int eventType; // EV_KEY or EV_ABS
} io[] = {
	{  3	, BTN_DPAD_UP   , 1 , 1 , 1 , 0, EV_KEY  }, // UP
	{  0	, BTN_DPAD_DOWN , 1 , 1 , 1 , 0, EV_KEY  }, // DOWN
	{ 23	, BTN_DPAD_LEFT , 1 , 1 , 1 , 0, EV_KEY  }, // LEFT
	{  2	, BTN_DPAD_RIGHT, 1 , 1 , 1 , 0, EV_KEY  }, // RIGHT
	{ 14	, BTN_X         , 1 , 1 , 1 , 0, EV_KEY  }, // X
	{ 11	, BTN_A         , 1 , 1 , 1 , 0, EV_KEY  }, // A
	{  6	, BTN_B         , 1 , 1 , 1 , 0, EV_KEY  }, // B
	{ 26	, BTN_Y         , 1 , 1 , 1 , 0, EV_KEY  }, // Y
	{  5	, BTN_TL        , 1 , 1 , 1 , 0, EV_KEY  }, // L1
	{  7	, BTN_TL2       , 1 , 1 , 1 , 0, EV_KEY  }, // L2
	{ 12	, BTN_TR        , 1 , 1 , 1 , 0, EV_KEY  }, // R1
	{  1	, BTN_TR2       , 1 , 1 , 1 , 0, EV_KEY  }, // R2
	{ 10	, BTN_START     , 1 , 1 , 1 , 0, EV_KEY  }, // START
	{  4	, BTN_SELECT    , 1 , 1 , 1 , 0, EV_KEY  }, // SELECT
	{ 24	, KEY_VOLUMEUP  , 1 , 1 , 1 , 0, EV_KEY  }, // Vol up
	{ 27	, KEY_VOLUMEDOWN, 1 , 1 , 1 , 0, EV_KEY  }, // Vol down
	{ -1	, -1            ,- 1,-1 , 1 , 0, EV_KEY  }  // DO NOT REMOVE
};


void cleanUp(void);
void err(char *msg);
void signalHandler(int n);
void sendEvent(int type, int code, int value);
int sendAnalog(int sw1, int sw2, int *adc, int *lastadc, int evt, int hatlow, int hatmid, int hatmax, int reverse);
int translateAnalog(int value, int threshhold, int hatlow, int hatmid, int hatmax, int reverse);
int adcRead(int sw1, int sw2);
void handleVolume();
void openDisplaySettingsFiles();
void saveDisplaySettings();
void handleDisplaySettings();
void stdMessage(char *message);
void writeToSysfs(char *sysfs, int value);
void init(void);

#endif
