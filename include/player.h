#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <raylib.h>

struct player {
	Vector2 pos, old_pos;
	float radius, rotation;

	Texture2D tex;
	Color tint;

	struct {
		Vector2 linear_velocity, linear_accel;
		Vector2 force;
		float moment_of_inertia, torque;
		float angular_velocity;
		Vector2 debug;
	} body;
};

_Bool player_collides(Vector2 *point);
void player_init(void);

#endif
