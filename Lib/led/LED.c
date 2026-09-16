#include "LED.h"
#include "string.h"
#include "wk_gpio.h"

// Flash Light Status Table
const sLED_Pattern LED_Bind = {0x00000002, 2, 100};           // ¿ìÉÁ
const sLED_Pattern LED_Synchronizing = {0x00000001, 2, 1000}; // ÂýÉÁ

const sLED_Pattern LED_Synchronized = {0x00000001, 1, 100}; // ³£ÁÁ
const sLED_Pattern LED_OFF = {0x00000000, 1, 100};          // ³£Ãð

const sLED_Pattern LED_OOM = {0x0000000A, 10, 100}; // 2ÉÁÒ»Ãð
const sLED_Pattern LED_OOL = {0x000003EA, 10, 100}; // 2ÉÁÒ»ÁÁ

const sLED_Pattern LED_OOOM = {0x0000002A, 16, 100}; // 3ÉÁÒ»Ãð
const sLED_Pattern LED_OOOL = {0x0000FF2A, 16, 100}; // 3ÉÁÒ»ÁÁ

const sLED_Pattern LED_OOMP = {0x00000066, 16, 100};  // PULS °æ±¾2ÉÁ1Ãð
const sLED_Pattern LED_OOOMP = {0x00000666, 16, 100}; // PULS °æ±¾3ÉÁ1Ãð

const sLED_Pattern LED_OOOOM = {0x000000AA, 16, 100};         // 4ÉÁÒ»Ãð
const sLED_Pattern LED_InternalError = {0x000002AA, 16, 100}; // 5ÉÁÒ»Ãð
//=============================================================================
// hardware

void ReceiverLedON()
{
    gpio_bits_reset(GPIOA, GPIO_PINS_8);
}

void ReceiverLedOFF()
{
    gpio_bits_set(GPIOA, GPIO_PINS_8);
}

tLedMember aReceiverLed = {
    .cfg = {
        .name = "rx_LED",
    },
    .ops = {
        .led_on = ReceiverLedON,
        .led_off = ReceiverLedOFF,
    },

    .RFLED_Initialized = FALSE,
    .RFLED_MsCounter = 0,
    .RFLED_PatternBitNb = 0,
};

void GyroLedON()
{
    gpio_bits_reset(GPIOB, GPIO_PINS_2);
}

void GyroLedOFF()
{
    gpio_bits_set(GPIOB, GPIO_PINS_2);
}

tLedMember aGyroLed = {
    .cfg = {
        .name = "gyro_LED",
    },
    .ops = {
        .led_on = GyroLedON,
        .led_off = GyroLedOFF,
    },

    .RFLED_Initialized = FALSE,
    .RFLED_MsCounter = 0,
    .RFLED_PatternBitNb = 0,
};

//=============================================================================
// lib

void LED_SetPattern(tLedMember *pLedMember, const sLED_Pattern *pPattern)
{
    if (pLedMember == NULL || pPattern == NULL)
    {
        return;
    }

    if (!memcmp(&pLedMember->RFLED_Pattern, pPattern, sizeof(sLED_Pattern)))
        return;
    pLedMember->RFLED_Pattern = *pPattern;
    pLedMember->RFLED_MsCounter = 0;
    pLedMember->RFLED_PatternBitNb = pPattern->NbPatternBits - 1;
    pLedMember->RFLED_MsCounter = pPattern->MsPerPatternBit - 1;
    return;
}

void LED_Init(tLedMember *pLedMember)
{
    if (pLedMember == NULL)
    {
        return;
    }

    LED_SetPattern(pLedMember, &LED_OFF);
    pLedMember->RFLED_Initialized = TRUE;
    return;
}

void LED_MsIRQTask(tLedMember *pLedMember)
{
    if (pLedMember == NULL)
    {
        return;
    }

    if (!pLedMember->RFLED_Initialized)
    {
        return;
    }

    pLedMember->RFLED_MsCounter++;
    if (pLedMember->RFLED_MsCounter < pLedMember->RFLED_Pattern.MsPerPatternBit)
        return;

    pLedMember->RFLED_MsCounter = 0;
    pLedMember->RFLED_PatternBitNb++;

    if (pLedMember->RFLED_PatternBitNb >= pLedMember->RFLED_Pattern.NbPatternBits)
        pLedMember->RFLED_PatternBitNb = 0;

    if (pLedMember->RFLED_Pattern.Pattern & (1 << pLedMember->RFLED_PatternBitNb))
    {
        pLedMember->ops.led_on();
    }
    else
    {
        pLedMember->ops.led_off();
    }

    return;
}
