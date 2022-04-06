#include <ti/devices/msp432p4xx/driverlib/driverlib.h>
#include "ButtonLED_HAL.h"

// Comment out the following macro to switch between
// Timer32 and Timer_A.
#define TIMER32 1

// Modify the BLOCKING macro below to activate/deactivate
// the blocking code. Notice the difference in performance.
#define BLOCKING    0
#define BLOCK_COUNT 1000000

// This function initializes all the peripherals except graphics
void initialize();

#ifdef TIMER32

// The counter cycles based on a 3MHz source clock and a prescaler = 1
#define HALF_SEC_COUNT 1500000
#define ONE_SEC_COUNT  3000000
#define TWO_SEC_COUNT  6000000

// The length of the PWM period in terms of counter cycles
#define PWM_PERIOD_CYLCLES TWO_SEC_COUNT

// The duty cycle in fraction form (ratio between On cycles and Total number of cycles)
#define DUTY_CYCLE_FRACTION 0.5

// The length of on cycles and off cycles in terms of counter cycle
#define ON_CYCLES     (DUTY_CYCLE_FRACTION * PWM_PERIOD_CYLCLES)
#define OFF_CYCLES    ((1-DUTY_CYCLE_FRACTION) * PWM_PERIOD_CYLCLES)

typedef enum
{
    ON, OFF
} pwm_state_t;

// We are using TIMER32_0_BASE as the On timer
void startOnTimer()
{
    Timer32_setCount(TIMER32_0_BASE, ON_CYCLES);
    Timer32_startTimer(TIMER32_0_BASE, true);
}

bool OnTimerExpired()
{
    return (Timer32_getValue(TIMER32_0_BASE) == 0);
}

void startOffTimer()
{
    Timer32_setCount(TIMER32_1_BASE, OFF_CYCLES);
    Timer32_startTimer(TIMER32_1_BASE, true);
}

bool OffTimerExpired()
{
    return (Timer32_getValue(TIMER32_1_BASE) == 0);
}

int main(void)
{

    initialize();

    startOffTimer();
    pwm_state_t pwmState = OFF;
    volatile int i; // Volatile is used here to prevent compiler optimizations from getting rid of this variable.

    while (1)
    {

        // We are implementing a simple FSM for generating a PWM signal
        if ((pwmState == ON) && OnTimerExpired())
        {
            // Blocking operation; we turn on the Launchpad LED so that we can see it happening
            // The BLOCK_COUNT and BLOCKING macros control this blocking loop
            // This blocking code makes the "ON" state longer than it should be and disturbs the correct duty cycle
            turnOn_LaunchpadLED1();
            for (i = 0; i < (BLOCK_COUNT * BLOCKING); i++)
                ;
            turnOff_LaunchpadLED1();

            pwmState = OFF;

            toggle_BoosterpackLED_blue(); // The PWM is driving the Blue LED
            startOffTimer();
        }

        else if ((pwmState == OFF) && OffTimerExpired())
        {
            pwmState = ON;

            toggle_BoosterpackLED_blue(); //switching the blue LED on
            startOnTimer();
        }
    }
}

#else

//The number of counter cycles during 1s based on a source clock of 3MHz and the divider of 64
#define ONE_SEC_COUNT  46875

#define ONE_TENTH_SEC_COUNT  4687

#define ONE_HUNDREDTH_SEC_COUNT  469

// This is based on a 3MHz source clock and a clock divider of 64
#define PWM_PERIOD_CYLCLES ONE_HUNDREDTH_SEC_COUNT

// Duty cycle as a fraction (between 0 and 1)
#define DUTY_CYCLE_FRACTION 0.5

// The number of on cycles (Timer_A counter cycles)
#define BLUE_ON_CYCLES (DUTY_CYCLE_FRACTION * PWM_PERIOD_CYLCLES)

// The number of off cycles (Timer_A counter cycles)
#define BLUE_OFF_CYCLES ((1-DUTY_CYCLE_FRACTION) * PWM_PERIOD_CYLCLES)

// Since we start at "off" cycle, the first change comes once the number of "off" cycles are over
// With a different starting point or a different timer_A mode, this value could be different to achieve the same duty cycle
#define BLU_COMPARE_CYCLES    BLUE_OFF_CYCLES

// The below set of choices are not programmer's.
// We need to study the Booster board and Launchpad board diagrams for this info.

// The BLUE LED is wired to TA2.1.
// As the programmer we cannot change this. It is decided by the board designer
#define BLU_TIMER    TIMER_A2_BASE
#define BLU_CHANNEL  TIMER_A_CAPTURECOMPARE_REGISTER_1

void initPWM()
{
    // For the Blue LED, we first configure it as a GPIO and turn it off
    // Then, we configure it to work as output channel of a timer
    // The blue LED on booster is wried to Port 5, Pin 6, we cannot change this as the programmer
    // BLUE LED
    GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_setAsPeripheralModuleFunctionOutputPin(
    GPIO_PORT_P5,
                                                GPIO_PIN6,
                                                GPIO_PRIMARY_MODULE_FUNCTION);
}

int main(void)
{

    // Stop WDT
    WDT_A_holdTimer();

    initialize();

    Timer_A_PWMConfig pwmConfig_blu;

    // The source clock is the system clock (3MHz)
    pwmConfig_blu.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;

    // The clock divider is 64
    pwmConfig_blu.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_64;

    // The # of counter cycles in one PWM round (period of PWM in terms of counter cycle)
    pwmConfig_blu.timerPeriod = PWM_PERIOD_CYLCLES;

    // The OC mode. We choose this such that we can achieve the waveform we are interested in.
    pwmConfig_blu.compareOutputMode = TIMER_A_OUTPUTMODE_SET_RESET;

    // The OC value. This is the value that when the counter hits it, something happens to the waveform
    // Since we chose SET_RESET, at this count the GPIO is set and the LED is turned on
    // This means the LED was off until hitting this count and therefore, it is the off-cycle count for us
    pwmConfig_blu.dutyCycle = BLU_COMPARE_CYCLES;

    // The output register used for OC operation.
    // The programmer has to choose this such that it feeds to the right pin connected to the device needing the PWM.
    pwmConfig_blu.compareRegister = BLU_CHANNEL;

    initPWM();

    // The below line starts generating the pulse
    Timer_A_generatePWM(BLU_TIMER, &pwmConfig_blu);

    volatile int i; // Volatile is used here to prevent compiler optimizations from getting rid of this variable.

    while (1)
    {
        // The blocking code.
        turnOn_LaunchpadLED1();
        for (i = 0; i < (BLOCK_COUNT * BLOCKING); i++)
            ;
        turnOff_LaunchpadLED1();

        // Nothing needs to be done here for handling the pulse as the Timer_A automatically takes care of it and the processor is hands off.
    }

}

#endif

void initialize()
{
    // stop the watchdog timer
    WDT_A_hold(WDT_A_BASE);

    Timer32_initModule(TIMER32_0_BASE, // There are two timers, we are using the one with the index 0
            TIMER32_PRESCALER_1, // The prescaler value is 1; The clock is not divided before feeding the counter
            TIMER32_32BIT, // The counter is used in 32-bit mode; the alternative is 16-bit mode
            TIMER32_PERIODIC_MODE); //This options is irrelevant for a one-shot timer

    Timer32_initModule(TIMER32_1_BASE, // There are two timers, we are using the one with the index 1
            TIMER32_PRESCALER_1, // The prescaler value is 1; The clock is not divided before feeding the counter
            TIMER32_32BIT, // The counter is used in 32-bit mode; the alternative is 16-bit mode
            TIMER32_PERIODIC_MODE); //This options is irrelevant for a one-shot timer

    initialize_BoosterpackLED_blue();
    turnOff_BoosterpackLED_blue();

    initialize_LaunchpadLED2_green();
    turnOff_LaunchpadLED2_green();

    initialize_LaunchpadLED1();
    turnOff_LaunchpadLED1();

}
