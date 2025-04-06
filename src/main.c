#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"

#define W_WID 1280
#define W_HEI 720
#define BGCOLOR (Color){32, 32, 32, 100}

// Layout constants
#define PANEL_PADDING 15
#define GRID_ROWS 20
#define GRID_COLS 20

// Stroke constants
#define DEFAULT_THICK 8.0f

typedef enum {
	MODE_FREE,
	MODE_LINE,
} Mode;

typedef struct {
	Mode mode;
	float thick;
	Color color;
} Stroke;

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
	int w = 70;
	int h = 70;
	DrawRectangleLines(x, y, w, h, WHITE);
	DrawCircleV((Vector2) { x + w/2, y + h/2 }, s->thick/2, s->color);
	DrawCircleLines(x + w/2, y + h/2, s->thick/2+4, s->color);
}

void draw_grid(int startX, int startY, int cellWidth, int cellHeight, int columns, int rows, Color color)
{
    for (int i = 0; i <= columns; i++) {
        int x = startX + i * cellWidth;
        DrawLine(x, startY, x, startY + rows * cellHeight, color);
    }
    for (int i = 0; i <= rows; i++) {
        int y = startY + i * cellHeight;
        DrawLine(startX, y, startX + columns * cellWidth, y, color);
    }
}

void draw_color_option(Rectangle* boundingbox, unsigned int x, unsigned int y, Color color) {
	DrawRectangle(x, y, 50, 50, color);
	DrawRectangleLinesEx((Rectangle){x-1, y-1, 52, 52}, 5.0f, GRAY);
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
	DrawLineEx(start, end, s->thick, s->color);
}

void draw_canvas(Stroke* s, Vector2 line[2], Vector2 mouse_current_position, Vector2 mouse_last_position) {
	switch (s->mode) {
	case MODE_FREE: {
		SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);
		if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			draw_stroke(mouse_last_position, mouse_current_position, s);
		}
		break;
	}
	case MODE_LINE: {
		SetMouseCursor(MOUSE_CURSOR_ARROW);
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			line[0] = mouse_current_position;
		}
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			line[1] = mouse_current_position;
			draw_line(line[0], line[1], s);
		}
		break;
	}
	}
	if (IsKeyDown(KEY_C)) {
		ClearBackground(BGCOLOR);	
	}
}

int main(void) {
	bool grid = true;
	Vector2 line[2];
	Stroke *s = &(Stroke){
		MODE_FREE,
		DEFAULT_THICK,
		GREEN,
	};
	Vector2 mouse_current_position, mouse_last_position;

	InitWindow(W_WID, W_HEI, "Inkpad");

	Color color_options[] = { WHITE, RED, GREEN, BLUE, PURPLE };
	RenderTexture2D canvas = LoadRenderTexture(W_WID, W_HEI - 100);
	
	SetTargetFPS(120);

	BeginTextureMode(canvas);
	ClearBackground(BGCOLOR);
	EndTextureMode();

	while (!WindowShouldClose()) {
		mouse_current_position = GetMousePosition();
		BeginTextureMode(canvas);
			draw_canvas(s, line, mouse_current_position, mouse_last_position);
		EndTextureMode();
		BeginDrawing();
			ClearBackground(BLACK);
			// Show stroke information
			show_stroke(PANEL_PADDING, canvas.texture.height + PANEL_PADDING, s);
			// Thickness Operations
			if (IsKeyPressed(KEY_ONE))   s->thick = DEFAULT_THICK;
			if (IsKeyPressed(KEY_TWO))   s->thick = DEFAULT_THICK + 5.0f;
			if (IsKeyPressed(KEY_THREE)) s->thick = DEFAULT_THICK + 10.0f;
			if (IsKeyPressed(KEY_FOUR))  s->thick = DEFAULT_THICK + 15.0f;
			if (IsKeyPressed(KEY_FIVE))  s->thick = DEFAULT_THICK + 20.0f;
			if (IsKeyPressed(KEY_ZERO))  s->thick = DEFAULT_THICK/2;

			// Modes
			if (IsKeyPressed(KEY_A))  s->mode = MODE_FREE;
			if (IsKeyPressed(KEY_L))  s->mode = MODE_LINE;

			// Draw grid
			if (IsKeyPressed(KEY_G)) grid = !grid;
			if (grid) draw_grid(0, 0, canvas.texture.width/GRID_ROWS, canvas.texture.height/GRID_COLS, GRID_ROWS, GRID_COLS, BGCOLOR);

			// Line preview draw
			if (s->mode == MODE_LINE && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
				draw_line(line[0], mouse_current_position, &(Stroke){
					0,
					1.0f,
					WHITE
				});
			}
			
			// Color options
			Rectangle bb = {0};
			int starting_pos = bb.x + bb.width + 80;
			for (size_t i = 0; i < sizeof(color_options)/sizeof(Color); i++) {
				draw_color_option(&bb, PANEL_PADDING + starting_pos + bb.width*i, canvas.texture.height + PANEL_PADDING, color_options[i]);
				if (check_boundingbox(bb, mouse_current_position) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
					s->color = color_options[i];
				}
			}

			// Draw canvas
			DrawTextureRec(
				canvas.texture,
			    (Rectangle){0, 0, (float)canvas.texture.width,
			    -(float)canvas.texture.height},
				(Vector2){0, 0},
				WHITE
			);
		EndDrawing();
		mouse_last_position = mouse_current_position;
	}

	UnloadRenderTexture(canvas);
	CloseWindow();
	return 0;
}
