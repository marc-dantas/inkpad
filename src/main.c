#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#include "Px437_IBM_VGA_9x16.c"
#include "Save.c"
#include "FXAA.c"

#define BGCOLOR (Color){18, 18, 18, 255}

// General constants
#define PANEL_PADDING      15 // pixels
#define PANEL_HEIGHT       90 // pixels
#define MAX_PAGES          5
#define DEFAULT_SLEEP_TIME 240 // 2 seconds if running as 120 fps

// System constants
#ifdef _WIN32
#    define INKPAD_HOME getenv("USERPROFILE")
#else
#    define INKPAD_HOME getenv("HOME")
#endif

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
	Stroke s;
	Vector2 line[2];
	Rectangle rect;
	Vector2 circle_center;
	TextState text;
	Vector2 mouse_last_position, mouse_current_position;
} Context;

Font global_font;

void draw_stroke(Vector2 start, Vector2 end, Stroke *s) {
	float thick = s->thick;
	Color color = s->color;
	DrawLineEx(start, end, thick, color);
	DrawCircleV(end, thick/2, color);
}

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
	DrawCircleV((Vector2) { x + w/2, y + h/2 }, s->thick/2, s->color);
	DrawCircleLines(x + w/2, y + h/2, s->thick/2+4, s->color);
	DrawTextEx(global_font, "MODE", (Vector2) { x + w + 5, y }, 15.0f, 1.0f, GRAY);
	Vector2 texpos = (Vector2) { x + w + 5, y+20 };
	switch (s->mode) {
	case MODE_FREE:
		DrawTextEx(global_font, "Free", texpos, 30.0f, 1.0f, WHITE);
		break;
	case MODE_LINE:
		DrawTextEx(global_font, "Line", texpos, 30.0f, 1.0f, WHITE);
		break;
	case MODE_ERASE:
		DrawTextEx(global_font, "Erase", texpos, 30.0f, 1.0f, WHITE);
		break;
	case MODE_TEXT:
		DrawTextEx(global_font, "Text", texpos, 30.0f, 1.0f, WHITE);
		break;
	case MODE_RECT:
		DrawTextEx(global_font, "Rect", texpos, 30.0f, 1.0f, WHITE);
		break;
	case MODE_CIRCLE:
		DrawTextEx(global_font, "Circle", texpos, 30.0f, 1.0f, WHITE);
		break;
	}
}

void draw_color_option(Rectangle* boundingbox, unsigned int x, unsigned int y, Color color) {
	DrawRectangle(x, y, 50, 50, color);
	DrawRectangleLines(x-1, y-1, 52, 52, WHITE);
	boundingbox->x = x;
	boundingbox->y = y;
	boundingbox->width = 50;
	boundingbox->height = 50;
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

void draw_line(Vector2 start, Vector2 end, Stroke* s) {
	DrawCircleV(start, s->thick/2, s->color);
	DrawLineEx(start, end, s->thick, s->color);
	DrawCircleV(end, s->thick/2, s->color);
}

void draw_rect(Rectangle rect, Stroke* s) {
	DrawRectangleRoundedLinesEx(rect, 0.01f, 15, s->thick, s->color);
}

void draw(Context* context) {
	Stroke* s = &context->s;
	Vector2* circle_center = &context->circle_center;
	TextState* text = &context->text;
	Rectangle* rect = &context->rect;
	Vector2* line[2]; line[0] = &context->line[0]; line[1] = &context->line[1];
	Vector2 mouse_current_position = context->mouse_current_position;
	Vector2 mouse_last_position = context->mouse_last_position;
	
	switch (s->mode) {
	case MODE_FREE: {
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			draw_stroke(mouse_last_position, mouse_current_position, s);
		}
		break;
	}
	case MODE_LINE: {
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			*line[0] = mouse_current_position;
		}
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			*line[1] = mouse_current_position;
			draw_line(*line[0], *line[1], s);
		}
		break;
	}
	case MODE_RECT: {
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			rect->x = mouse_current_position.x;
			rect->y = mouse_current_position.y;
		}
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			if (mouse_current_position.x < rect->x) {
				rect->width = rect->x - mouse_current_position.x;
				rect->x = mouse_current_position.x;
			} else {	
				rect->width = mouse_current_position.x - rect->x;
			}
			if (mouse_current_position.y < rect->y) {
				rect->height = rect->y - mouse_current_position.y;
				rect->y = mouse_current_position.y;
			} else {
				rect->height = mouse_current_position.y - rect->y;
			}
			draw_rect(*rect, s);
		}
		break;
	}
	case MODE_TEXT: {
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			text->pos = mouse_current_position;
			text->active = true;
		}
		if (text->active) {
			if (IsKeyPressed(KEY_ENTER)) {
				DrawTextEx(global_font, text->content, (Vector2) { text->pos.x, text->pos.y - s->thick }, s->thick*2, 1.0f, s->color);
				memset(text->content, 0, sizeof(text->content));
				text->active = false;
			} else if (IsKeyPressed(KEY_BACKSPACE)) {
				int last = strlen(text->content) > 0 ? strlen(text->content)-1 : 0;
				text->content[last] = 0;
			} else {
				char c = GetCharPressed();
				text->content[strlen(text->content)] = c;
			}
		}
		break;
	}
	case MODE_ERASE: {
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			draw_stroke(mouse_last_position, mouse_current_position, &(Stroke) {
				MODE_ERASE,
				s->thick,
				BGCOLOR,
			});
		}
		break;
	}
	case MODE_CIRCLE: {
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			*circle_center = mouse_current_position;
		}
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			int a, b, c;
			a = circle_center->y - mouse_current_position.y;
			b = circle_center->x - mouse_current_position.x;
			c = sqrt(a*a + b*b);
			DrawRing(*circle_center, c, c + s->thick, 0.0f, 360.0f, 80, s->color);
		}
		break;
	}
	}
}

void draw_stroke_preview(Context* context) {
	Vector2 pos = context->mouse_current_position;
	Vector2* line[2]; line[0] = &context->line[0]; line[1] = &context->line[1];
	Rectangle* rect = &context->rect;
	Vector2 circle_center = context->circle_center;
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
			draw_line(*line[0], pos, &(Stroke){
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
			Rectangle preview_rect = *rect;
			if (pos.x < preview_rect.x) {
				preview_rect.width = preview_rect.x - pos.x;
				preview_rect.x = pos.x;
			} else {
				preview_rect.width = pos.x - preview_rect.x;
			}
			if (pos.y < preview_rect.y) {
				preview_rect.height = preview_rect.y - pos.y;
				preview_rect.y = pos.y;
			} else {
				preview_rect.height = pos.y - preview_rect.y;
			}
			draw_rect(preview_rect, &(Stroke) {
				MODE_RECT,
				1.0f,
				WHITE
			});
		}
		break;
	case MODE_TEXT:
		DrawLineEx((Vector2) { pos.x, pos.y - s->thick/2 }, (Vector2) { pos.x, pos.y + s->thick/2 }, 3.0f, c);
		if (text->active) {
			if (!IsKeyPressed(KEY_ENTER)) {
				int text_wid = MeasureTextEx(global_font, text->content, s->thick*2, 1.0f).x;
				DrawLineEx(
					(Vector2) {
						text->pos.x + text_wid + 2,
						text->pos.y + s->thick/2 + 2
					},
					(Vector2) {
						text->pos.x + text_wid + 2 + s->thick,
						text->pos.y + s->thick/2 + 2,
					},
					3.0f,
					c
				);
				DrawTextEx(global_font, text->content, (Vector2) { text->pos.x, text->pos.y - s->thick }, s->thick*2, 1.0f, WHITE);
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
			DrawLineEx(circle_center, pos, 1.5f, WHITE);
			int a, b, len;
			a = circle_center.y - pos.y;
			b = circle_center.x - pos.x;
			len = sqrt(a*a + b*b);
			DrawCircleLinesV(circle_center, len, WHITE);
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

int main(void) {
	printf("INKPAD: HOME DIRECTORY: %s\n", INKPAD_HOME);
	
	Context context = {0};
	context.s = (Stroke){
		MODE_FREE,
		DEFAULT_THICK,
		GREEN,
	};
	context.text = (TextState){0};
	
	Mode saved_mode = context.s.mode;

	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_MAXIMIZED);
	InitWindow(GetScreenWidth(), GetScreenHeight(), "Inkpad");

	
	size_t window_width = GetScreenWidth();
	size_t window_height = GetScreenHeight();

	Shader fxaa = LoadShaderFromMemory(0, fxaa_shader);
	int resLoc = GetShaderLocation(fxaa, "resolution");
	Vector2 res = { (float)window_width, (float)window_height };
	SetShaderValue(fxaa, resLoc, &res, SHADER_UNIFORM_VEC2);
	HideCursor();

	global_font = LoadFont_Px437();
	Image i = { .data = SAVE_DATA, .width = SAVE_WIDTH, .height = SAVE_HEIGHT, .format = SAVE_FORMAT, .mipmaps = 1 };;
	Texture2D save_icon = LoadTextureFromImage(i);
	
	Color color_options[] = { WHITE, BEIGE, RED, ORANGE, YELLOW, GREEN, LIME, SKYBLUE, BLUE, PURPLE };
	RenderTexture2D pages[MAX_PAGES] = {
		LoadRenderTexture(window_width, window_height), // These 100 pixels is the height if the status panel
		LoadRenderTexture(window_width, window_height),
		LoadRenderTexture(window_width, window_height),
		LoadRenderTexture(window_width, window_height),
		LoadRenderTexture(window_width, window_height)
	};
	int page_selection = 0;
	RenderTexture2D canvas = pages[page_selection];

	char status_text[128] = {0};
	int status_timer = DEFAULT_SLEEP_TIME;
	
	SetTargetFPS(120);

	for (int i = 0; (size_t)i < sizeof(pages)/sizeof(RenderTexture2D); ++i) {
		BeginTextureMode(pages[i]);
		ClearBackground(BGCOLOR);
		EndTextureMode();
	}

	while (!WindowShouldClose()) {
		context.mouse_current_position = GetMousePosition();
		BeginTextureMode(canvas);
			if (check_boundingbox((Rectangle){0, 0, window_width, window_height - PANEL_HEIGHT - context.s.thick/2}, context.mouse_current_position))
				draw(&context);
		EndTextureMode();
		BeginDrawing();
			ClearBackground((Color){ 40, 40, 40, 255 });

			BeginShaderMode(fxaa);
			// Draw canvas
			DrawTextureRec(
				canvas.texture,
			    (Rectangle){
			    	0,
			    	0,
			    	(float)canvas.texture.width,
			    	-(float)canvas.texture.height
			    }, // Flips the texture so it doesn't look upside down bc of opengl shit
				(Vector2){0, 0},
				WHITE
			);
			EndShaderMode();

			// Draw panel
			DrawRectangle(0, window_height - PANEL_HEIGHT, window_width, PANEL_HEIGHT, BLACK);
			DrawRectangleLines(0, window_height - PANEL_HEIGHT, window_width, PANEL_HEIGHT, GRAY);

			// Messages
			char* version_text = "Inkpad v0.6 DEV";
			Vector2 size = MeasureTextEx(global_font, version_text, 13.0f, 1.0f);
			DrawTextEx(global_font, version_text, (Vector2) { window_width-size.x-PANEL_PADDING, window_height-100+PANEL_PADDING }, 13.0f, 1.0f, GRAY);
			
			// Show Coordinates
			char pos_text[32];
			sprintf(pos_text, "X: %.2f Y: %.2f", context.mouse_current_position.x, context.mouse_current_position.y);
			DrawTextEx(global_font, pos_text, (Vector2) { 10, 10 }, 15.0f, 1.0f, WHITE);

			// Page number
			char page_number_text[16];
			sprintf(page_number_text, "%d/%d", page_selection+1, MAX_PAGES);
			DrawTextEx(global_font, page_number_text, (Vector2) { 20, window_height - PANEL_HEIGHT - 40 }, 25.0f, 1.0f, WHITE);

			// Input
			if (!context.text.active) {
				// Thickness Operations
				if (IsKeyPressed(KEY_ONE))   context.s.thick = DEFAULT_THICK;
				if (IsKeyPressed(KEY_TWO))   context.s.thick = DEFAULT_THICK + 5.0f;
				if (IsKeyPressed(KEY_THREE)) context.s.thick = DEFAULT_THICK + 10.0f;
				if (IsKeyPressed(KEY_FOUR))  context.s.thick = DEFAULT_THICK + 15.0f;
				if (IsKeyPressed(KEY_FIVE))  context.s.thick = DEFAULT_THICK + 20.0f;
				if (IsKeyPressed(KEY_ZERO))  context.s.thick = DEFAULT_THICK/2;

				// Fullscreen
				if (IsKeyPressed(KEY_F11)) {
					if (!IsWindowFullscreen()) {
						window_width = GetMonitorWidth(GetCurrentMonitor());
						window_height = GetMonitorHeight(GetCurrentMonitor());
					}
					SetWindowSize(window_width, window_height);
					ToggleFullscreen();
				}

				if (IsWindowResized()) {
					window_width = GetScreenWidth();
					window_height = GetScreenHeight();
				}
				
				// Change Thickness by Mouse wheel
				if (IsKeyDown(KEY_LEFT_ALT)) {
					double wheel = GetMouseWheelMove() * 3;
					if (wheel < 0) context.s.thick += context.s.thick >= DEFAULT_THICK/2 ? wheel : 0;
					else if (wheel > 0) context.s.thick += context.s.thick <= DEFAULT_THICK + 20.0f ? wheel : 0;
					show_stroke_tooltip(context.mouse_current_position, &context.s);
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
						BeginTextureMode(canvas);
						ClearBackground(BGCOLOR);				
						EndTextureMode();
						strcpy(status_text, "Cleared screen");
						status_timer = DEFAULT_SLEEP_TIME;
					} else {
						context.s.mode = MODE_ERASE;
					}
				}
				if (IsKeyPressed(KEY_SPACE)) context.s.mode = MODE_TEXT;
				if (IsKeyPressed(KEY_R))     context.s.mode = MODE_RECT;
				if (IsKeyPressed(KEY_C))     context.s.mode = MODE_CIRCLE;

				// Page shortcuts
				if (IsKeyPressed(KEY_PERIOD))
				{ page_selection = page_selection < MAX_PAGES-1 ? page_selection + 1 : page_selection;
			      canvas = pages[page_selection];
			      strcpy(status_text, TextFormat("Changed to page %d", page_selection + 1));
  	  			  status_timer = DEFAULT_SLEEP_TIME;
				}
				if (IsKeyPressed(KEY_COMMA))
				{ page_selection = page_selection > 0 ? page_selection - 1 : page_selection;
			      canvas = pages[page_selection];
				  strcpy(status_text, TextFormat("Changed to page %d", page_selection + 1));
	  			  status_timer = DEFAULT_SLEEP_TIME;
  				}
			}

			// Show stroke information
			DrawTextEx(global_font, "Stroke", (Vector2) { PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
			show_stroke(PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING+20, &context.s);
			
			// Color options
			// BB means Bounding Box, not Bubble Gum! You bastard!
			Rectangle bb = {0};
			int starting_pos = PANEL_PADDING + bb.x + bb.width + 200;
			DrawTextEx(global_font, "Colors", (Vector2) { starting_pos, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
			size_t len = sizeof(color_options)/sizeof(Color);
			for (size_t i = 0; i < len; i++) {
				draw_color_option(&bb, starting_pos + bb.width*i, window_height - PANEL_HEIGHT + PANEL_PADDING+20, color_options[i]);
				if (check_boundingbox(bb, context.mouse_current_position) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
					context.s.color = color_options[i];
				}
			}
			starting_pos += bb.width*len;

			// Draw page buttons
			starting_pos += PANEL_PADDING;
			DrawTextEx(global_font, "Pages", (Vector2) { starting_pos, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
			for (size_t i = 0; i < MAX_PAGES; i++) {
				draw_page_option(&bb, (size_t) page_selection == i, i+1, starting_pos + bb.width*i, window_height - PANEL_HEIGHT + PANEL_PADDING+20);
				if (check_boundingbox(bb, context.mouse_current_position) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
					page_selection = i;
					canvas = pages[page_selection];
					strcpy(status_text, TextFormat("Changed to page %d", page_selection + 1));
	  				status_timer = DEFAULT_SLEEP_TIME;
				}
			}
			starting_pos += bb.width*MAX_PAGES;

			// Save button
			starting_pos += PANEL_PADDING;
			DrawTextEx(global_font, "Save", (Vector2) { starting_pos, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
			draw_texture_button(&bb, save_icon, starting_pos, window_height - PANEL_HEIGHT + PANEL_PADDING+20);
			if (check_boundingbox(bb, context.mouse_current_position) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
				char* home = INKPAD_HOME;
				const char* filename = TextFormat("%s/inkpad_page%d.png", home, page_selection+1);
				save_canvas_as_image(canvas.texture, filename);
				strcpy(status_text, TextFormat("Saved canvas successfully as \"%s\".", filename));
				status_timer = DEFAULT_SLEEP_TIME;
			}
			starting_pos += bb.width;

			// Status text
			DrawTextEx(global_font, "Status", (Vector2) { starting_pos + PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
			DrawTextEx(global_font, status_text, (Vector2) { starting_pos + PANEL_PADDING + 5, window_height - PANEL_HEIGHT + PANEL_PADDING+25 }, window_width/100, 1.0f, GREEN);
			DrawRectangleLines(starting_pos + PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING+20, window_width - starting_pos - PANEL_PADDING*2, bb.height, WHITE);
			if (status_timer > 0) status_timer--;
			else memset(status_text, 0, sizeof(status_text));
			
			// Draw stroke preview
			draw_stroke_preview(&context);
		EndDrawing();
		context.mouse_last_position = context.mouse_current_position;
	}

	UnloadRenderTexture(canvas);
	CloseWindow();
	return 0;
}
