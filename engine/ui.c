#include "engine/ui.h"
#include "engine/hash.h"
#include "game.h"
#include "player.h"
#include "errno.h"
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#define CHILD_COUNT_INCREMENT 32

extern struct game game;

const struct ui_res UIRES_BAD_ID = { -1, 0 };

struct {
	size_t count;
	struct ui_object *list;
	// list[0] is always the root!
} ui_objects;

const struct ui_descriptor desc_default_attributes = {
	.class = UIC_NONE,
	.anchor = (Vector2){ 0, 0 },
	.position = (UDim2){ { 0, 0 }, { 0, 0 } },
	.size = (UDim2){ { 200, 100 }, { 0, 0 } },
	.color = (Color){ 192, 192, 192, 0 },
	.transparency = 0,

	.invincible = 0,
	.visible = 1,
	.hidden = 0,

	._abs_position = (Vector2){ 0, 0 },
	._abs_size = (Vector2){ 0, 0 },
};

const struct uie_canvas canvas_default_attributes = {};

const struct uie_frame frame_default_attributes = {};

const struct uie_label label_default_attributes = {
	.text.color = BLACK,
	.text.transparency = 0,
	.text.font = UIF_DEFAULT,
	.text.font_size = 16,
	.text.size = 0,
	.text.string = NULL,
};

const struct uie_button button_default_attributes = {
	.text.color = BLACK,
	.text.transparency = 0,
	.text.font = UIF_DEFAULT,
	.text.font_size = 16,
	.text.size = 0,
	.text.string = NULL,
};

const struct uie_image image_default_attributes = {
	.img.tint = (Color){ 255, 255, 255, 255 },
	.img.transparency = 0,
	.img.tex = { 0 },
};

static struct ui_object *ui_get_root(void)
{
	return &ui_objects.list[0];
}

static unsigned calculate_id(const char *name)
{
	return hash_str(name);
}

static Vector2 get_screen_dim(void)
{
	const int w = GetScreenWidth();
	const int h = GetScreenHeight();
	return (Vector2){ (float)w, (float)h };
}

static _Bool id_exists(unsigned id)
{
	for (size_t i = 0; i < ui_objects.count; ++i)
		if (ui_objects.list[i].id == id)
			return 1;
	return 0;
}

static struct ui_object *add_entry(unsigned id, struct ui_descriptor *data)
{
	struct ui_object *this = &ui_objects.list[ui_objects.count++];
	this->id = id;
	this->child_count = 0;
	this->child_max = CHILD_COUNT_INCREMENT;
	this->children = malloc(this->child_max * sizeof(struct ui_object *));
	this->parent = NULL;
	this->data = data;
	return this;
}

static void remove_entry_at(size_t i)
{
	const size_t osize = sizeof(struct ui_object);
	struct ui_object *obj = &ui_objects.list[i];

	ui_objects.count--;

	const size_t count = ui_objects.count;

	if (i == count) {
		memset(obj, 0, osize);
		return;
	}

	memmove(obj - osize, obj, (count - i) * osize);
	memset(&ui_objects.list[count], 0, osize);
}

static void remove_entry(unsigned id)
{
	for (size_t i = 0; i < ui_objects.count; ++i) {
		struct ui_object *obj = &ui_objects.list[i];

		if (obj->id != id)
			continue;

		remove_entry_at(i);
		break;
	}
}

static int find_entry(unsigned id)
{
	for (size_t i = 0; i < ui_objects.count; ++i)
		if (ui_objects.list[i].id == id)
			return (int)i;
	return -1;
}

static void register_child(struct ui_object *parent, struct ui_object *child)
{
	// TODO Add error checking for child_count and dynamic allocation.
	parent->children[parent->child_count++] = child;
}

void ui_set_parent(struct ui_object *obj, struct ui_object *parent)
{
	if (obj == NULL || parent == NULL)
		return;

	obj->parent = parent;

	// TODO Update this to remove children, too.
	register_child(parent, obj);
}

void ui_set_text(struct ui_object *obj, const char *s)
{
	if (obj == NULL || s == NULL)
		return;

	const size_t len = strlen(s) + 1;
	struct ui_text *text = &obj->data->label.text;

	if (text->string != NULL)
		free(text->string);

	text->size = len;
	text->string = malloc(len);
	memcpy(text->string, s, len);
}

void ui_set_ftext(struct ui_object *obj, const char *f, ...)
{
	if (obj == NULL || f == NULL)
		return;

	va_list args;
	va_start(args, f);

	// Determine the length of the formatted string
	va_list args_copy;
	va_copy(args_copy, args);
	int len = vsnprintf(NULL, 0, f, args_copy);
	va_end(args_copy);

	if (len < 0) {
		va_end(args);
		return;
	}

	len++;

	struct ui_text *text = &obj->data->label.text;

	// FIXME Efficiency

	if (text->string != NULL)
		free(text->string);

	text->size = len;
	text->string = malloc(len);
	if (text->string == NULL) {
		va_end(args);
		return;
	}

	vsnprintf(text->string, len, f, args);
	va_end(args);
}

void ui_set_image(struct ui_object *obj, const char *filename)
{
	if (obj == NULL || filename == NULL)
		return;

	struct ui_image *img = &obj->data->image.img;

	if (img->tex.id != 0)
		UnloadTexture(img->tex);

	Image image = LoadImage(filename);
	img->tex = LoadTextureFromImage(image);
	UnloadImage(image);
}

static void set_default_class_attributes(struct ui_descriptor *descriptor)
{
	const void *list;
	size_t size;
	void *dest;

	switch (descriptor->class) {
	case UIC_CANVAS:
		list = &canvas_default_attributes;
		size = sizeof canvas_default_attributes;
		dest = &descriptor->canvas;
		break;
	case UIC_FRAME:
		list = &frame_default_attributes;
		size = sizeof frame_default_attributes;
		dest = &descriptor->frame;
		break;
	case UIC_LABEL:
		list = &label_default_attributes;
		size = sizeof label_default_attributes;
		dest = &descriptor->label;
		break;
	case UIC_BUTTON:
		list = &button_default_attributes;
		size = sizeof button_default_attributes;
		dest = &descriptor->button;
		break;
	case UIC_IMAGE:
		list = &image_default_attributes;
		size = sizeof image_default_attributes;
		dest = &descriptor->image;
		break;
	default:
		return;
	}

	memcpy(dest, list, size);
}

static void set_default_attributes(struct ui_descriptor *descriptor,
				   enum ui_class class)
{
	memcpy(descriptor, &desc_default_attributes, sizeof *descriptor);
	descriptor->class = class;

	set_default_class_attributes(descriptor);
}

static void setup_button_handlers(struct ui_descriptor *button)
{
	// TODO Register click area
}

static void initialize_descriptor(struct ui_descriptor *descriptor,
				  enum ui_class class)
{
	set_default_attributes(descriptor, class);

	switch (descriptor->class) {
	case UIC_BUTTON:
	case UIC_IMAGEBUTTON:
		setup_button_handlers(descriptor);
		break;
	default:
		break;
	}
}

struct ui_res ui_create(enum ui_class class, const char *name,
			struct ui_object *parent)
{
	const unsigned id = calculate_id(name);
	if (id_exists(id))
		return UIRES_BAD_ID;

	size_t data_size = sizeof(struct ui_descriptor);

	//   TODO: Make a large structure to index this type of data by class
	// id, instead of writing switch statements to hardcode them.

	switch (class) {
	case UIC_NONE:
		return (struct ui_res){ 0, NULL };
	case UIC_CANVAS:
		data_size += sizeof(struct uie_canvas);
		break;
	case UIC_FRAME:
		data_size += sizeof(struct uie_frame);
		break;
	case UIC_LABEL:
		data_size += sizeof(struct uie_label);
		break;
	case UIC_BUTTON:
		data_size += sizeof(struct uie_button);
		break;
	case UIC_IMAGE:
		data_size += sizeof(struct uie_image);
		break;
	case UIC_IMAGEBUTTON:
		return (struct ui_res){ -EINVAL, NULL };
	}

	struct ui_descriptor *descriptor = calloc(1, data_size);
	struct ui_object *obj = add_entry(id, descriptor);

	initialize_descriptor(descriptor, class);
	ui_set_parent(obj, parent);

	return (struct ui_res){ 0, obj };
}

static void free_class_data(struct ui_descriptor *descriptor)
{
	switch (descriptor->class) {
	case UIC_LABEL:
		free(descriptor->label.text.string);
		break;
	case UIC_IMAGE:
		//free(descriptor->image.img.data);
		break;
	case UIC_BUTTON:
		free(descriptor->button.text.string);
		break;
	default:
		break;
	}
}

static void ui_free(struct ui_object *obj)
{
	for (size_t i = 0; i < obj->child_count; ++i)
		ui_free(obj->children[i]);

	free_class_data(obj->data);
	//free(obj->children);
	free(obj->data);

	remove_entry(obj->id);
}

int ui_delete(const char *name)
{
	const unsigned id = calculate_id(name);
	const int idx = find_entry(id);

	if (idx < 0)
		return -ENOENT;

	struct ui_object *obj = &ui_objects.list[idx];
	struct ui_descriptor *data = obj->data;

	if (data->invincible)
		//   TODO Make this and similar checks a function,
		// so that the codebase does not depend
		// on those names which might change.
		return -EINVAL;

	ui_free(obj);

	return 0;
}

struct ui_object *ui_get(const char *by_name)
{
	const int idx = find_entry(calculate_id(by_name));
	if (idx < 0)
		return NULL;

	return &ui_objects.list[idx];
}

////////////
// DRAWING METHODS BELOW!
//
//   TODO Perhaps make a new file for specifically drawing out of descriptors?
//   TODO By the way since you're seeing this, we need panic methods too for
// the entire game.

//   TODO The issue with recalculate_absolute_size() and _position functions
// is that they are being updated just before rendering. So those absolute
// values are behind by 1 frame and reading them as a user can be useless.
// Try updating them in real time, such as by implementing set_position()
// and set_size() functions. Though, things will get slightly inconsistent.
//   Alternatively, we do not support reading those values by the user, and
// instead implement get_absolute_position() and get_absolute_size() functions
// that they can use on the objects instead. This is more preferable, as it
// avoids unnecessary calculations and does not reduce consistency as much.

//   TODO Add support for anchors.

static void recalculate_absolute_position(struct ui_descriptor *subject,
					  const struct ui_descriptor *parent)
{
	subject->_abs_position.x =
		parent->_abs_position.x +
		parent->_abs_size.x * subject->position.scale.x +
		subject->position.offset.x;
	subject->_abs_position.y =
		parent->_abs_position.y +
		parent->_abs_size.y * subject->position.scale.y +
		subject->position.offset.y;
}

static void recalculate_absolute_size(struct ui_descriptor *subject,
				      const struct ui_descriptor *parent)
{
	subject->_abs_size.x = parent->_abs_size.x * subject->size.scale.x +
			       subject->size.offset.x;
	subject->_abs_size.y = parent->_abs_size.y * subject->size.scale.y +
			       subject->size.offset.y;
}

static Color fix_color(Color color, float transparency)
{
	return (Color){ color.r, color.g, color.b,
			0xFF - (char)(transparency * 0xFF) };
}

static void draw_rect(const struct ui_descriptor *data)
{
	const Color color = fix_color(data->color, data->transparency);
	DrawRectangleV(data->_abs_position, data->_abs_size, color);
}

static void draw_text(const struct ui_text *text, Vector2 pos)
{
	const Color color = fix_color(text->color, text->transparency);
	DrawText(text->string, (int)pos.x, (int)pos.y, text->font_size, color);
}

static void draw_bound_text(const struct ui_text *text, Vector2 pos,
			    Vector2 bounds)
{
	// TODO Text auto-wrap, since it must be within bounds
	draw_text(text, pos);
}

static void draw_image(const struct ui_image *img, Vector2 pos, Vector2 size,
		       float transparency)
{
	const Texture2D tex = img->tex;
	const Color tint = fix_color(img->tint, transparency);
	const Rectangle src = {
		.x = 0,
		.y = 0,
		.width = (float)tex.width,
		.height = (float)tex.height,
	};
	const Rectangle dest = {
		.x = pos.x,
		.y = pos.y,
		.width = size.x,
		.height = size.y,
	};
	DrawTexturePro(tex, src, dest, (Vector2){ 0, 0 }, 0, tint);
}

static void ui_draw_frame(const struct ui_descriptor *data)
{
	draw_rect(data);
}

static void ui_draw_label(const struct ui_descriptor *data)
{
	draw_rect(data);
	draw_bound_text(&data->label.text, data->_abs_position,
			data->_abs_size);
}

static void ui_draw_button(const struct ui_descriptor *data)
{
	ui_draw_label(data);
}

static void ui_draw_image(const struct ui_descriptor *data)
{
	draw_image(&data->image.img, data->_abs_position, data->_abs_size,
		   data->transparency);
}

static void ui_draw_single(const struct ui_descriptor *data)
{
	switch (data->class) {
	case UIC_FRAME:
		ui_draw_frame(data);
		break;
	case UIC_LABEL:
		ui_draw_label(data);
		break;
	case UIC_BUTTON:
		ui_draw_button(data);
		break;
	case UIC_IMAGE:
		ui_draw_image(data);
		break;
	default:
		break;
	}
}

static void ui_draw_tree(const struct ui_object *obj,
			 const struct ui_descriptor *parent)
{
	if (!obj->data->visible)
		return;

	if (parent != NULL && !obj->data->hidden) {
		recalculate_absolute_position(obj->data, parent);
		recalculate_absolute_size(obj->data, parent);
		ui_draw_single(obj->data);
	}

	for (size_t i = 0; i < obj->child_count; ++i)
		ui_draw_tree(obj->children[i], obj->data);
}

void redraw_ui(void)
{
	const struct ui_object *root = ui_get_root();
	if (root == NULL)
		return;

	//   Recalculate root's size according to the window size, allowing
	// UI elements to scale dynamically.
	root->data->size.offset = root->data->_abs_size = get_screen_dim();

	ui_draw_tree(root, NULL);
}

static void ui_create_root(void)
{
	struct ui_object *root = ui_create(UIC_CANVAS, "root", NULL).object;
	root->data->size.offset = root->data->_abs_size = get_screen_dim();
}

static void ui_create_objlist(void)
{
	ui_objects.count = 0;
	ui_objects.list = malloc(sizeof *ui_objects.list * UI_MAX_OBJECTS);
}

void ui_init(void)
{
	ui_create_objlist();
	ui_create_root();

	struct ui_object *root = ui_get_root();

	struct ui_object *rrect = ui_create(UIC_FRAME, "red", root).object;
	rrect->data->position.offset = (Vector2){ 0, 0 };
	rrect->data->size = (UDim2){ { 230, 0 }, { 0, 1 } };
	rrect->data->color = RED;

	struct ui_object *grect = ui_create(UIC_FRAME, "green", rrect).object;
	grect->data->position.offset = (Vector2){ 140, 320 };
	grect->data->size.offset = (Vector2){ 280, 190 };
	grect->data->color = GREEN;

	struct ui_object *brect = ui_create(UIC_FRAME, "blue", root).object;
	brect->data->position.offset = (Vector2){ 80, 250 };
	brect->data->size.offset = (Vector2){ 140, 240 };
	brect->data->color = BLUE;

	struct ui_object *title = ui_create(UIC_LABEL, "title", root).object;
	title->data->position.offset = (Vector2){ 0, 0 };
	title->data->size = (UDim2){ { 0, 60 }, { 1, 0 } };
	title->data->transparency = 1;
	title->data->label.text.font_size = 46;
	title->data->label.text.color = ORANGE;
	ui_set_text(
		title,
		"Mathline [Still have to implement the line auto-wrapping]");

	const Color stat_color = RAYWHITE;
	const int stat_font_size = 20;

	struct ui_object *fps = ui_create(UIC_LABEL, "fps", root).object;
	fps->data->position.offset = (Vector2){ 10, 70 };
	fps->data->size = (UDim2){ { 0, 20 }, { 1, 0 } };
	fps->data->transparency = 1;
	fps->data->label.text.font_size = stat_font_size;
	fps->data->label.text.color = stat_color;

	struct ui_object *zoom = ui_create(UIC_LABEL, "zoom", root).object;
	zoom->data->position.offset = (Vector2){ 10, 90 };
	zoom->data->size = (UDim2){ { 0, 20 }, { 1, 0 } };
	zoom->data->transparency = 1;
	zoom->data->label.text.font_size = stat_font_size;
	zoom->data->label.text.color = stat_color;

	struct ui_object *target = ui_create(UIC_LABEL, "target", root).object;
	target->data->position.offset = (Vector2){ 10, 110 };
	target->data->size = (UDim2){ { 0, 20 }, { 1, 0 } };
	target->data->transparency = 1;
	target->data->label.text.font_size = stat_font_size;
	target->data->label.text.color = stat_color;

	struct ui_object *veloc = ui_create(UIC_LABEL, "veloc", root).object;
	veloc->data->position.offset = (Vector2){ 10, 130 };
	veloc->data->size = (UDim2){ { 0, 20 }, { 1, 0 } };
	veloc->data->transparency = 1;
	veloc->data->label.text.font_size = stat_font_size;
	veloc->data->label.text.color = stat_color;

	struct ui_object *testimg = ui_create(UIC_IMAGE, "dumimg", root).object;
	testimg->data->size.offset = (Vector2){ 400, 100 };
	testimg->data->position.offset = (Vector2){ 300, 100 };
	ui_set_image(testimg, "res/img/level_num_btn.png");
}

void update_stat_counters(void)
{
	//   NOTE: This is bad for performance; do not do this. Always keep a
	// reference to an object somewhere outside of the function, for quick
	// access. I am lazy and this will get rewritten anyway.
	struct ui_object *fps = ui_get("fps");
	struct ui_object *zoom = ui_get("zoom");
	struct ui_object *target = ui_get("target");
	struct ui_object *veloc = ui_get("veloc");

	ui_set_ftext(fps, "fps: %d", GetFPS());
	ui_set_ftext(zoom, "zoom: %.08f", game.camera.zoom);
	ui_set_ftext(target, "target: { %.04f, %.04f }", game.camera.target.x,
		     game.camera.target.y);
	ui_set_ftext(veloc, "velocity: { %.02f, %.02f }",
		     game.player->body.linear_velocity.x,
		     game.player->body.linear_velocity.y);
}
