#include <stdio.h>
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

typedef struct {
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
	DrawRectangle(x, y, 30, 30, color);
	DrawRectangleLines(x-1, y-1, 32, 32, WHITE);
	boundingbox->x = x;
	boundingbox->y = y;
	boundingbox->width = 30;
	boundingbox->height = 30;
}

bool check_boundingbox(Rectangle bb, Vector2 pos) {
    return (pos.x >= bb.x) &&
           (pos.x <= bb.x + bb.width) &&
           (pos.y >= bb.y) &&
           (pos.y <= bb.y + bb.height);
}

int main(void) {
	bool grid = true;
	Stroke *s = &(Stroke){
		DEFAULT_THICK,
		GREEN,
	};
	Vector2 line_point;
	Vector2 mouse_current_position, mouse_last_position;

	InitWindow(W_WID, W_HEI, "Inkpad");

	SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);

	RenderTexture2D canvas = LoadRenderTexture(W_WID, W_HEI - 100);
	SetTargetFPS(120);

	BeginTextureMode(canvas);
	ClearBackground(BGCOLOR);
	EndTextureMode();

	while (!WindowShouldClose()) {
		mouse_current_position = GetMousePosition();
		BeginTextureMode(canvas);
			if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
				draw_stroke(mouse_last_position, mouse_current_position, s);
			}
			if (IsKeyDown(KEY_C)) {
				ClearBackground(BGCOLOR);	
			}
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

			// Draw grid
			if (IsKeyPressed(KEY_G)) grid = !grid;
			if (grid) draw_grid(0, 0, canvas.texture.width/GRID_ROWS, canvas.texture.height/GRID_COLS, GRID_ROWS, GRID_COLS, BGCOLOR);

			// Color options
			Color options[5] = { WHITE, RED, GREEN, BLUE, PURPLE };
			Rectangle bb;
			for (size_t i = 0; i < sizeof(options)/sizeof(Color); i++) {
				draw_color_option(&bb, PANEL_PADDING + 80 + bb.width*i, canvas.texture.height + PANEL_PADDING, options[i]);
				if (check_boundingbox(bb, mouse_current_position) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
					s->color = options[i];
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
