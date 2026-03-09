#include "timer.h"
#include "stm32wlxx_hal.h"

static TimerEvent_t *TimerListHead = NULL;

static void TimerRemove(TimerEvent_t *obj)
{
    TimerEvent_t *prev = NULL;
    TimerEvent_t *cur = TimerListHead;

    while (cur != NULL) {
        if (cur == obj) {
            if (prev == NULL) {
                TimerListHead = cur->Next;
            } else {
                prev->Next = cur->Next;
            }
            cur->Next = NULL;
            break;
        }
        prev = cur;
        cur = cur->Next;
    }
}

void TimerInit(TimerEvent_t *obj, void (*callback)(void *))
{
    if (obj == NULL) {
        return;
    }
    obj->Timestamp = 0U;
    obj->ReloadValue = 0U;
    obj->IsRunning = 0U;
    obj->Callback = callback;
    obj->Context = NULL;
    obj->Next = NULL;
}

void TimerSetValue(TimerEvent_t *obj, uint32_t value)
{
    if (obj == NULL) {
        return;
    }
    obj->ReloadValue = value;
}

void TimerStart(TimerEvent_t *obj)
{
    if ((obj == NULL) || (obj->IsRunning != 0U)) {
        return;
    }

    obj->Timestamp = TimerGetCurrentTime();
    obj->IsRunning = 1U;
    obj->Next = TimerListHead;
    TimerListHead = obj;
}

void TimerStop(TimerEvent_t *obj)
{
    if (obj == NULL) {
        return;
    }
    obj->IsRunning = 0U;
    TimerRemove(obj);
}

TimerTime_t TimerGetCurrentTime(void)
{
    return HAL_GetTick();
}

TimerTime_t TimerGetElapsedTime(TimerTime_t time)
{
    return (TimerTime_t)(HAL_GetTick() - time);
}

void TimerProcess(void)
{
    TimerEvent_t *cur = TimerListHead;
    TimerEvent_t *next = NULL;

    while (cur != NULL) {
        next = cur->Next;
        if ((cur->IsRunning != 0U) && (TimerGetElapsedTime(cur->Timestamp) >= cur->ReloadValue)) {
            cur->IsRunning = 0U;
            TimerRemove(cur);
            if (cur->Callback != NULL) {
                cur->Callback(cur->Context);
            }
        }
        cur = next;
    }
}
