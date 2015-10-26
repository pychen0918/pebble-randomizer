#ifndef __WAIT_H__
#define __WAIT_H__

#define WAIT_ANIMATION_TIMER_DELTA	33
#define WAIT_ANIMATION_BAR_LEFT_MARGIN	10
#define WAIT_ANIMATION_BAR_RIGHT_MARGIN	10
#define WAIT_ANIMATION_BAR_HEIGHT	6
#define WAIT_ANIMATION_BAR_RADIUS	(WAIT_ANIMATION_BAR_HEIGHT/2)
#define WAIT_WINDOW_TIMEOUT		25000	// Wait window should timeout in 25 seconds. js location looking has only 20 seconds timeout.
#ifdef PBL_ROUND
#define WAIT_BANNER_HEIGHT		44
#define WAIT_BANNER_TOP_MARGIN		12
#else
#define WAIT_BANNER_HEIGHT		32
#endif

void wait_window_push(void);
Window *get_wait_window(void);

#endif // #ifndef __WAIT_H__
