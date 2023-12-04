
/*
Possible features:
	- Have some small number of colors to pick from. Maybe get the color selection thingy by pressing the right mouse button?
		- Could have certain # of discrete colors or a continuous gradient.

	- User should be able to "make" their own colors.
	- Undo (everything from button-down -> button-up could be considered "1 unit of undo")
	- Not erasing everything when window is resized.
	- Saving the drawing into a file.
	- Opening a drawing from a file.
	- Handle x-button close window correctly (assuming we arent already)

Possible design decisions:
	* We could store the state of the drawing in some data structure. As we only draw circles currently, it would just be a matter of keeping an array of circles and their associated colors.

	* We could keep a pixel buffer and draw into that sort of manually, then blit into the window. (Could use mit-shm extension to speed this up)
*/

#include <stdint.h>

struct Theme {
	uint32_t background_color, foreground_color;
};

struct Point {
	int32_t x, y;
};

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <xcb/xcb.h>
#include <unistd.h>
#include <float.h>

xcb_connection_t *connection;
xcb_window_t window;
xcb_gcontext_t graphics_context;

uint32_t LINE_WIDTH = 8;

int32_t window_width = 600;
int32_t window_height = 600;

enum ThemeName {
	DARK_THEME,
	LIGHT_THEME,
	ORANGE
};

void set_theme(struct Theme *theme, enum ThemeName theme_name){
	switch(theme_name){
		case DARK_THEME: {
			theme->background_color = 0x101010;
			theme->foreground_color = 0xefefef;
			break;
		}
		case LIGHT_THEME: {
			theme->background_color = 0xffffff;
			theme->foreground_color = 0x000000;
			break;
		}
		default:
			assert(false);
	}
}

int main()
{
	struct Theme current_theme;
	set_theme(&current_theme, DARK_THEME);

	connection = xcb_connect(NULL, NULL);

	const xcb_setup_t *setup = xcb_get_setup(connection);
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
	xcb_screen_t *screen = iter.data;

	window = xcb_generate_id(connection);
	uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	uint32_t event_mask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION;
	uint32_t values[] = {current_theme.background_color, event_mask};
	xcb_create_window(
		connection,
		XCB_COPY_FROM_PARENT,
		window,
		screen->root,
		0, 0,
		window_width, window_height,
		0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		screen->root_visual,
		mask, values);

	const char *title = "Draw";
	xcb_change_property(
		connection,
		XCB_PROP_MODE_REPLACE,
		window,
		XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING,
		8,
		strlen(title),
		title);

	xcb_map_window(connection, window);


	// uint32_t color = 0x00ff00ff; // -RGB
	{
		graphics_context = xcb_generate_id(connection);
		uint32_t gc_mask = XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH | XCB_GC_CAP_STYLE;
		uint32_t gc_values[] = {current_theme.foreground_color, LINE_WIDTH, XCB_CAP_STYLE_ROUND};
		xcb_create_gc(connection, graphics_context, screen->root, gc_mask, gc_values);
	}

	// xcb_gcontext_t rectangle_gc;
	// {
	// 	rectangle_gc = xcb_generate_id(connection);
	// 	uint32_t gc_mask = XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH | XCB_GC_CAP_STYLE;
	// 	uint32_t gc_values[] = {0xff0000, LINE_WIDTH, XCB_CAP_STYLE_ROUND};
	// 	xcb_create_gc(connection, rectangle_gc, screen->root, gc_mask, gc_values);
	// }

	xcb_flush(connection);

	bool mouse_left_button_down = false;
	xcb_point_t prev_point;

	while(true) {
		xcb_generic_event_t *event = xcb_wait_for_event(connection);
		if(event){
			// printf("response type: %d\n", event->response_type);
			switch(event->response_type & ~0x80){
				case 0:{
					printf("error\n");
					break;
				}
				case XCB_EXPOSE: {
					printf("expose event\n");

					break;
				}
				case XCB_BUTTON_PRESS: {
					xcb_button_press_event_t *bp = (xcb_button_press_event_t *)event;

					if(bp->detail == 1){
						// left mouse button
						mouse_left_button_down = true;

						prev_point.x = bp->event_x;
						prev_point.y = bp->event_y;
					}else if(bp->detail == 3){
						// right mouse button
						printf("right mouse button press\n");


						// draw a rectangle

						{
							uint32_t props = XCB_GC_FOREGROUND;
							uint32_t values[] = {0x0000ff};
							xcb_change_gc(connection, graphics_context, props, values);
						}

						uint32_t rectangles_len = 1;
						xcb_rectangle_t rectangles[] = {
							{bp->event_x, bp->event_y, 100, 100}
						};
						xcb_poly_fill_rectangle(
							connection,
							window,
							graphics_context,
							rectangles_len, rectangles);

						{
							uint32_t props = XCB_GC_FOREGROUND;
							uint32_t values[] = {current_theme.foreground_color};
							xcb_change_gc(connection, graphics_context, props, values);
						}

						xcb_flush(connection);
					}else{
						assert(false);
					}
					break;
				}
				case XCB_BUTTON_RELEASE: {
					xcb_button_release_event_t *br = (xcb_button_release_event_t *)event;

					if(br->detail == 1){
						// left mouse button
						mouse_left_button_down = false;

						xcb_point_t points[] = {
							{prev_point.x, prev_point.y},
							{br->event_x, br->event_y}
						};
						xcb_poly_line(connection, XCB_COORD_MODE_ORIGIN, window, graphics_context, 2, points);
						xcb_flush(connection);
					}else if(br->detail == 3){
						// right mouse button
						printf("right mouse button release\n");
					}else{
						assert(false);
					}
					break;
				}
				case XCB_MOTION_NOTIFY: {
					xcb_motion_notify_event_t *mn = (xcb_motion_notify_event_t *)event;

					if(mouse_left_button_down){
						xcb_point_t points[] = {
							{prev_point.x, prev_point.y},
							{mn->event_x, mn->event_y}
						};
						xcb_poly_line(connection, XCB_COORD_MODE_ORIGIN, window, graphics_context, 2, points);
						xcb_flush(connection);

						prev_point.x = mn->event_x;
						prev_point.y = mn->event_y;
					}

					break;
				}
				default: {
					printf("unknown event\n");
				}
			}
			free(event);
		}else{
			printf("I/O error!\n");
			break; // I/0 error (docs)
		}
	}

	// pause();
	// xcb_disconnect(connection);
	return 0;
}