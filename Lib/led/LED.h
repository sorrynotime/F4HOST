#ifndef _LED_H_
#define _LED_H_

typedef struct
{
    unsigned long Pattern;
    unsigned short NbPatternBits;
    unsigned short MsPerPatternBit;
} sLED_Pattern;

typedef struct
{
    struct
    {
        unsigned char name[10];
    } cfg;

    struct
    {
        void (*led_on)(void);
        void (*led_off)(void);
    } ops;

    unsigned char RFLED_Initialized;

    unsigned long RFLED_MsCounter;
    unsigned long RFLED_PatternBitNb;
    sLED_Pattern RFLED_Pattern;
} tLedMember;

extern const sLED_Pattern LED_Bind;
extern const sLED_Pattern LED_Synchronizing;

extern const sLED_Pattern LED_Synchronized;
extern const sLED_Pattern LED_OFF;

extern const sLED_Pattern LED_OOM;
extern const sLED_Pattern LED_OOL;

extern const sLED_Pattern LED_OOOM;
extern const sLED_Pattern LED_OOOL;

extern const sLED_Pattern LED_OOMP;
extern const sLED_Pattern LED_OOOMP;

extern const sLED_Pattern LED_OOOOM;
extern const sLED_Pattern LED_InternalError;

void LED_SetPattern(tLedMember *pLedMember, const sLED_Pattern *pPattern);
void LED_Init(tLedMember *pLedMember);
void LED_MsIRQTask(tLedMember *pLedMember);

//µÆ
extern tLedMember aReceiverLed;
extern tLedMember aGyroLed;
//============================================================================

#endif
