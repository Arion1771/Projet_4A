#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t TimerTime_t;

typedef struct TimerEvent_s
{
  TimerTime_t Timestamp;
  TimerTime_t ReloadValue;
  uint8_t IsRunning;
  void (*Callback)(void *);
  void *Context;
  struct TimerEvent_s *Next;
} TimerEvent_t;

void TimerInit(TimerEvent_t *obj, void (*callback)(void *));
void TimerSetValue(TimerEvent_t *obj, uint32_t value);
void TimerStart(TimerEvent_t *obj);
void TimerStop(TimerEvent_t *obj);
TimerTime_t TimerGetCurrentTime(void);
TimerTime_t TimerGetElapsedTime(TimerTime_t time);
void TimerProcess(void);

#endif /* TIMER_H */
