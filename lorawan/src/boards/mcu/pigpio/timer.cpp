#if defined TARGET_PIGPIO
#include "boards/mcu/timer.h"
#include "boards/mcu/board.h"

#include <pigpio.h>

static bool timerInUse[10] = {false, false, false, false, false, false, false, false, false, false};
static bool timerTerminate[10] = {false, false, false, false, false, false, false, false, false, false};
static bool timerSelfStart[10] = {false, false, false, false, false, false, false, false, false, false};
static pthread_t timerThread[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// External functions

void TimerConfig(void)
{
	/// \todo Nothing to do here for RPI/pigpio
}

void TimerInit(TimerEvent_t *obj, void (*callback)(void))
{
	// Look for an available Ticker
	for (int idx = 0; idx < 10; idx++)
	{
		if (timerInUse[idx] == false)
		{
			timerInUse[idx] = true;
			timerTerminate[idx] = false;
			obj->timerNum = idx;
			obj->Callback = callback;
			return;
		}
	}
	LOG_LIB("TIM", "No more timers available!");
	/// \todo We run out of tickers, what do we do now???
}

static void timerCallback(TimerEvent_t * obj)
{
	int idx = obj->timerNum;
	timerThread[idx] = pthread_self();
	obj->Callback();
	timerThread[idx] = 0;

	// NOTE: if one-shot, this will TERMINATE this execution thread. Also if
	// the timer was stopped and not restarted by its own callback.
	bool selfStart = timerSelfStart[idx];
	timerSelfStart[idx] = false;
	if ((obj->oneShot || timerTerminate[idx]) && !selfStart) gpioSetTimerFunc(idx, 0, NULL);
}

void TimerStart(TimerEvent_t *obj)
{
	int idx = obj->timerNum;
	timerTerminate[idx] = false;
	int r = gpioSetTimerFuncEx(idx, obj->ReloadValue, (gpioTimerFuncEx_t)&timerCallback, obj);
	if (r != 0) {
		fprintf(stderr, "TimerStart: gpioSetTimerFuncEx() fail: %d\n", r);
	}
	if (obj->oneShot && pthread_equal(pthread_self(), timerThread[idx])) {
		// TimerStart called from inside callback. Even if timer is one-shot,
		// callback should be run at least once again.
		timerSelfStart[idx] = true;
	}
}

void TimerStop(TimerEvent_t *obj)
{
	int idx = obj->timerNum;
	if (pthread_equal(pthread_self(), timerThread[idx])) {
		// Protect against unintended self-termination
		timerTerminate[idx] = true;
	} else {
		gpioSetTimerFunc(idx, 0, NULL);
	}
	timerSelfStart[idx] = false;
}

void TimerReset(TimerEvent_t *obj)
{
	int idx = obj->timerNum;
	if (!pthread_equal(pthread_self(), timerThread[idx])) {
		// Cancel current timer interval
		gpioSetTimerFunc(idx, 0, NULL);
	}
	timerTerminate[idx] = false;
	int r = gpioSetTimerFuncEx(idx, obj->ReloadValue, (gpioTimerFuncEx_t)&timerCallback, obj);
	if (r != 0) {
		fprintf(stderr, "TimerReset: gpioSetTimerFuncEx() fail: %d\n", r);
	}
	if (obj->oneShot && pthread_equal(pthread_self(), timerThread[idx])) {
		// TimerReset called from inside callback. Even if timer is one-shot,
		// callback should be run at least once again.
		timerSelfStart[idx] = true;
	}
}

void TimerSetValue(TimerEvent_t *obj, uint32_t value)
{
	obj->ReloadValue = value;
}

TimerTime_t TimerGetCurrentTime(void)
{
	return millis();
}

TimerTime_t TimerGetElapsedTime(TimerTime_t past)
{
	uint32_t nowInTicks = millis();
	uint32_t pastInTicks = past;
	TimerTime_t diff = nowInTicks - pastInTicks;

	return diff;
}

#endif
