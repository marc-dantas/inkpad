#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "raylib.h"
#include "raymath.h"

// Assets
#include "assets.c"

// General constants
#define DEFAULT_BGCOLOR    (Color){ 15, 15, 15, 255 }
#define PANEL_BGCOLOR      (Color){ 18, 18, 18, 130 }
#define PANEL_PADDING      15         // gap between elements inside the panel (pixels)
#define PANEL_HEIGHT       90         // height of the panel (pixels)
#define MAX_PAGES          5          // number of pages
#define DEFAULT_SLEEP_TIME 240        // time (in frames) to show a caption in status
#define CANCEL_KEY         KEY_ESCAPE // key to press to cancel action

#define DEFAULT_SMOOTH_LEASH_SIZE 4  // how long (delayed) is the pen in draw stroke mode (in pixels).
                                     // In other words, how much you have to drag the mouse to start drawing
#define MAX_SMOOTH_LEASH_SIZE    30
#define MAX_TEXT_SIZE            256

// System constants
#ifdef _WIN32
#    define INKPAD_HOME getenv("USERPROFILE")
#else
#    define INKPAD_HOME getenv("HOME")
#endif

// Version
#define VERSION "0.8 DEV"
#define VERSION_NAME "Inkpad "VERSION

// Misc
#define da_reserve(da, expected_capacity)                                              \
    do {                                                                               \
        if ((expected_capacity) > (da)->capacity) {                                    \
            if ((da)->capacity == 0) {                                                 \
                (da)->capacity = 256;                                                  \
            }                                                                          \
            while ((expected_capacity) > (da)->capacity) {                             \
                (da)->capacity *= 2;                                                   \
            }                                                                          \
            (da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items)); \
            assert((da)->items != NULL && "Buy more RAM lol");                         \
        }                                                                              \
    } while (0)

// Append an item to a dynamic array
#define da_append(da, item)                  \
    do {                                     \
        da_reserve((da), (da)->count + 1);   \
        (da)->items[(da)->count++] = (item); \
    } while (0)

#define da(T) \
	T *items; \
	size_t count; \
	size_t capacity

// Stroke constants
#define DEFAULT_THICK 8.0f

typedef enum {
	MODE_ERASE = 0,
	
	MODE_DRAW,
	MODE_LINE,
	MODE_RECT,
	MODE_TEXT,
	MODE_CIRCLE,
} Mode;

typedef struct {
	float thick;
	Color color;
	int smoothness; // value in pixels of the radius of smooth leash
} Stroke;

typedef struct {
	da(Image);
	size_t cursor;
} History;

typedef struct {
	da(History);
} HistoryList;

void History_push(History* target, RenderTexture2D snapshot) {
	Image img = LoadImageFromTexture(snapshot.texture);
	ImageFlipVertical(&img); // Flips the texture so it doesn't look upside down bc of opengl shit
	da_append(target, img);
}

typedef enum {
	ENTITY_PATH = 0,
	ENTITY_LINE,
	ENTITY_RECT,
	ENTITY_CIRCLE,
	ENTITY_TEXT
} EntityKind;

typedef struct {
	EntityKind kind;
	Stroke stroke;
	union {
		struct { Vector2* items; size_t count, capacity; } path;
		struct { Vector2 start; Vector2 end; } line;
		struct { Rectangle bb; } rect;
		struct { Vector2 center; size_t radius; } circle;
		struct { Vector2 position; char content[MAX_TEXT_SIZE]; } text;
	};
} Entity;

typedef struct {
	Entity* items;
	size_t count, capacity;
} Canvas;

typedef struct {
	bool cancel;                       // Flag to cancel the current stroke action being done
	bool typing;                       // When in text mode during typing, this flag is true
	size_t current_page;               // Index of the current page selected
	Mode mode;                         // Current mode
	Canvas* canvas;                    // Current canvas object
	Entity current_entity;             // Current entity to be saved between frames
	Stroke s;                          // Current stroke state
	Vector2 last_point;                // Used to save the previous point clicked while holding LMB
	HistoryList history;               // List of all histories across the pages
	Vector2 mouse_last_position, mouse_current_position; 
} Context;

// I wish C had operator overloading...
bool color_eq(Color a, Color b) {
	return (a.r==b.r &&
			a.g==b.g &&
			a.b==b.b &&
			a.a==b.a);
}

// Global context and assets
static Font global_font;

static Context context = {0};

void draw_message(unsigned int x, unsigned int y, const char* text) {
	DrawText(text, x, y, 25, WHITE);
}

void show_stroke_tooltip(Vector2 position, const char* text) {
	Vector2 text_size = MeasureTextEx(global_font, text, 17.0f, 1.0f);
	int pad = 12;
	Rectangle frame = (Rectangle){
		position.x,
		position.y,
		text_size.x + pad*2,
		text_size.y + pad*2
	};
	Vector2 text_position = {
		position.x + pad,
		position.y + pad,
	};
	DrawRectangleLinesEx(frame, 1.0f, GRAY);
	DrawTextEx(global_font, text, text_position, 17.0f, 1.0f, GRAY);
}

void show_stroke(Context* context, unsigned int x, unsigned int y) {
	int w = 50;
	int h = 50;
	Stroke* s = &context->s;
	DrawTextEx(global_font, "MODE", (Vector2) { x + w + 5, y }, 15.0f, 1.0f, GRAY);
	Vector2 texpos = (Vector2) { x + w + 5, y+10 };
	char* text = "Unknown";
	switch (context->mode) {
	case MODE_DRAW: {
		DrawCircleV((Vector2) { x + w/2, y + h/2 }, s->thick/2, s->color);
		text = "Draw";
	} break;
	case MODE_LINE: {
		DrawCircleV((Vector2) { x + w/2, y + h/2 }, s->thick/2, s->color);
		DrawCircleLines(x + w/2, y + h/2, s->thick/2+4, s->color);
		text = "Line";
	} break;
	case MODE_ERASE: {
		DrawCircleLines(x + w/2, y + h/2, s->thick/2, s->color);
		DrawCircleLines(x + w/2, y + h/2, s->thick/2+4, s->color);
		text = "Erase";
	} break;
	case MODE_TEXT: {
		int text_size = s->thick*2;
		Vector2 size = MeasureTextEx(global_font, "Aa", text_size, 1.0f);
		DrawRectangleLines(x, y, w, h, s->color);
		DrawTextEx(global_font, "Aa", (Vector2) { x + w/2 - size.x/2, y + h/2 - size.y/2 }, text_size, 1.0f, s->color);
		text = "Text";
	} break;
	case MODE_RECT: {
		DrawRectangle(x + w/2 - s->thick/2, y + h/2 - s->thick/2, s->thick, s->thick, s->color);
		DrawRectangleLines(x + w/2 - s->thick/2 - 4, y + h/2 - s->thick/2 - 4, s->thick + 8, s->thick + 8, s->color);
		text = "Rect";
	} break;
	case MODE_CIRCLE: {
		DrawCircleV((Vector2) { x + w/2, y + h/2 }, s->thick/2, s->color);
		text = "Circle";
	} break;
	}
	DrawTextEx(global_font, text, texpos, 30.0f, 1.0f, WHITE);
}

void draw_color_option(Rectangle* boundingbox, unsigned int x, unsigned int y, bool selected, Color color) {
	Rectangle rec = (Rectangle){
		x, y, 50, 50
	};
	DrawRectangle(rec.x, rec.y, rec.width, rec.height, color);
	DrawRectangleLinesEx(rec, selected ? 4.0f : 1.0f, WHITE);
	if (boundingbox) {
		boundingbox->x      = rec.x;
		boundingbox->y      = rec.y;
		boundingbox->width  = rec.width;
		boundingbox->height = rec.height;
	}
}

void draw_page_option(Rectangle* boundingbox, bool selected, int number, unsigned int x, unsigned int y) {
	if (selected) DrawRectangle(x, y, 50, 50, (Color) { 10, 230, 10, 255 });
	DrawRectangleLines(x-1, y-1, 52, 52, WHITE);
	char text[2];
	sprintf(text, "%d", number);
	DrawTextEx(global_font, text, (Vector2) { x+5, y+5 }, 21.0f, 1.0f, WHITE);
	boundingbox->x = x;
	boundingbox->y = y;
	boundingbox->width = 50;
	boundingbox->height = 50;
}

bool check_boundingbox(Rectangle bb, Vector2 pos) {
    return (pos.x >= bb.x) &&
           (pos.x <= bb.x + bb.width) &&
           (pos.y >= bb.y) &&
           (pos.y <= bb.y + bb.height);
}

// Modes //

void draw_path(Vector2 start, Vector2 end, Stroke s) {
	float thick = s.thick;
	Color color = s.color;
	DrawCircleV(start, thick/2, color);
	DrawLineEx(start, end, thick, color);
	DrawCircleV(end, thick/2, color);
}

void draw_line(Vector2 start, Vector2 end, Stroke s) {
	DrawCircleV(start, s.thick/2, s.color);
	DrawLineEx(start, end, s.thick, s.color);
	DrawCircleV(end, s.thick/2, s.color);
}

void draw_rect(Rectangle rect, Stroke s) {
	DrawRectangleRoundedLinesEx(rect, 0.01f, 15, s.thick, s.color);
}

// Main draw function that handles when you click LMB
void draw(Context* context) {
	Stroke* s = &context->s;
	Vector2* last_point = &context->last_point;
	Vector2 mouse_current_position = context->mouse_current_position;
	Vector2 mouse_last_position = context->mouse_last_position;
	Entity* ent = &context->current_entity;

	bool entity_done = false;
	
	ent->stroke = *s;
	
	switch (context->mode) {
	case MODE_DRAW:
		ent->kind = ENTITY_PATH;
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) entity_done = true;
		if (s->smoothness > 0) {
			if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
				*last_point = mouse_current_position;
			}
			if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
				Vector2 dir = {
					context->mouse_current_position.x - context->last_point.x,
					context->mouse_current_position.y - context->last_point.y,
				};
				float leash = (float)s->smoothness;
				float dist = sqrtf(dir.x*dir.x + dir.y*dir.y);
				if (dist > leash) {
				    float t = (dist - leash) / (float)dist;
				    Vector2 prev = context->last_point;
				    context->last_point.x += dir.x * t;
				    context->last_point.y += dir.y * t;
				    da_append(&ent->path, prev);
				    da_append(&ent->path, context->last_point);
				}
			}
			break;
		}
		// Fall to the non-smooth draw
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			da_append(&ent->path, mouse_last_position);
		    da_append(&ent->path, mouse_current_position);
		}
		break;
	case MODE_LINE:
		ent->kind = ENTITY_LINE;
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
			*last_point = mouse_current_position;
		
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			ent->line.start = *last_point;
			ent->line.end = mouse_current_position;
			entity_done = true;
		}
		break;
	case MODE_RECT:
		ent->kind = ENTITY_RECT;
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
			*last_point = mouse_current_position;
		
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			if (context->cancel) {
				context->cancel = false;
				break;
			}

			Rectangle rect = {0};
			
			if (mouse_current_position.x < last_point->x) {
				rect.width = last_point->x - mouse_current_position.x;
				rect.x = mouse_current_position.x;
			} else {	
				rect.width = mouse_current_position.x - last_point->x;
				rect.x = last_point->x;
			}
			
			if (mouse_current_position.y < last_point->y) {
				rect.height = last_point->y - mouse_current_position.y;
				rect.y = mouse_current_position.y;
			} else {
				rect.height = mouse_current_position.y - last_point->y;
				rect.y = last_point->y;
			}

			ent->rect.bb = rect;
			entity_done = true;
		}
		break;
	case MODE_TEXT:
		ent->kind = ENTITY_TEXT;
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			*last_point = mouse_current_position;
			context->typing = true;
		}
		
		if (context->typing) {
			if (context->cancel) {
				context->typing = false;
				memset(ent->text.content, 0, sizeof(ent->text.content));
				context->cancel = false;
				break;
			}
			if (IsKeyPressed(KEY_ENTER)) {
				ent->text.position = *last_point;
				context->typing = false;
				entity_done = true;
			} else if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
				int last = strlen(ent->text.content) > 0 ? strlen(ent->text.content)-1 : 0;
				ent->text.content[last] = 0;
			} else {
				char c = GetCharPressed();
				size_t i = strlen(ent->text.content);
				if (i < MAX_TEXT_SIZE)
					ent->text.content[strlen(ent->text.content)] = c;
			}
		}
		break;
	case MODE_ERASE:
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			draw_path(mouse_last_position, mouse_current_position, (Stroke) {
				s->thick,
				DEFAULT_BGCOLOR,
				DEFAULT_SMOOTH_LEASH_SIZE
			});
		}
		break;
	case MODE_CIRCLE:
		ent->kind = ENTITY_CIRCLE;
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			*last_point = mouse_current_position;
		}
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			if (context->cancel) {
				context->cancel = false;
				break;
			}
			int a, b, c;
			a = last_point->y - mouse_current_position.y;
			b = last_point->x - mouse_current_position.x;
			c = sqrt(a*a + b*b);
			ent->circle.center = *last_point;
			ent->circle.radius = c;
			entity_done = true;
		}
		break;
	}
	if (entity_done) {
		da_append(context->canvas, context->current_entity);
		TraceLog(LOG_INFO, "Added entity #%zu", context->canvas->count);
		memset(&context->current_entity, 0, sizeof(Entity));
	}
}

// Function to handle the drawing of the stroke before you press LMB (preview)
void draw_preview(Context* context) {
	Vector2 pos = context->mouse_current_position;
	Vector2* last_point = &context->last_point;
	Stroke* s = &context->s;
	Entity entity = context->current_entity;
	Entity* ent = &context->current_entity;
	
	Color c = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? s->color : WHITE;
	
	switch (context->mode) {
	case MODE_DRAW:
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && entity.kind == ENTITY_PATH && entity.path.count > 0) {
			for (size_t j = 0; j < entity.path.count - 1; j++) {
				draw_path(entity.path.items[j], entity.path.items[j+1], entity.stroke);
			}
		}
		if (s->smoothness > 0) {
			if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
				DrawLineV(context->last_point, context->mouse_current_position, c);
				DrawCircleLinesV(context->last_point, s->thick/2 + 5, WHITE);
			} else {
				DrawCircleLinesV(pos, s->thick/2, c);
			}
			break;
		}
		DrawCircleLinesV(pos, s->thick/2, c);
		break;
	case MODE_LINE:
		DrawCircle(pos.x, pos.y, 2.0f, c);

		DrawLineEx((Vector2) { pos.x - s->thick/2, pos.y }, (Vector2) { pos.x + s->thick/2, pos.y }, 1.0f, c);
		DrawLineEx((Vector2) { pos.x, pos.y - s->thick/2 }, (Vector2) { pos.x, pos.y + s->thick/2 }, 1.0f, c);
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			draw_line(*last_point, pos, (Stroke) {
				1.0f,
				WHITE,
				0
			});
		}
		break;
	case MODE_RECT:
		DrawRectangleLinesEx((Rectangle) { pos.x - s->thick/2, pos.y - s->thick/2, s->thick, s->thick }, 1.0f, c);
		DrawCircle(pos.x, pos.y, 1.0f, c);
		
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			if (IsKeyPressed(CANCEL_KEY)) context->cancel = true;
			if (context->cancel) break;
 			Rectangle rect = {0};
			if (pos.x < last_point->x) {
				rect.width = last_point->x - pos.x;
				rect.x = pos.x;
			} else {
				rect.width = pos.x - last_point->x;
				rect.x = last_point->x;
			}
			if (pos.y < last_point->y) {
				rect.height = last_point->y - pos.y;
				rect.y = pos.y;
			} else {
				rect.height = pos.y - last_point->y;
				rect.y = last_point->y;
			}
			draw_rect(rect, (Stroke) {
				1.0f,
				WHITE,
				0
			});
		}
		break;
	case MODE_TEXT:
		DrawLineEx((Vector2) { pos.x, pos.y - s->thick/2 }, (Vector2) { pos.x, pos.y + s->thick/2 }, 3.0f, c);
		if (context->typing) {
			if (IsKeyPressed(CANCEL_KEY)) context->cancel = true;
			if (context->cancel) break;
			if (IsKeyPressed(KEY_ENTER)) break;
			int text_wid;
			int text_size = s->thick*2;
			int pad = 15;
			int cursor_wid = text_size/2;
			if (*(ent->text.content) == 0) {
				const char* msg = "Start typing...";
				text_wid = MeasureTextEx(global_font, msg, text_size, 1.0f).x;
				DrawTextEx(
					global_font,
					msg,
					(Vector2) { last_point->x, last_point->y - text_size/2 },
					text_size,
					2.0f,
					GRAY
				);
				DrawLineEx(
					(Vector2) {
						last_point->x,
						last_point->y + text_size/2
					}, // x
					(Vector2) {
						last_point->x + cursor_wid,
						last_point->y + text_size/2,
					}, // y
					3.0f,
					c
				);
			} else {
				text_wid = MeasureTextEx(global_font, ent->text.content, text_size, 1.0f).x;
				DrawTextEx(
					global_font,
					ent->text.content,
					(Vector2) { last_point->x, last_point->y - text_size/2 },
					text_size,
					1.0f,
					WHITE
				);
				DrawLineEx(
					(Vector2) {
						last_point->x + text_wid,
						last_point->y + text_size/2
					}, // x
					(Vector2) {
						last_point->x + text_wid + cursor_wid,
						last_point->y + text_size/2,
					}, // y
					3.0f,
					c
				);
			}
			DrawRectangleLines(
				last_point->x               - pad,
				last_point->y - text_size/2 - pad,
				text_wid      + cursor_wid  + pad*2,
				text_size                   + pad*2,
				s->color
			);
		}
		break;
	case MODE_ERASE:
		DrawLineEx((Vector2) { pos.x - s->thick/2, pos.y - s->thick/2 }, (Vector2) { pos.x + s->thick/2, pos.y + s->thick/2 }, 2.0f, WHITE);
		DrawLineEx((Vector2) { pos.x + s->thick/2, pos.y - s->thick/2 }, (Vector2) { pos.x - s->thick/2, pos.y + s->thick/2 }, 2.0f, WHITE);
		break;
	case MODE_CIRCLE:
		DrawRing(pos, s->thick/2, s->thick/1.5, 0.0f, 360.0f, 30, c);
		DrawCircle(pos.x, pos.y, 1.0f, c);
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			if (IsKeyPressed(CANCEL_KEY)) context->cancel = true;
			if (context->cancel) break;
			DrawLineEx(*last_point, pos, 1.5f, WHITE);
			int a, b, len;
			a = last_point->y - pos.y;
			b = last_point->x - pos.x;
			len = sqrt(a*a + b*b);
			DrawCircleLinesV(*last_point, len, WHITE);
		}
		break;
	}
}

void set_status_caption(char* buffer, int* timer, const char* message) {
	strcpy(buffer, message);
	*timer = DEFAULT_SLEEP_TIME;
}

void debug_text(char* txt, int x, int y) {
	DrawTextEx(global_font, txt, (Vector2) { x, y }, 20.0f, 1.0f, GREEN);
}

#define STATUS_TEXT_MAX_SIZE 128

void draw_panel(
	Context* context,
	int window_width,
	int window_height,
	int* status_timer,
	char* status_text,
	Canvas pages[MAX_PAGES],
	Color* color_options,
	int n_color_options
) {
	DrawRectangle(0, window_height - PANEL_HEIGHT, window_width, PANEL_HEIGHT, PANEL_BGCOLOR);
	DrawRectangleLines(0, window_height - PANEL_HEIGHT, window_width, PANEL_HEIGHT, GRAY);
	// Show stroke information
	DrawTextEx(global_font, "Stroke", (Vector2) { PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
	show_stroke(context, PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING+20);

	// Messages
	Vector2 size = MeasureTextEx(global_font, VERSION_NAME, 13.0f, 1.0f);
	DrawTextEx(
		global_font,
		VERSION_NAME,
		(Vector2) {
			window_width-size.x-PANEL_PADDING,
			window_height-100+PANEL_PADDING
		},
		13.0f,
		1.0f,
		GRAY
	);
	
	// Color options
	// BB means Bounding Box, not Bubble Gum! You bastard!
	Rectangle bb = {0};
	int starting_pos = PANEL_PADDING + bb.x + bb.width + 200;
	DrawTextEx(global_font, "Colors", (Vector2) { starting_pos, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
	size_t len = n_color_options;
	for (size_t i = 0; i < len; i++) {
		bool selected = color_eq(context->s.color, color_options[i]);
		draw_color_option(&bb, starting_pos + bb.width*i, window_height - PANEL_HEIGHT + PANEL_PADDING+20, selected, color_options[i]);
		if (check_boundingbox(bb, context->mouse_current_position) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			context->s.color = color_options[i];
		}
	}
	starting_pos += bb.width*len;

	// Draw page buttons
	starting_pos += PANEL_PADDING;
	DrawTextEx(global_font, "Pages", (Vector2) { starting_pos, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
	for (size_t i = 0; i < MAX_PAGES; i++) {
		draw_page_option(&bb, (size_t) context->current_page == i, i+1, starting_pos + bb.width*i, window_height - PANEL_HEIGHT + PANEL_PADDING+20);
		if (check_boundingbox(bb, context->mouse_current_position) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			context->current_page = i;
			context->canvas = &pages[context->current_page];
			set_status_caption(status_text, status_timer, TextFormat("Changed to page %d", context->current_page + 1));
		}
	}
	starting_pos += bb.width*MAX_PAGES;

	// Status text
	DrawTextEx(global_font, "Status", (Vector2) { starting_pos + PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
	DrawTextEx(global_font, status_text, (Vector2) { starting_pos + PANEL_PADDING + 5, window_height - PANEL_HEIGHT + PANEL_PADDING+25 }, window_width/100, 1.0f, GREEN);
	DrawRectangleLines(starting_pos + PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING+20, window_width - starting_pos - PANEL_PADDING*2, bb.height, WHITE);

	if (*status_timer > 0) (*status_timer)--;
	else memset(status_text, 0, STATUS_TEXT_MAX_SIZE);
}

double clamped_increment(double x, double inc, double min, double max) {
    return ((x+inc) >= min && (x+inc) <= max)? inc : 0.0;
}

void render_canvas(const Canvas* canvas) {
	for (size_t i = 0; i < canvas->count; i++) {
		Entity entity = canvas->items[i];
		switch (entity.kind) {
		case ENTITY_PATH: {
			if (entity.path.count <= 0) break;
			for (size_t j = 0; j < entity.path.count - 1; j++) {
				draw_path(entity.path.items[j], entity.path.items[j+1], entity.stroke);
			}
		} break;
		case ENTITY_LINE: {
			draw_line(entity.line.start, entity.line.end, entity.stroke);
		} break;
		case ENTITY_RECT: {
			draw_rect(entity.rect.bb, entity.stroke);
		} break;
		case ENTITY_CIRCLE: {
			DrawRing(entity.circle.center, entity.circle.radius, entity.circle.radius + entity.stroke.thick, 0, 360, 60, entity.stroke.color);
		} break;
		case ENTITY_TEXT: {
			Vector2 text_pos = {
				entity.text.position.x,
				entity.text.position.y - entity.stroke.thick
			};
			DrawTextEx(global_font, entity.text.content, text_pos, entity.stroke.thick * 2, 1.0f, entity.stroke.color);
		} break;
		}
	}
}

int main(void) {
	TraceLog(LOG_INFO, "HOME DIRECTORY: %s", INKPAD_HOME);
	
	// Initialization
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_MAXIMIZED);
	InitWindow(0, 0, "Inkpad");
	SetTargetFPS(120);
	SetExitKey(0);

	size_t window_width = GetScreenWidth();
	size_t window_height = GetScreenHeight();
	TraceLog(LOG_INFO, "Window Width: %zu", window_width);
	TraceLog(LOG_INFO, "Window Height: %zu", window_height);
	
	// Loading assets and configuration
	global_font = LoadFontFromMemory(".ttf", IBMPlexMono_SemiBold_ttf, IBMPlexMono_SemiBold_size, 100, NULL, 0);;;;;;
	
	// Context and canvas
	context.s = (Stroke){
		.thick = DEFAULT_THICK,
		.color = GREEN,
		.smoothness = DEFAULT_SMOOTH_LEASH_SIZE,
	};
	
	context.mode = MODE_DRAW;
	
	Mode saved_mode = context.mode;

	Canvas pages[MAX_PAGES] = {0};
	context.current_page = 0;
	context.canvas = &pages[context.current_page];

	// Interface
	Color color_options[] = { BLACK, WHITE, BEIGE, RED, ORANGE, YELLOW, GREEN, LIME, SKYBLUE, BLUE, PURPLE };

	char status_text[STATUS_TEXT_MAX_SIZE] = {0};
	int status_timer = 0;
	bool show_panel = true;

	// Welcome message
	set_status_caption(status_text, &status_timer, "Welcome to Inkpad. Press F11 to hide the Panel.");
	
	// Window
	while (!WindowShouldClose()) {
		BeginDrawing();
			context.mouse_current_position = GetMousePosition();
			bool is_on_canvas = check_boundingbox(
				(Rectangle) {
					0, 0,
					window_width,
					window_height - (show_panel ? PANEL_HEIGHT : 0) - context.s.thick/2
				},
				context.mouse_current_position
			);
			if (is_on_canvas) {
				draw(&context);
			}
			ClearBackground(DEFAULT_BGCOLOR);

			render_canvas(context.canvas);

			if (show_panel) draw_panel(
				&context,
				window_width,
				window_height,
				&status_timer,
				status_text,
				pages,
				color_options,
				sizeof(color_options)/sizeof(Color)
			);
			
			// Show Coordinates
			char pos_text[32];
			sprintf(pos_text, "X: %.2f Y: %.2f", context.mouse_current_position.x, context.mouse_current_position.y);
			DrawTextEx(global_font, pos_text, (Vector2) { 10, 10 }, 15.0f, 1.0f, WHITE);

			// Page number
			char page_number_text[16];
			sprintf(page_number_text, "%zu/%d", context.current_page+1, MAX_PAGES);
			int height = MeasureTextEx(global_font, page_number_text, 25.0f, 1.0f).y;
			DrawTextEx(
				global_font,
				page_number_text,
				(Vector2) {
					PANEL_PADDING,
					window_height - (show_panel ? PANEL_HEIGHT : 0) - height - PANEL_PADDING
				},
				25.0f,
				1.0f,
				WHITE
			);

			// Input
			if (!context.typing) {
				// Thickness Operations
				if (IsKeyPressed(KEY_ONE))   context.s.thick = DEFAULT_THICK;
				if (IsKeyPressed(KEY_TWO))   context.s.thick = DEFAULT_THICK + 5.0f;
				if (IsKeyPressed(KEY_THREE)) context.s.thick = DEFAULT_THICK + 10.0f;
				if (IsKeyPressed(KEY_FOUR))  context.s.thick = DEFAULT_THICK + 15.0f;
				if (IsKeyPressed(KEY_FIVE))  context.s.thick = DEFAULT_THICK + 20.0f;
				if (IsKeyPressed(KEY_ZERO))  context.s.thick = DEFAULT_THICK/2;

				// Fullscreen and window resizing
				if (IsKeyPressed(KEY_F11)) {
					show_panel = !show_panel;
				}
				
				if (IsWindowResized()) {
					window_width = GetScreenWidth();
					window_height = GetScreenHeight();
				}

				// Quit and Cancel key
				if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) break;
				if (context.cancel) set_status_caption(status_text, &status_timer, "Action cancelled");

				// Undo
				if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z)) {
					// History* h = &context.history.items[context.current_page];
					// if (h->cursor > 0) {
					// 	if (h->cursor == h->count) History_push(h, *context.canvas); // Save state before undo to be able to redo
					// 	clear_and_draw_texture(*context.canvas, LoadTextureFromImage(h->items[--h->cursor]));
					// 	set_status_caption(status_text, &status_timer, "Undid action");
					// }
				}

				// Redo
				if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Y)) {
					// History* h = &context.history.items[context.current_page];
					// if (h->cursor + 1 < h->count) {
					// 	clear_and_draw_texture(*context.canvas, LoadTextureFromImage(h->items[++h->cursor]));
					// 	set_status_caption(status_text, &status_timer, "Redid action");
					// }
				}
				
				// Change Thickness by Mouse wheel (LEFT ALT)
				if (IsKeyDown(KEY_LEFT_ALT)) {
					double wheel = GetMouseWheelMove() * 4;
					context.s.thick += clamped_increment(context.s.thick, wheel, DEFAULT_THICK/2, DEFAULT_THICK+20.0);
					Vector2 pos = (Vector2){context.mouse_current_position.x+context.s.thick/2, context.mouse_current_position.y};
					show_stroke_tooltip(pos, TextFormat("Thickness: %.2f", context.s.thick));
				}

				// Change color by Mouse wheel (TAB)
				if (IsKeyDown(KEY_TAB)) {
					int wheel = (int)GetMouseWheelMove();
					size_t len = sizeof(color_options)/sizeof(Color);
					Color color = context.s.color;
					size_t index = 0;
					for (size_t i = 0; i < len; i++) {
						if (color_eq(color, color_options[i])) {
							index = i;
						}
					}
					Vector2 pos = context.mouse_current_position;
					size_t pad = context.s.thick/2;

					size_t selected = (index-wheel);
					draw_color_option(NULL, pos.x + pad, pos.y + pad, false, color);					
					context.s.color = color_options[selected <= len ? selected : len-1];
				}

				// Stroke smoothness by Mouse wheel
				if (IsKeyDown(KEY_S) && context.mode == MODE_DRAW) {
					double wheel = GetMouseWheelMove();
					context.s.smoothness += clamped_increment(context.s.smoothness, wheel, 0, MAX_SMOOTH_LEASH_SIZE);
					DrawCircleLinesV(context.mouse_current_position, context.s.smoothness, context.s.color);
					Vector2 pos = (Vector2){context.mouse_current_position.x+context.s.smoothness, context.mouse_current_position.y};
					show_stroke_tooltip(pos, TextFormat("Smoothness: %.2f", (float)context.s.smoothness));
				}
				
				// Quick erase
				if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
				{ saved_mode = context.mode;
				  context.mode = MODE_ERASE;
				  context.s.thick *= 2;
			    }
				if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
				{ context.mode = saved_mode;
			      context.s.thick /= 2;
				}

				// Quick line
			    if (IsKeyPressed(KEY_LEFT_SHIFT))
			    { saved_mode = context.mode;
			      context.mode = MODE_LINE;
			    }
				if (IsKeyReleased(KEY_LEFT_SHIFT)) context.mode = saved_mode;

				// Modes
				if (IsKeyPressed(KEY_A)) context.mode = MODE_DRAW;
				if (IsKeyPressed(KEY_L)) context.mode = MODE_LINE;
				if (IsKeyPressed(KEY_X)) {
					if (IsKeyDown(KEY_LEFT_CONTROL)) {
						context.canvas->count = 0;
						set_status_caption(status_text, &status_timer, "Cleared screen");
					} else {
						context.mode = MODE_ERASE;
					}
				}
				if (IsKeyPressed(KEY_SPACE)) context.mode = MODE_TEXT;
				if (IsKeyPressed(KEY_R))     context.mode = MODE_RECT;
				if (IsKeyPressed(KEY_C))     context.mode = MODE_CIRCLE;

				// Page shortcuts
				if (IsKeyPressed(KEY_PERIOD))
				{ context.current_page = context.current_page < MAX_PAGES-1 ? context.current_page + 1 : context.current_page;
			      context.canvas = &pages[context.current_page];
			      set_status_caption(status_text, &status_timer, TextFormat("Changed to page %d", context.current_page + 1));
				}
				if (IsKeyPressed(KEY_COMMA))
				{ context.current_page = context.current_page > 0 ? context.current_page - 1 : context.current_page;
			      context.canvas = &pages[context.current_page];
			      set_status_caption(status_text, &status_timer, TextFormat("Changed to page %d", context.current_page + 1));
  				}
			}
			
			// Draw stroke preview
			if (!is_on_canvas)
				ShowCursor();
			else {
				HideCursor();
				draw_preview(&context);
			}
		EndDrawing();
		context.mouse_last_position = context.mouse_current_position;
	}

	// UnloadRenderTexture(*context.canvas);
	CloseWindow();
	return 0;
}

