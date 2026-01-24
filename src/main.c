#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "raylib.h"

// Assets
#include "res/IBMPlexMono_SemiBold.c"
#include "res/Save.c"

// Shader
#include "fxaa.c"

// General constants
#define DEFAULT_BGCOLOR (Color){18, 18, 18, 255}
#define PANEL_BGCOLOR (Color){18, 18, 18, 130}
#define PANEL_PADDING      15         // gap between elements inside the panel (pixels)
#define PANEL_HEIGHT       90         // height of the panel (pixels)
#define MAX_PAGES          5          // number of pages
#define DEFAULT_SLEEP_TIME 240        // time (in frames) to show a caption in status
#define CANCEL_KEY         KEY_ESCAPE // key to press to cancel action

// System constants
#ifdef _WIN32
#    define INKPAD_HOME getenv("USERPROFILE")
#else
#    define INKPAD_HOME getenv("HOME")
#endif

// Version
#define VERSION "0.7-dev"
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

// Stroke constants
#define DEFAULT_THICK 8.0f

typedef enum {
	MODE_FREE = 0,
	MODE_LINE,
	MODE_RECT,
	MODE_TEXT,
	MODE_ERASE,
	MODE_CIRCLE,
} Mode;

typedef struct {
	Mode mode;
	float thick;
	Color color;
} Stroke;

typedef struct {
	Vector2 pos;
	char content[1024];
	bool active;
} TextState;

typedef struct {
	Image *items;
	size_t count;
	size_t capacity;
	
	size_t cursor;
} History;

typedef struct {
	History *items;
	size_t count;
	size_t capacity;
} HistoryList;

void History_push(History* target, RenderTexture2D snapshot) {
	Image img = LoadImageFromTexture(snapshot.texture);
	ImageFlipVertical(&img); // Flips the texture so it doesn't look upside down bc of opengl shit
	da_append(target, img);
}

typedef struct {
	bool cancel;                       // Flag to cancel the current stroke action being done
	size_t current_page;               // Index of the current page selected
	RenderTexture2D* canvas;           // Current canvas object
	Stroke s;                          // Current stroke state
	Vector2 last_point;                // Used to save the previous point clicked while holding LMB
	TextState text;                    // Current text state
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
static Texture2D save_icon;

static Context context = {0};

void draw_message(unsigned int x, unsigned int y, char* text) {
	DrawText(text, x, y, 25, WHITE);
}

void show_stroke_tooltip(Vector2 position, Stroke* s) {
	int x = position.x;
	int y = position.y;
	Vector2 texpos = (Vector2) { x+s->thick/2, y+s->thick/2 };
	DrawTextEx(global_font, TextFormat("%.2f", s->thick), texpos, 17.0f, 1.0f, GRAY);
}

void show_stroke(unsigned int x, unsigned int y, Stroke* s) {
	int w = 50;
	int h = 50;
	DrawTextEx(global_font, "MODE", (Vector2) { x + w + 5, y }, 15.0f, 1.0f, GRAY);
	Vector2 texpos = (Vector2) { x + w + 5, y+10 };
	char* text = "Unknown";
	switch (s->mode) {
	case MODE_FREE:
		DrawCircleV((Vector2) { x + w/2, y + h/2 }, s->thick/2, s->color);
		DrawCircleLines(x + w/2, y + h/2, s->thick/2+4, s->color);
		text = "Free";
		break;
	case MODE_LINE:
		DrawCircleV((Vector2) { x + w/2, y + h/2 }, s->thick/2, s->color);
		DrawCircleLines(x + w/2, y + h/2, s->thick/2+4, s->color);
		text = "Line";
		break;
	case MODE_ERASE:
		DrawCircleLines(x + w/2, y + h/2, s->thick/2, s->color);
		DrawCircleLines(x + w/2, y + h/2, s->thick/2+4, s->color);
		text = "Erase";
		break;
	case MODE_TEXT:
		int text_size = s->thick*2;
		Vector2 size = MeasureTextEx(global_font, "T", text_size, 1.0f);
		DrawRectangleLines(x, y, w, h, s->color);
		DrawTextEx(global_font, "T", (Vector2) { x + w/2 - size.x/2, y + h/2 - size.y/2 }, text_size, 1.0f, s->color);
		text = "Text";
		break;
	case MODE_RECT:
		DrawRectangle(x + w/2 - s->thick/2, y + h/2 - s->thick/2, s->thick, s->thick, s->color);
		DrawRectangleLines(x + w/2 - s->thick/2 - 4, y + h/2 - s->thick/2 - 4, s->thick + 8, s->thick + 8, s->color);
		text = "Rect";
		break;
	case MODE_CIRCLE:
		DrawCircleV((Vector2) { x + w/2, y + h/2 }, s->thick/2, s->color);
		text = "Circle";
		break;
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

void draw_texture_button(Rectangle* boundingbox, Texture2D tex, unsigned int x, unsigned int y) {
	float scaleX = (float)52 / tex.width;
    float scaleY = (float)52 / tex.height;
	DrawTextureEx(tex, (Vector2){x, y}, 0.0f, (scaleX < scaleY) ? scaleX : scaleY, WHITE);
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

void draw_free(Vector2 start, Vector2 end, Stroke *s) {
	float thick = s->thick;
	Color color = s->color;
	DrawLineEx(start, end, thick, color);
	DrawCircleV(end, thick/2, color);
}

void draw_line(Vector2 start, Vector2 end, Stroke* s) {
	DrawCircleV(start, s->thick/2, s->color);
	DrawLineEx(start, end, s->thick, s->color);
	DrawCircleV(end, s->thick/2, s->color);
}

void draw_rect(Rectangle rect, Stroke* s) {
	DrawRectangleRoundedLinesEx(rect, 0.01f, 15, s->thick, s->color);
}

// Main draw function that handles when you click LMB
void draw(Context* context) {
	Stroke* s = &context->s;
	TextState* text = &context->text;
	Vector2* last_point = &context->last_point;
	Vector2 mouse_current_position = context->mouse_current_position;
	Vector2 mouse_last_position = context->mouse_last_position;
	
	switch (s->mode) {
	case MODE_FREE:
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			draw_free(mouse_last_position, mouse_current_position, s);
		}
		break;
	case MODE_LINE:
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
			*last_point = mouse_current_position;
		
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
			draw_line(*last_point, mouse_current_position, s);
		break;
	case MODE_RECT:
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
			draw_rect(rect, s);
		}
		break;
	case MODE_TEXT:
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			text->pos = mouse_current_position;
			text->active = true;
		}
		if (text->active) {
			if (context->cancel) {
				text->active = false;
				memset(text->content, 0, sizeof(text->content));
				context->cancel = false;
				break;
			}
			if (IsKeyPressed(KEY_ENTER)) {
				int text_size = s->thick*3;
				DrawTextEx(global_font, text->content, (Vector2) { text->pos.x, text->pos.y - text_size/2 }, text_size, 1.0f, s->color);
				memset(text->content, 0, sizeof(text->content));
				text->active = false;
			} else if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
				int last = strlen(text->content) > 0 ? strlen(text->content)-1 : 0;
				text->content[last] = 0;
			} else {
				char c = GetCharPressed();
				text->content[strlen(text->content)] = c;
			}
		}
		break;
	case MODE_ERASE:
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			draw_free(mouse_last_position, mouse_current_position, &(Stroke) {
				MODE_ERASE,
				s->thick,
				DEFAULT_BGCOLOR,
			});
		}
		break;
	case MODE_CIRCLE:
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
			DrawRing(*last_point, c, c + s->thick, 0.0f, 360.0f, 80, s->color);
		}
		break;
	}
}

// Function to handle the drawing of the stroke before you press LMB (preview)
void draw_preview(Context* context) {
	Vector2 pos = context->mouse_current_position;
	
	Vector2* last_point = &context->last_point;
	TextState* text = &context->text;
	Stroke* s = &context->s;
	
	Color c = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? s->color : WHITE;
	switch (s->mode) {
	case MODE_FREE:
		DrawCircleLinesV(pos, s->thick/2, c);
		break;
	case MODE_LINE:
		DrawCircle(pos.x, pos.y, 2.0f, c);

		DrawLineEx((Vector2) { pos.x - s->thick/2, pos.y }, (Vector2) { pos.x + s->thick/2, pos.y }, 1.0f, c);
		DrawLineEx((Vector2) { pos.x, pos.y - s->thick/2 }, (Vector2) { pos.x, pos.y + s->thick/2 }, 1.0f, c);
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			draw_line(*last_point, pos, &(Stroke){
				MODE_LINE,
				1.0f,
				WHITE
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
			draw_rect(rect, &(Stroke) {
				MODE_RECT,
				1.0f,
				WHITE
			});
		}
		break;
	case MODE_TEXT:
		DrawLineEx((Vector2) { pos.x, pos.y - s->thick/2 }, (Vector2) { pos.x, pos.y + s->thick/2 }, 3.0f, c);
		if (text->active) {
			if (IsKeyPressed(CANCEL_KEY)) context->cancel = true;
			if (context->cancel) break;
			if (!IsKeyPressed(KEY_ENTER)) {
				int text_size = s->thick*3;
				int text_wid = MeasureTextEx(global_font, text->content, text_size, 1.0f).x;
				int pad = 15;
				int cursor_wid = text_size/2;
				DrawRectangleLines(
					text->pos.x               - pad,
					text->pos.y - text_size/2 - pad,
					text_wid    + cursor_wid  + pad*2,
					text_size                 + pad*2,
					s->color
				);
				DrawLineEx(
					(Vector2) {
						text->pos.x + text_wid,
						text->pos.y + text_size/2 - text_size*0.1
					}, // x
					(Vector2) {
						text->pos.x + text_wid + cursor_wid,
						text->pos.y + text_size/2 - text_size*0.1,
					}, // y
					3.0f,
					c
				);
				DrawTextEx(
					global_font,
					text->content,
					(Vector2) { text->pos.x, text->pos.y - text_size/2 },
					text_size,
					1.0f,
					WHITE
				);
			}
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

void save_canvas_as_image(Texture2D canvas, const char* filename) {
	Image i = LoadImageFromTexture(canvas);
	ImageFlipVertical(&i);
	ExportImage(i, filename);
	UnloadImage(i);
}

void set_status_caption(char* buffer, int* timer, const char* message) {
	strcpy(buffer, message);
	*timer = DEFAULT_SLEEP_TIME;
}

void debug_text(char* txt, int x, int y) {
	DrawTextEx(global_font, txt, (Vector2) { x, y }, 20.0f, 1.0f, GREEN);
}

void draw_panel(
	Context* context,
	int window_width,
	int window_height,
	int* status_timer,
	char* status_text,
	RenderTexture2D pages[MAX_PAGES],
	Color* color_options,
	int n_color_options
) {
	DrawRectangle(0, window_height - PANEL_HEIGHT, window_width, PANEL_HEIGHT, PANEL_BGCOLOR);
	DrawRectangleLines(0, window_height - PANEL_HEIGHT, window_width, PANEL_HEIGHT, GRAY);
	// Show stroke information
	DrawTextEx(global_font, "Stroke", (Vector2) { PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
	show_stroke(PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING+20, &context->s);

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

	// Save button
	starting_pos += PANEL_PADDING;
	DrawTextEx(global_font, "Save", (Vector2) { starting_pos, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
	draw_texture_button(&bb, save_icon, starting_pos, window_height - PANEL_HEIGHT + PANEL_PADDING+20);
	if (check_boundingbox(bb, context->mouse_current_position) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
		char* home = INKPAD_HOME;
		const char* filename = TextFormat("%s/inkpad_page%d.png", home, context->current_page+1);
		save_canvas_as_image(context->canvas->texture, filename);
		set_status_caption(status_text, status_timer, TextFormat("Saved canvas successfully as \"%s\".", filename));
	}
	starting_pos += bb.width;

	// Status text
	DrawTextEx(global_font, "Status", (Vector2) { starting_pos + PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
	DrawTextEx(global_font, status_text, (Vector2) { starting_pos + PANEL_PADDING + 5, window_height - PANEL_HEIGHT + PANEL_PADDING+25 }, window_width/100, 1.0f, GREEN);
	DrawRectangleLines(starting_pos + PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING+20, window_width - starting_pos - PANEL_PADDING*2, bb.height, WHITE);
	if (*status_timer > 0) (*status_timer)--;
	else memset(status_text, 0, sizeof(status_text));
}

int main(void) {
	TraceLog(LOG_INFO, "HOME DIRECTORY: %s", INKPAD_HOME);
	
	// Initialization
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_MAXIMIZED);
	InitWindow(0, 0, "Inkpad");
	size_t window_width = GetScreenWidth();
	size_t window_height = GetScreenHeight();
	SetTargetFPS(120);
	SetExitKey(0);

	// Loading assets and configuration
	global_font = LoadFontFromMemory(".ttf", IBMPlexMono_SemiBold_ttf, IBMPlexMono_SemiBold_size, 40, NULL, 0);;;;;;
	save_icon = LoadTextureFromImage(LoadImageFromMemory(".png", Save_png, Save_size));

	Shader fxaa = LoadShaderFromMemory(0, fxaa_shader);
	int resLoc = GetShaderLocation(fxaa, "resolution");
	Vector2 res = { (float)window_width, (float)window_height };
	SetShaderValue(fxaa, resLoc, &res, SHADER_UNIFORM_VEC2);

	// Context and canvas
	context.s = (Stroke){
		MODE_FREE,
		DEFAULT_THICK,
		GREEN,
	};
	context.text = (TextState){0};
	
	Mode saved_mode = context.s.mode;

	RenderTexture2D pages[MAX_PAGES] = {
		LoadRenderTexture(window_width, window_height), // These 100 pixels is the height if the status panel
		LoadRenderTexture(window_width, window_height),
		LoadRenderTexture(window_width, window_height),
		LoadRenderTexture(window_width, window_height),
		LoadRenderTexture(window_width, window_height)
	};
	context.current_page = 0;
	context.canvas = &pages[context.current_page];

	for (int i = 0; (size_t)i < sizeof(pages)/sizeof(RenderTexture2D); ++i) {
		da_append(&context.history, (History){0});
		BeginTextureMode(pages[i]);
		ClearBackground(DEFAULT_BGCOLOR);
		EndTextureMode();
	}

	// Interface
	Color color_options[] = { WHITE, BEIGE, RED, ORANGE, YELLOW, GREEN, LIME, SKYBLUE, BLUE, PURPLE };

	char status_text[128] = {0};
	int status_timer = 0;
	bool show_panel = true;

	// Welcome message
	set_status_caption(status_text, &status_timer, "Welcome to Inkpad. Press F11 to hide the Panel.");
	
	// Window
	while (!WindowShouldClose()) {
		context.mouse_current_position = GetMousePosition();

		bool is_on_canvas = check_boundingbox(
			(Rectangle) {
				0, 0,
				window_width,
				window_height - (show_panel ? PANEL_HEIGHT : 0) - context.s.thick/2
			},
			context.mouse_current_position
		);
		
		BeginTextureMode(*context.canvas);
			if (is_on_canvas) {
				if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
					History* h = &context.history.items[context.current_page];
					if (h->cursor < h->count) {
						// Deallocate all snapshots after the cursor
						for (int i = h->cursor+1; i < h->count; i++) {
							UnloadImage(h->items[i]);
							TraceLog(LOG_INFO, "Deallocated snapshot from History");
						}
						h->count = h->cursor; // cut loose the history list "tail"
					}
					History_push(h, *context.canvas);
					h->cursor++;
				}
				draw(&context);
			}
		EndTextureMode();
		BeginDrawing();
			ClearBackground((Color){ 40, 40, 40, 255 });
			
			BeginShaderMode(fxaa);
			// Draw canvas
			DrawTextureRec(
				context.canvas->texture,
			    (Rectangle){
			    	0,
			    	0,
			    	(float)context.canvas->texture.width,
			    	-(float)context.canvas->texture.height
			    }, // Flips the texture so it doesn't look upside down bc of opengl shit
				(Vector2){0, 0},
				WHITE
			);
			EndShaderMode();
			
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
			sprintf(page_number_text, "%ld/%d", context.current_page+1, MAX_PAGES);
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
			if (!context.text.active) {
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

				// Quit key
				if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) break;
				if (context.cancel) set_status_caption(status_text, &status_timer, "Action cancelled");

				// Undo
				if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z)) {
					History* h = &context.history.items[context.current_page];
					if (h->cursor > 0) {
						if (h->cursor == h->count) History_push(h, *context.canvas); // Save state before undo to be able to redo
						BeginTextureMode(*context.canvas);
							Texture2D t = LoadTextureFromImage(h->items[--h->cursor]);
							DrawTexture(t, 0, 0, WHITE);
						EndTextureMode();
						UnloadTexture(t); // Avoid memory leak
						set_status_caption(status_text, &status_timer, "Undid action");
					}
				}

				// Redo
				if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Y)) {
					History* h = &context.history.items[context.current_page];
					if (h->cursor + 1 < h->count) {
						BeginTextureMode(*context.canvas);
							Texture2D t = LoadTextureFromImage(h->items[++h->cursor]);
							DrawTexture(t, 0, 0, WHITE);
						EndTextureMode();
						UnloadTexture(t); // Avoid memory leak
						set_status_caption(status_text, &status_timer, "Redid action");
					}
				}
				
				// Change Thickness by Mouse wheel (LEFT ALT)
				if (IsKeyDown(KEY_LEFT_ALT)) {
					double wheel = GetMouseWheelMove() * 3;
					if (wheel < 0) context.s.thick += context.s.thick >= DEFAULT_THICK/2 ? wheel : 0;
					else if (wheel > 0) context.s.thick += context.s.thick <= DEFAULT_THICK + 20.0f ? wheel : 0;
					show_stroke_tooltip(context.mouse_current_position, &context.s);
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
					
					if (selected < 0)
						context.s.color = color_options[selected >= 0 ? selected : 0];
					else
						context.s.color = color_options[selected <= len ? selected : len-1];
				}
				// Quick erase
				if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
				{ saved_mode = context.s.mode;
				  context.s.mode = MODE_ERASE;
				  context.s.thick *= 2;
			    }
				if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
				{ context.s.mode = saved_mode;
			      context.s.thick /= 2;
				}

				// Quick line
			    if (IsKeyPressed(KEY_LEFT_SHIFT))
			    { saved_mode = context.s.mode;
			      context.s.mode = MODE_LINE;
			    }
				if (IsKeyReleased(KEY_LEFT_SHIFT)) context.s.mode = saved_mode;

				// Modes
				if (IsKeyPressed(KEY_A)) context.s.mode = MODE_FREE;
				if (IsKeyPressed(KEY_L)) context.s.mode = MODE_LINE;
				if (IsKeyPressed(KEY_X)) {
					if (IsKeyDown(KEY_LEFT_CONTROL)) {
						BeginTextureMode(*context.canvas);
						ClearBackground(DEFAULT_BGCOLOR);				
						EndTextureMode();
						set_status_caption(status_text, &status_timer, "Cleared screen");
					} else {
						context.s.mode = MODE_ERASE;
					}
				}
				if (IsKeyPressed(KEY_SPACE)) context.s.mode = MODE_TEXT;
				if (IsKeyPressed(KEY_R))     context.s.mode = MODE_RECT;
				if (IsKeyPressed(KEY_C))     context.s.mode = MODE_CIRCLE;

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
			// debug_text(TextFormat("history cursor = %ld", context.history[context.current_page].cursor), 50, 50);
			// debug_text(TextFormat("history len = %ld", context.history[context.current_page].len), 50, 70);
		EndDrawing();
		context.mouse_last_position = context.mouse_current_position;
	}

	UnloadRenderTexture(*context.canvas);
	CloseWindow();
	return 0;
}
