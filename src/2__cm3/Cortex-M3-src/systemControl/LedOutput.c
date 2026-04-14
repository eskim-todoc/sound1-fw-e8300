
#include <hw.h>
#include <stdbool.h>

#include "processorDirective.h"
#include "board.h"
#include "LedOutput.h"
#include "cfx_cm3_sharedMemory.h"

/**
 * LED 켜기 우선 순위
 *
 * 1 순위.
 * en__LED_Map_Error, en__LED_MCU_Error, en__LED_MCU_Accelerometer_Error,
 * en__LED_MCU_FPGA_Error, en__LED_MCU_RF_PMIC_Error
 *
 * 2 순위.
 * en__LED_POWER_On, en__LED_POWER_Off
 *
 * 3 순위.
 *
 */

static bool testLED_Trigger;

static tdc_led_ind_state_t sg_led_ind_state;

void tdc_led_set_ind_state(tdc_led_ind_state_t state)
{
    sg_led_ind_state = state;
}

tdc_led_ind_state_t tdc_led_get_ind_state(void)
{
    return sg_led_ind_state;
}

void enabletestLED_Trigger(void)
{
    testLED_Trigger = true;
}

void disabletestLED_Trigger(void)
{
    testLED_Trigger = false;
}

bool isTestTriggerEanbled(void)
{
    return testLED_Trigger;
}

static EN__LED_PATTERN LedOutputPattern;

void updateLED_OutputPattern(EN__LED_PATTERN Led_Pattern)
{
    LedOutputPattern = Led_Pattern;
}

EN__LED_PATTERN geteLED_OutputPattern(void)
{
    return LedOutputPattern;
}

static EN__LED_COLOR LED_outputColor = en__LED_BLACK;

void Led_powerOn(bool resetTimerCounter)
{
    const int PatternTime_ms    = 300;
    const int onTime_ms         = 80;
    const int PatternOutput_Num = 5;

    static int timerCounter       = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }
    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_SKYBLUE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
        outputCycleCounter++;
    }

    if (outputCycleCounter == PatternOutput_Num)
    {
        updateLED_OutputPattern(en__LED_NA);
        outputCycleCounter = 0;
    }
}

void Led_powerOff(bool resetTimerCounter)
{
    const int PatternTime_ms    = 300;
    const int onTime_ms         = 100;
    const int PatternOutput_Num = 4;

    static int timerCounter       = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }

    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_BLUE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
        outputCycleCounter++;
    }

    if (outputCycleCounter == PatternOutput_Num)
    {
        updateLED_OutputPattern(en__LED_NA);

        outputCycleCounter = 0;
    }
}

void standby_isdNotConnectedLED_batteryNormal(void)
{
    LED_outputColor = en__LED_GREEN;
}

void standby_isdNotConnectedLED_batteryLow(void)
{
    LED_outputColor = en__LED_ORANGE;
}

void sitimulationOutputOnLED_batteryNormal(bool resetTimerCounter)
{
    const int PatternTime_ms = 1000;
    const int onTime_ms      = 200;

    static int timerCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }

    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_GREEN;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
    }
}

void sitimulationOutputOnLED_batteryLow(bool resetTimerCounter)
{
    const int PatternTime_ms = 1000;
    const int onTime_ms      = 200;

    static int timerCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }

    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_ORANGE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
    }
}

void mappingConnected_ISD_connected_OutputOnLED_batteryNormal(bool resetTimerCounter)
{
    const int PatternTime_ms = 1000;
    const int onTime_ms      = 200;

    static int timerCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }

    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_BLUE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
    }
}
void mappingConnected_ISD_connected_OutputOnLED_batteryLow(bool resetTimerCounter)
{
    const int PatternTime_ms = 1000;
    const int onTime_ms      = 100;

    static int timerCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }

    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_PURPLE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
    }
}

void mappingConnected_ISD_Unconnected_OutputOnLED_batteryNormal(void)
{
    LED_outputColor = en__LED_BLUE;
}
void mappingConnected_ISD_Unconnected_OutputOnLED_batteryLow(void)
{
    LED_outputColor = en__LED_PURPLE;
}

void batteryChargingLevelLED_veryLow(bool resetTimerCounter)
{
    const int PatternTime_ms = 200;
    const int onTime_ms      = 20;

    static int timerCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }

    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_WHITE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
    }
}

void batteryChargingLevelLED_0btw20(bool resetTimerCounter)
{
    const int longPatternOffTime_ms = 4000;
    const int shortPatternTime_ms   = 500;
    const int onTime_ms             = 160;

    const int shortPatternOutput_Nums = 1;

    static int shortTimerCounter  = 0;
    static int longtimeCounter    = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        shortTimerCounter = 0;
    }

    if (shortTimerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_WHITE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    shortTimerCounter++;
    longtimeCounter++;

    if (shortTimerCounter == shortPatternTime_ms)  //
    {
        outputCycleCounter++;

        if (outputCycleCounter != shortPatternOutput_Nums)
        {
            shortTimerCounter = 0;
        }
    }

    if (longtimeCounter == longPatternOffTime_ms)
    {
        shortTimerCounter  = 0;
        longtimeCounter    = 0;
        outputCycleCounter = 0;
    }
}

void batteryChargingLevelLED_20btw40(bool resetTimerCounter)
{
    const int longPatternOffTime_ms = 4000;
    const int shortPatternTime_ms   = 500;
    const int onTime_ms             = 160;

    const int shortPatternOutput_Nums = 2;

    static int shortTimerCounter  = 0;
    static int longtimeCounter    = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        shortTimerCounter = 0;
    }

    if (shortTimerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_WHITE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    shortTimerCounter++;
    longtimeCounter++;

    if (shortTimerCounter == shortPatternTime_ms)  //
    {
        outputCycleCounter++;

        if (outputCycleCounter != shortPatternOutput_Nums)
        {
            shortTimerCounter = 0;
        }
    }

    if (longtimeCounter == longPatternOffTime_ms)
    {
        shortTimerCounter  = 0;
        longtimeCounter    = 0;
        outputCycleCounter = 0;
    }
}

void batteryChargingLevelLED_40btw60(bool resetTimerCounter)
{
    const int longPatternOffTime_ms = 4000;
    const int shortPatternTime_ms   = 500;
    const int onTime_ms             = 160;

    const int shortPatternOutput_Nums = 3;

    static int shortTimerCounter  = 0;
    static int longtimeCounter    = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        shortTimerCounter = 0;
    }

    if (shortTimerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_WHITE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    shortTimerCounter++;
    longtimeCounter++;

    if (shortTimerCounter == shortPatternTime_ms)  //
    {
        outputCycleCounter++;

        if (outputCycleCounter != shortPatternOutput_Nums)
        {
            shortTimerCounter = 0;
        }
    }

    if (longtimeCounter == longPatternOffTime_ms)
    {
        shortTimerCounter  = 0;
        longtimeCounter    = 0;
        outputCycleCounter = 0;
    }
}

void batteryChargingLevelLED_60btw80(bool resetTimerCounter)
{
    const int longPatternOffTime_ms = 4000;
    const int shortPatternTime_ms   = 500;
    const int onTime_ms             = 160;

    const int shortPatternOutput_Nums = 4;

    static int shortTimerCounter  = 0;
    static int longtimeCounter    = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        shortTimerCounter = 0;
    }

    if (shortTimerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_WHITE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    shortTimerCounter++;
    longtimeCounter++;

    if (shortTimerCounter == shortPatternTime_ms)  //
    {
        outputCycleCounter++;

        if (outputCycleCounter != shortPatternOutput_Nums)
        {
            shortTimerCounter = 0;
        }
    }

    if (longtimeCounter == longPatternOffTime_ms)
    {
        shortTimerCounter  = 0;
        longtimeCounter    = 0;
        outputCycleCounter = 0;
    }
}

void batteryChargingLevelLED_80btw100(bool resetTimerCounter)
{
    const int longPatternOffTime_ms = 4000;
    const int shortPatternTime_ms   = 500;
    const int onTime_ms             = 160;

    const int shortPatternOutput_Nums = 5;

    static int shortTimerCounter  = 0;
    static int longtimeCounter    = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        shortTimerCounter = 0;
    }

    if (shortTimerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_WHITE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    shortTimerCounter++;
    longtimeCounter++;

    if (shortTimerCounter == shortPatternTime_ms)  //
    {
        outputCycleCounter++;

        if (outputCycleCounter != shortPatternOutput_Nums)
        {
            shortTimerCounter = 0;
        }
    }

    if (longtimeCounter == longPatternOffTime_ms)
    {
        shortTimerCounter  = 0;
        longtimeCounter    = 0;
        outputCycleCounter = 0;
    }
}

void batteryChargingLevelLED_Full(bool resetTimerCounter)
{
    LED_outputColor = en__LED_WHITE;
}

void LED_Map_error(bool resetTimerCounter)
{
    const int PatternTime_ms = 500;
    const int onTime_ms      = 100;

    static int timerCounter       = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }

    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_RED;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
    }
}

void LED_MCU_error(bool resetTimerCounter)
{
    const int PatternTime_ms = 1000;
    const int onTime_ms      = 500;

    static int timerCounter       = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }

    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_RED;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
    }
}

void LED_Accelerometer_error(bool resetTimerCounter)
{
    const int PatternTime_ms = 2000;
    const int onTime_ms      = 500;

    static int timerCounter       = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }

    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_RED;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
    }
}

void LED_FPGA_error(bool resetTimerCounter)
{
    const int PatternTime_ms = 1000;
    const int onTime_ms      = 500;

    static int timerCounter       = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }

    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_BLUE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
    }
}

void LED_RF_PMIC_error(bool resetTimerCounter)
{
    const int PatternTime_ms = 1000;
    const int onTime_ms      = 500;

    static int timerCounter       = 0;
    static int outputCycleCounter = 0;

    if (resetTimerCounter)
    {
        timerCounter = 0;
    }

    if (timerCounter < onTime_ms)
    {
        LED_outputColor = en__LED_PURPLE;
    }
    else
    {
        LED_outputColor = en__LED_BLACK;
    }

    timerCounter++;

    if (timerCounter >= PatternTime_ms)  //
    {
        timerCounter = 0;
    }
}

void LED_black(void)
{
    LED_outputColor = en__LED_BLACK;
}

void LED_White(void)
{
    LED_outputColor = en__LED_WHITE;
}

void LED_Memory_error(void)
{
#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

void LED_clock_error(void)
{
#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

void turnOffLED(void)
{
    LED_outputColor = en__LED_BLACK;
#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

void turnON_RedLED(void)
{
    // LED_outputColor=en__LED_RED;

#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

void turnON_GreenLED(void)
{
    // LED_outputColor=en__LED_GREEN;

#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

void turnON_BlueLED(void)
{
    // LED_outputColor=en__LED_BLUE;

#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

#if defined(LED_IS_ACTIVELOW)

#if defined(LED_B_pin_CFX_test)
void LED_OUT(void)
{

    const int pwmTime_ms   = 10;
    int       pwmDuty_rate = 3;

    static int timerCounter = 1;

    bool enablePatternOut;

    if (0 == timerCounter % pwmDuty_rate)
        enablePatternOut = false;
    else
        enablePatternOut = true;

    if (enablePatternOut)
    {
        switch (LED_outputColor)
        {

            case en__LED_BLACK:
            {

                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_RED:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_GREEN:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_BLUE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_ORANGE:
            {

                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_SKYBLUE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_PURPLE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_WHITE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;

            default:
            {

                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
        }
    }
    else
    {

        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    }

#if 1  // LED PWM 사용할 경우

    timerCounter++;
    if (timerCounter >= pwmTime_ms)
        timerCounter = 1;

#endif
}

#else

void LED_OUT(void)
{

    const int pwmTime_ms   = 10;
    int       pwmDuty_rate = 3;

    static int timerCounter = 1;

    bool enablePatternOut;

    if (0 == timerCounter % pwmDuty_rate)
        enablePatternOut = false;
    else
        enablePatternOut = true;

    if (enablePatternOut)
    {
        switch (LED_outputColor)
        {

            case en__LED_BLACK:
            {

                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_RED:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_GREEN:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_BLUE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_ORANGE:
            {

                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_SKYBLUE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_PURPLE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_WHITE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;

            default:
            {

                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
        }
    }
    else
    {

        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
    }

#if 1  // LED PWM 사용할 경우

    timerCounter++;
    if (timerCounter >= pwmTime_ms)
        timerCounter = 1;

#endif

    if (isTestTriggerEanbled())
    {
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
    }
}

#endif  //

#else
void LED_OUT(void)
{
    const int pwmTime_ms   = 10;
    int       pwmDuty_rate = 2;  // 3;

    static int timerCounter = 1;

    bool enablePatternOut;

    if (0 == timerCounter % pwmDuty_rate)
    {
        enablePatternOut = false;
    }
    else
    {
        enablePatternOut = true;
    }

    if (enablePatternOut)
    {
        switch (LED_outputColor)
        {
            case en__LED_BLACK:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_RED:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_GREEN:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_BLUE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_ORANGE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_SKYBLUE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_PURPLE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_WHITE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            default:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
        }
    }
    else
    {
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
    }

#if 0  // LED PWM 사용할 경우

    timerCounter++;

    if (timerCounter >= pwmTime_ms)
    {
        timerCounter = 1;
    }

#endif

    if (isTestTriggerEanbled())
    {
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
    }
}

#endif

void LedPatternOut(EN__LED_PATTERN ledOutputPattern)
{
    static EN__LED_PATTERN prev_ledOutputPattern = en__LED_NA;

    bool resetTimerEnable = false;

    updateLED_OutputPattern(ledOutputPattern);

    if (prev_ledOutputPattern != ledOutputPattern)
    {
        resetTimerEnable = true;
    }

    prev_ledOutputPattern = ledOutputPattern;

    switch (ledOutputPattern)
    {
        case en__LED_Map_Error:
            LED_Map_error(resetTimerEnable);
            break;

        case en__LED_MCU_Error:
            LED_MCU_error(resetTimerEnable);
            break;

        case en__LED_MCU_Accelerometer_Error:
            LED_Accelerometer_error(resetTimerEnable);
            break;

        case en__LED_MCU_FPGA_Error:
            LED_FPGA_error(resetTimerEnable);
            break;

        case en__LED_MCU_RF_PMIC_Error:
            LED_RF_PMIC_error(resetTimerEnable);
            break;

        case en__LED_POWER_On:
            Led_powerOn(resetTimerEnable);
            break;

        case en__LED_POWER_Off:
            Led_powerOff(resetTimerEnable);
            break;

#if 1  // 내부기 연결 안되면 깜빡이고, 연결되면 켜져있게 변경 (1==기존, 0==LED 변경)

            // 연결된 상태
        case en__LED_ISD_StimulationOut_batteryNormal:
        {
            if (2 == readLED_indicatorOnOff())
            {
                LED_black();
            }
            else
            {
                sitimulationOutputOnLED_batteryNormal(resetTimerEnable);
            }
        }
        break;

        case en__LED_ISD_StimulationOut_batteryLow:
        {
            if (2 == readLED_indicatorOnOff())
            {
                LED_black();
            }
            else
            {
                sitimulationOutputOnLED_batteryLow(resetTimerEnable);
            }
        }
        break;

        // 스텐바이
        case en__LED_StandbyForconneded_ISD_batteryNormal:
        {
            // 내부기와 연결이 되지 않은 상태에서는 사용자 설정값을 읽어 올 수 없는 상태이며,
            // 연결이 되지 않은 상태는 표시를 해주는 것이 맞을 듯 한다.
            standby_isdNotConnectedLED_batteryNormal();
        }
        break;

        case en__LED_StandbyForconneded_ISD_batteryLow:
        {
            if (2 == readLED_indicatorOnOff())
            {
                LED_black();
            }
            else
            {
                standby_isdNotConnectedLED_batteryLow();
            }
        }
        break;
#else
            // 연결된 상태
        case en__LED_ISD_StimulationOut_batteryNormal:
        {
            if (2 == readLED_indicatorOnOff())
            {
                LED_black();
            }
            else
            {
                standby_isdNotConnectedLED_batteryNormal();
            }
        }
        break;

        case en__LED_ISD_StimulationOut_batteryLow:
        {
            if (2 == readLED_indicatorOnOff())
            {
                LED_black();
            }
            else
            {
                standby_isdNotConnectedLED_batteryLow();
            }
        }
        break;

        // 스텐바이
        case en__LED_StandbyForconneded_ISD_batteryNormal:
        {
            // 내부기와 연결이 되지 않은 상태에서는 사용자 설정값을 읽어 올 수 없는 상태이며,
            // 연결이 되지 않은 상태는 표시를 해주는 것이 맞을 듯 한다.
            if (2 == readLED_indicatorOnOff())
            {
                LED_black();
            }
            else
            {
                sitimulationOutputOnLED_batteryNormal(resetTimerEnable);
            }
        }
        break;

        case en__LED_StandbyForconneded_ISD_batteryLow:
        {
            if (2 == readLED_indicatorOnOff())
            {
                LED_black();
            }
            else
            {
                sitimulationOutputOnLED_batteryLow(resetTimerEnable);
            }
        }
        break;
#endif
            ///

        case en__LED_MappingConneted_ISD_Connected_BatteryNormal:
            mappingConnected_ISD_connected_OutputOnLED_batteryNormal(resetTimerEnable);  // 기존
            // mappingConnected_ISD_Unconnected_OutputOnLED_batteryNormal();  // 내부기 없으면 깜빡이게 변경
            break;

        case en__LED_MappingConneted_ISD_Connected_BatteryLow:
            mappingConnected_ISD_connected_OutputOnLED_batteryLow(resetTimerEnable);  // 기존
            // mappingConnected_ISD_Unconnected_OutputOnLED_batteryLow();  // 내부기 없으면 깜빡이게 변경
            break;

        case en__LED_MappingConneted_ISD_Unconnected_BatteryNormal:
            mappingConnected_ISD_Unconnected_OutputOnLED_batteryNormal();  // 기존
            // mappingConnected_ISD_connected_OutputOnLED_batteryNormal(resetTimerEnable);  // 내부기 없으면 깜빡이게 변경
            break;

        case en__LED_MappingConneted_ISD_Unconnected_BatteryLow:
            mappingConnected_ISD_Unconnected_OutputOnLED_batteryLow();  // 기존
            // mappingConnected_ISD_connected_OutputOnLED_batteryLow(resetTimerEnable);  // 내부기 없으면 깜빡이게 변경
            break;

        case en__LED_BatteryChargingLevel_0per:
            batteryChargingLevelLED_veryLow(resetTimerEnable);
            break;

        case en__LED_BatteryChargingLevel_0btw20:
            batteryChargingLevelLED_0btw20(resetTimerEnable);
            break;

        case en__LED_BatteryChargingLevel_20btw40:
            batteryChargingLevelLED_20btw40(resetTimerEnable);
            break;

        case en__LED_BatteryChargingLevel_40btw60:
            batteryChargingLevelLED_40btw60(resetTimerEnable);
            break;

        case en__LED_BatteryChargingLevel_60btw80:
            batteryChargingLevelLED_60btw80(resetTimerEnable);
            break;

        case en__LED_BatteryChargingLevel_80btw100:
            batteryChargingLevelLED_80btw100(resetTimerEnable);
            break;

        case en__LED_BatteryChargingLevel_100per:
            batteryChargingLevelLED_Full(resetTimerEnable);
            break;

        case en__LED_NA:
        default:
            LED_black();
            break;
    }

    LED_OUT();
}
