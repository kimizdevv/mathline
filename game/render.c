#include "engine/render.h"
#include "graph.h"
#include "player.h"
#include <rlgl.h>
#include <raymath.h>
#include <malloc.h>

extern struct game game;

static struct {
	Color graph_color;
} rctx;

static float __f(float x)
{
	//return sinf(powf(x, x)) * x;
	return sinf(x);
}

static float __g(float x)
{
	return atanf(x);
}

void render_init(void)
{
	rctx.graph_color = RED;

	game.ngraphs = 1;
	game.graphs = malloc(game.ngraphs * sizeof(graph_t));

	game.graphs[0] = __f;
	//game.graphs[1] = __g;
}

void render(void)
{
	for (int i = 0; i < game.ngraphs; ++i)
		render_fgraph(game.graphs[i], i % 2 == 0 ? RED : BLUE);

	const struct player *player = game.player;
	DrawCircleV(player->pos, player->radius, player->color);
}
