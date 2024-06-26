#include "engine/window.h"
#include "game.h"
#include "perfgoals.h"
#include "gameconfig.h"
#include <malloc.h>
#include <raylib.h>

#define USE_MAXIMIZED_WINDOW 1

struct window window = { 0 };

static void wnd_init(void)
{
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | USE_MAXIMIZED_WINDOW ?
			       FLAG_WINDOW_MAXIMIZED :
			       0);
	InitWindow(SCREEN_W, SCREEN_H, GAME_NAME);
	SetTargetFPS(TARGET_FPS);

	window.screen_w = SCREEN_W;
	window.screen_h = SCREEN_H;

	game_init(&window);
}

static void wnd_mainloop(void)
{
	while (!WindowShouldClose())
		process_frame();
}

static void wnd_cleanup(void)
{
	CloseWindow();

	// TODO deallocation
}

int main(void)
{
	wnd_init();
	wnd_mainloop();

	wnd_cleanup();

	return 0;
}
