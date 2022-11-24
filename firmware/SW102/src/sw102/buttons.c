#include "buttons.h"
#include "button.h"
#include "ui.h"
#include "main.h"

// this is a replacement for common/buttons.c
// this better suits our UI logic

static unsigned int events;

buttons_events_t buttons_get_events(void)
{
	return (buttons_events_t)events;
}

void buttons_clear_all_events(void){
	events = 0;
}

// 20ms

#define LONGCLICK_THRESHOLD (1000/20)

void buttons_clock(void)
{
	static int oldstate = 0;
	static int hold[4];
	int state = 	(PollButton(&buttonUP)?2:0)|
			(PollButton(&buttonDWN)?4:0)|
			(PollButton(&buttonM)?8:0)|
			(PollButton(&buttonPWR)?1:0);

	int press = state & ~oldstate, release = oldstate & ~state;
	
	for(int i=0;i<4;i++) {
		int bit = 1<<i;
	
		if(press & bit) 
			events|=ONOFF_PRESS << i;

		if(state & bit) {
			hold[i]++;
			if(hold[i] == LONGCLICK_THRESHOLD)
				events |= ONOFF_LONG_CLICK<<i;
		}

		if(release & bit) {
			events|=ONOFF_RELEASE << i;
			if(hold[i] < LONGCLICK_THRESHOLD)
				events |= ONOFF_CLICK<<i;
			hold[i]=0;
		}
	}

	oldstate = state;
}
