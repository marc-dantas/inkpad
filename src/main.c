#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#include "Px437_IBM_VGA_9x16.c"

#define W_WID GetScreenWidth()
#define W_HEI GetScreenHeight()
#define BGCOLOR (Color){18, 18, 18, 255}

// Layout constants
#define PANEL_PADDING 15
#define GRID_ROWS 20
#define GRID_COLS 20

// Stroke constants
#define DEFAULT_THICK 8.0f

typedef enum {
	MODE_FREE,
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

void show_stroke(unsigned int x, unsigned int y, Stroke* s) {
	int w = 50;
	int h = 50;
	DrawRectangleLines(x, y, w, h, WHITE);
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

void draw_button(Rectangle* boundingbox, unsigned int x, unsigned int y, Texture2D tex) {
	DrawTextureEx(tex, (Vector2){ x, y }, 0.0f, 50.0/tex.width, WHITE);
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

void draw_canvas(Stroke* s, Vector2* circle_center, TextState* text, Rectangle* rect, Vector2 line[2], Vector2 mouse_current_position, Vector2 mouse_last_position) {
	switch (s->mode) {
	case MODE_FREE: {
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			draw_stroke(mouse_last_position, mouse_current_position, s);
		}
		break;
	}
	case MODE_LINE: {
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			line[0] = mouse_current_position;
		}
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			line[1] = mouse_current_position;
			draw_line(line[0], line[1], s);
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
	if (IsKeyDown(KEY_C) && !text->active) {
		ClearBackground(BGCOLOR);
	}
}

void draw_stroke_preview(Vector2 pos, Vector2 line[2], Rectangle* rect, Vector2 circle_center, TextState* text, Stroke* s) {
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
			draw_line(line[0], pos, &(Stroke){
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

int main(void) {
	Vector2 line[2];
	Rectangle rect;
	Vector2 circle_center;
	Stroke *s = &(Stroke){
		MODE_FREE,
		DEFAULT_THICK,
		GREEN,
	};
	Mode quick_erase_saved_mode = s->mode;
	Vector2 mouse_current_position, mouse_last_position;

	InitWindow(W_WID, W_HEI, "Inkpad");
	ToggleFullscreen();
	HideCursor();

	global_font = LoadFont_Px437();
	Color color_options[] = { WHITE, BEIGE, RED, ORANGE, YELLOW, GREEN, LIME, SKYBLUE, BLUE, PURPLE };
	RenderTexture2D canvas = LoadRenderTexture(W_WID, W_HEI - 100);
	TextState text = {0};
	
	SetTargetFPS(120);

	BeginTextureMode(canvas);
	ClearBackground(BGCOLOR);
	EndTextureMode();

	while (!WindowShouldClose()) {
		mouse_current_position = GetMousePosition();
		BeginTextureMode(canvas);
			draw_canvas(s, &circle_center, &text, &rect, line, mouse_current_position, mouse_last_position);
		EndTextureMode();
		BeginDrawing();
			ClearBackground(BLACK);

			// Draw canvas
			DrawTextureRec(
				canvas.texture,
			    (Rectangle){0, 0, (float)canvas.texture.width,
			    -(float)canvas.texture.height},
				(Vector2){0, 0},
				WHITE
			);

			// Messages
			DrawTextEx(global_font, "(c) 2025 Marcio Dantas", (Vector2) { W_WID-210, W_HEI-20 }, 15.0f, 1.0f, WHITE);
			DrawTextEx(global_font, "Inkpad v0.2", (Vector2) { W_WID-210, W_HEI-40 }, 15.0f, 1.0f, WHITE);
			
			// Show Coordinates
			char pos_text[32];
			sprintf(pos_text, "X: %.2f Y: %.2f", mouse_current_position.x, mouse_current_position.y);
			DrawTextEx(global_font, pos_text, (Vector2) { 10, 10 }, 15.0f, 1.0f, WHITE);

			// Input
			if (!text.active) {
				// Thickness Operations
				if (IsKeyPressed(KEY_ONE))   s->thick = DEFAULT_THICK;
				if (IsKeyPressed(KEY_TWO))   s->thick = DEFAULT_THICK + 5.0f;
				if (IsKeyPressed(KEY_THREE)) s->thick = DEFAULT_THICK + 10.0f;
				if (IsKeyPressed(KEY_FOUR))  s->thick = DEFAULT_THICK + 15.0f;
				if (IsKeyPressed(KEY_FIVE))  s->thick = DEFAULT_THICK + 20.0f;
				if (IsKeyPressed(KEY_ZERO))  s->thick = DEFAULT_THICK/2;

				// Quick erase
				if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
				{ quick_erase_saved_mode = s->mode;
				  s->mode = MODE_ERASE;
			    }
				if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) s->mode = quick_erase_saved_mode;

				// Modes
				if (IsKeyPressed(KEY_A)) s->mode = MODE_FREE;
				if (IsKeyPressed(KEY_L)) s->mode = MODE_LINE;
				if (IsKeyPressed(KEY_X)) s->mode = MODE_ERASE;
				if (IsKeyPressed(KEY_T)) s->mode = MODE_TEXT;
				if (IsKeyPressed(KEY_R)) s->mode = MODE_RECT;
				if (IsKeyPressed(KEY_O)) s->mode = MODE_CIRCLE;
			}

			// Show stroke information
			DrawTextEx(global_font, "Stroke", (Vector2) { PANEL_PADDING, canvas.texture.height + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
			show_stroke(PANEL_PADDING, canvas.texture.height + 20 + PANEL_PADDING, s);
			
			// Color options
			Rectangle bb = {0};
			int starting_pos = bb.x + bb.width + 200;
			DrawTextEx(global_font, "Colors", (Vector2) { PANEL_PADDING + starting_pos, canvas.texture.height + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
			for (size_t i = 0; i < sizeof(color_options)/sizeof(Color); i++) {
				draw_color_option(&bb, PANEL_PADDING + starting_pos + bb.width*i, canvas.texture.height + 20 + PANEL_PADDING, color_options[i]);
				if (check_boundingbox(bb, mouse_current_position) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
					s->color = color_options[i];
				}
			}
			
			// Draw stroke preview
			draw_stroke_preview(mouse_current_position, line, &rect, circle_center, &text, s);	
		EndDrawing();
		mouse_last_position = mouse_current_position;
	}

	UnloadRenderTexture(canvas);
	CloseWindow();
	return 0;
}
