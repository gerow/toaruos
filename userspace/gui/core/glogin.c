/* This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2013-2014 Kevin Lange
 */
/*
 * glogin
 *
 * Graphical Login screen
 */
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include <cairo.h>

#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "lib/graphics.h"
#include "lib/shmemfonts.h"
#include "lib/kbd.h"
#include "lib/yutani.h"
#include "lib/toaru_auth.h"

#include "gui/ttk/ttk.h"

sprite_t logo;

gfx_context_t * ctx;

uint16_t win_width;
uint16_t win_height;

int uid = 0;

#define LOGO_FINAL_OFFSET 100
#define BOX_WIDTH  272
#define BOX_HEIGHT 104
#define USERNAME_BOX 1
#define PASSWORD_BOX 2
#define EXTRA_TEXT_OFFSET 15
#define TEXTBOX_INTERIOR_LEFT 4
#define LEFT_OFFSET 84

int center_x(int x) {
	return (win_width - x) / 2;
}

int center_y(int y) {
	return (win_height - y) / 2;
}

#define INPUT_SIZE 1024
int buffer_put(char * input_buffer, char c) {
	int input_collected = strlen(input_buffer);
	if (c == 8) {
		/* Backspace */
		if (input_collected > 0) {
			input_collected--;
			input_buffer[input_collected] = '\0';
		}
		return 0;
	}
	if (c < 10 || (c > 10 && c < 32) || c > 126) {
		return 0;
	}
	input_buffer[input_collected] = c;
	input_collected++;
	input_buffer[input_collected] = '\0';
	if (input_collected == INPUT_SIZE - 1) {
		return 1;
	}
	return 0;
}

int32_t min(int32_t a, int32_t b) {
	return (a < b) ? a : b;
}

int32_t max(int32_t a, int32_t b) {
	return (a > b) ? a : b;
}

void draw_box(gfx_context_t * ctx, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
	int32_t _min_x = max(x, 0);
	int32_t _min_y = max(y,  0);
	int32_t _max_x = min(x + w - 1, ctx->width  - 1);
	int32_t _max_y = min(y + h - 1, ctx->height - 1);

	for (int i = _min_y; i < _max_y; ++i) {
		draw_line(ctx, _min_x, _max_x, i, i, color);
	}
}

void draw_box_border(gfx_context_t * ctx, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
	int32_t _min_x = max(x, 0);
	int32_t _min_y = max(y,  0);
	int32_t _max_x = min(x + w - 1, ctx->width  - 1);
	int32_t _max_y = min(y + h - 1, ctx->height - 1);

	draw_line(ctx, _min_x, _max_x, _min_y, _min_y, color);
	draw_line(ctx, _min_x, _max_x, _max_y, _max_y, color);
	draw_line(ctx, _min_x, _min_x, _min_y, _max_y, color);
	draw_line(ctx, _max_x, _max_x, _min_y, _max_y, color);
}

struct text_box {
	int x;
	int y;
	unsigned int width;
	unsigned int height;

	uint32_t text_color;

	struct login_container * parent;

	int is_focused:1;
	int is_password:1;

	unsigned int cursor;
	char * buffer;

	char * placeholder;
};

struct login_container {
	int x;
	int y;
	unsigned int width;
	unsigned int height;

	struct text_box * username_box;
	struct text_box * password_box;

	int show_error:1;
};

void draw_text_box(cairo_t * cr, struct text_box * tb) {
	int x = tb->parent->x + tb->x;
	int y = tb->parent->y + tb->y;

	set_font_size(13);
	int text_offset = 15;

	cairo_rounded_rectangle(cr, 1 + x, 1 + y, tb->width - 2, tb->height - 2, 2.0);
	if (tb->is_focused) {
		cairo_set_source_rgba(cr, 8.0/255.0, 193.0/255.0, 236.0/255.0, 1.0);
	} else {
		cairo_set_source_rgba(cr, 158.0/255.0, 169.0/255.0, 177.0/255.0, 1.0);
	}
	cairo_set_line_width(cr, 2);
	cairo_stroke(cr);

	{
		cairo_pattern_t * pat = cairo_pattern_create_linear(1 + x, 1 + y, 1 + x, 1 + y + tb->height - 2);
		if (tb->is_focused) {
			cairo_pattern_add_color_stop_rgba(pat, 0, 241.0/255.0, 241.0/255.0, 244.0/255.0, 1.0);
			cairo_pattern_add_color_stop_rgba(pat, 1, 1, 1, 1, 1.0);
		} else {
			cairo_pattern_add_color_stop_rgba(pat, 0, 241.0/255.0, 241.0/255.0, 244.0/255.0, 0.9);
			cairo_pattern_add_color_stop_rgba(pat, 1, 1, 1, 1, 0.9);
		}
		cairo_rounded_rectangle(cr, 1 + x, 1 + y, tb->width - 2, tb->height - 2, 2.0);
		cairo_set_source(cr, pat);
		cairo_fill(cr);
		cairo_pattern_destroy(pat);
	}

	char * text = tb->buffer;
	char password_circles[512];
	uint32_t color = tb->text_color;

	if (strlen(tb->buffer) == 0 && !tb->is_focused) {
		text = tb->placeholder;
		color = rgba(0,0,0,127);
	} else if (tb->is_password) {
		strcpy(password_circles, "");
		for (int i = 0; i < strlen(tb->buffer); ++i) {
			strcat(password_circles, "⚫");
		}
		text = password_circles;
	}

	draw_string(ctx, x + TEXTBOX_INTERIOR_LEFT, y + text_offset, color, text);

	if (tb->is_focused) {
		int width = draw_string_width(text);
		draw_line(ctx, x + TEXTBOX_INTERIOR_LEFT + width, x + TEXTBOX_INTERIOR_LEFT + width, y + 2, y + text_offset + 1, tb->text_color);
	}

}

void draw_login_container(cairo_t * cr, struct login_container * lc) {

	/* Draw rounded rectangle */
	cairo_rounded_rectangle(cr, lc->x, lc->y, lc->width, lc->height, 4.0);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.75);
	cairo_fill(cr);

	/* Draw labels */
	if (lc->show_error) {
		char * error_message = "Incorrect username or password.";

		set_font_size(11);
		draw_string(ctx, lc->x + (lc->width - draw_string_width(error_message)) / 2, lc->y + 6 + EXTRA_TEXT_OFFSET, rgb(240, 20, 20), error_message);
	}

	draw_text_box(cr, lc->username_box);
	draw_text_box(cr, lc->password_box);

}

int main (int argc, char ** argv) {
	load_sprite_png(&logo, "/usr/share/logo_login.png");
	init_shmemfonts();

	yutani_t * y = yutani_init();

	if (!y) {
		fprintf(stderr, "[glogin] Connection to server failed.\n");
		return 1;
	}

	/* Generate surface for background */
	sprite_t * bg_sprite;

	int width  = y->display_width;
	int height = y->display_height;

	win_width = width;
	win_height = height;

	/* Do something with a window */
	yutani_window_t * wina = yutani_window_create(y, width, height);
	assert(wina);
	yutani_set_stack(y, wina, 0);
	ctx = init_graphics_yutani_double_buffer(wina);
	draw_fill(ctx, rgba(0,0,0,255));
	yutani_flip(y, wina);

	cairo_surface_t * cs = cairo_image_surface_create_for_data((void*)ctx->backbuffer, CAIRO_FORMAT_ARGB32, ctx->width, ctx->height, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, ctx->width));
	cairo_t * cr = cairo_create(cs);

	{
		sprite_t * wallpaper = malloc(sizeof(sprite_t));
		load_sprite_png(wallpaper, "/usr/share/wallpaper.png");

		float x = (float)width  / (float)wallpaper->width;
		float y = (float)height / (float)wallpaper->height;

		int nh = (int)(x * (float)wallpaper->height);
		int nw = (int)(y * (float)wallpaper->width);;

		bg_sprite = create_sprite(width, height, ALPHA_OPAQUE);
		gfx_context_t * bg = init_graphics_sprite(bg_sprite);

		if (nw > width) {
			draw_sprite_scaled(bg, wallpaper, (width - nw) / 2, 0, nw, height);
		} else {
			draw_sprite_scaled(bg, wallpaper, 0, (height - nh) / 2, width, nh);
		}

		/* Three box blurs = good enough approximation of a guassian, but faster*/
		blur_context_box(bg, 20);
		blur_context_box(bg, 20);
		blur_context_box(bg, 20);

		free(bg);
		free(wallpaper);
	}

	while (1) {

		yutani_set_stack(y, wina, 0);
		yutani_focus_window(y, wina->wid);

		draw_fill(ctx, rgb(0,0,0));
		draw_sprite(ctx, bg_sprite, center_x(width), center_y(height));
		flip(ctx);
		yutani_flip(y, wina);

		char * foo = malloc(sizeof(uint32_t) * width * height);
		memcpy(foo, ctx->backbuffer, sizeof(uint32_t) * width * height);

		for (int i = 0; i < LOGO_FINAL_OFFSET; i += 2) {
			memcpy(ctx->backbuffer, foo, sizeof(uint32_t) * width * height);
			draw_sprite(ctx, &logo, center_x(logo.width), center_y(logo.height) - i);
			flip(ctx);
			yutani_flip_region(y, wina, center_x(logo.width), center_y(logo.height) - i, logo.width, logo.height + 5);
			usleep(10000);
		}

		size_t buf_size = wina->width * wina->height * sizeof(uint32_t);
		char * buf = malloc(buf_size);

		uint32_t i = 0;

		uint32_t black = rgb(0,0,0);
		uint32_t white = rgb(255,255,255);

		int x_offset = 65;
		int y_offset = 64;

		int fuzz = 3;

		char username[INPUT_SIZE] = {0};
		char password[INPUT_SIZE] = {0};
		char hostname[512];

		{
			char _hostname[256];
			syscall_gethostname(_hostname);

			struct tm * timeinfo;
			struct timeval now;
			gettimeofday(&now, NULL); //time(NULL);
			timeinfo = localtime((time_t *)&now.tv_sec);

			char _date[256];
			strftime(_date, 256, "%a %B %d %Y", timeinfo);

			sprintf(hostname, "%s // %s", _hostname, _date);
		}

		char kernel_v[512];

		{
			struct utsname u;
			uname(&u);
			/* UTF-8 Strings FTW! */
			uint8_t * os_name_ = "とあるOS";
			uint32_t l = snprintf(kernel_v, 512, "%s %s", os_name_, u.release);
		}

		uid = 0;

		int box_x = center_x(BOX_WIDTH);
		int box_y = center_y(0) + 8;

		int focus = 0;

		set_font_size(11);
		int hostname_label_left = width - 10 - draw_string_width(hostname);
		int kernel_v_label_left = 10;

		struct text_box username_box = { (BOX_WIDTH - 170) / 2, 30, 170, 20, rgb(0,0,0), NULL, 0, 0, 0, username, "Username" };
		struct text_box password_box = { (BOX_WIDTH - 170) / 2, 58, 170, 20, rgb(0,0,0), NULL, 0, 1, 0, password, "Password" };

		struct login_container lc = { box_x, box_y, BOX_WIDTH, BOX_HEIGHT, &username_box, &password_box, 0 };

		username_box.parent = &lc;
		password_box.parent = &lc;

		while (1) {
			focus = 0;
			memset(username, 0x0, INPUT_SIZE);
			memset(password, 0x0, INPUT_SIZE);

			while (1) {

				memcpy(ctx->backbuffer, foo, sizeof(uint32_t) * width * height);
				draw_sprite(ctx, &logo, center_x(logo.width), center_y(logo.height) - LOGO_FINAL_OFFSET);

				set_font_size(11);
				draw_string_shadow(ctx, hostname_label_left, height - 12, white, hostname, rgb(0,0,0), 2, 1, 1, 3.0);
				draw_string_shadow(ctx, kernel_v_label_left, height - 12, white, kernel_v, rgb(0,0,0), 2, 1, 1, 3.0);

				if (focus == USERNAME_BOX) {
					username_box.is_focused = 1;
					password_box.is_focused = 0;
				} else if (focus == PASSWORD_BOX) {
					username_box.is_focused = 0;
					password_box.is_focused = 1;
				} else {
					username_box.is_focused = 0;
					password_box.is_focused = 0;
				}

				draw_login_container(cr, &lc);

				flip(ctx);
				yutani_flip(y, wina);

				struct yutani_msg_key_event kbd;
				struct yutani_msg_window_mouse_event mou;
				int msg_type = 0;
				do {
					yutani_msg_t * msg = yutani_poll(y);
					switch (msg->type) {
						case YUTANI_MSG_KEY_EVENT:
							{
								struct yutani_msg_key_event * ke = (void*)msg->data;
								if (ke->event.action == KEY_ACTION_DOWN) {
									memcpy(&kbd, ke, sizeof(struct yutani_msg_key_event));
									msg_type = 1;
								}
							}
							break;
						case YUTANI_MSG_WINDOW_MOUSE_EVENT:
							{
								struct yutani_msg_window_mouse_event * me = (void*)msg->data;
								memcpy(&mou, me, sizeof(struct yutani_msg_mouse_event));
								msg_type = 2;
							}
							break;
					}
					free(msg);
				} while (!msg_type);

				if (msg_type == 1) {

					if (kbd.event.keycode == '\n') {
						if (focus == USERNAME_BOX) {
							focus = PASSWORD_BOX;
							continue;
						} else if (focus == PASSWORD_BOX) {
							break;
						} else {
							focus = USERNAME_BOX;
							continue;
						}
					}

					if (kbd.event.keycode == '\t') {
						if (focus == USERNAME_BOX) {
							focus = PASSWORD_BOX;
						} else {
							focus = USERNAME_BOX;
						}
						continue;
					}

					if (kbd.event.key) {

						if (!focus) {
							focus = USERNAME_BOX;
						}

						if (focus == USERNAME_BOX) {
							buffer_put(username, kbd.event.key);
						} else if (focus == PASSWORD_BOX) {
							buffer_put(password, kbd.event.key);
						}

					}

				} else if (msg_type == 2) {

					if ((mou.command == YUTANI_MOUSE_EVENT_DOWN
					     && mou.buttons & YUTANI_MOUSE_BUTTON_LEFT)
					    || (mou.command == YUTANI_MOUSE_EVENT_CLICK)) {
						/* Determine if we were inside of a text box */

						if (mou.new_x >= lc.x + username_box.x &&
						    mou.new_x <= lc.x + username_box.x + username_box.width &&
						    mou.new_y >= lc.y + username_box.y &&
						    mou.new_y <= lc.y + username_box.y + username_box.height) {
							/* Ensure this box is focused. */
							focus = USERNAME_BOX;
							continue;
						} else if (mou.new_x >= lc.x + password_box.x &&
						    mou.new_x <= lc.x + password_box.x + password_box.width &&
						    mou.new_y >= lc.y + password_box.y &&
						    mou.new_y <= lc.y + password_box.y + password_box.height) {
							/* Ensure this box is focused. */
							focus = PASSWORD_BOX;
							continue;
						} else {
							focus = 0;
							continue;
						}

					}

				}

			}

			uid = toaru_auth_check_pass(username, password);

			if (uid >= 0) {
				break;
			}
			lc.show_error = 1;
		}

		memcpy(ctx->backbuffer, foo, sizeof(uint32_t) * width * height);
		flip(ctx);
		yutani_flip(y, wina);

		pid_t _session_pid = fork();
		if (!_session_pid) {
			setuid(uid);
			toaru_auth_set_vars();
			char * args[] = {"/bin/gsession", NULL};
			execvp(args[0], args);
		}

		free(foo);
		free(buf);

		waitpid(_session_pid, NULL, 0);
	}

	yutani_close(y, wina);


	return 0;
}
