#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "raylib.h"
#include "raymath.h"
#include "assets.c"
#include "inkpad.h"

#ifdef _WIN32
#    define INKPAD_HOME getenv("USERPROFILE")
#else
#    define INKPAD_HOME getenv("HOME")
#endif

// Dynamic Array (stole from nob: https://github.com/tsoding/nob.h)
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


static Font global_font;

double clamped_increment(double x, double inc, double min, double max) {
    return ((x+inc) >= min && (x+inc) <= max)? inc : 0.0;
}

bool inkpad_color_eq(Color a, Color b) {
	return (a.r==b.r &&
			a.g==b.g &&
			a.b==b.b &&
			a.a==b.a);
}

bool check_boundingbox(Rectangle bb, Vector2 pos) {
    return (pos.x >= bb.x) &&
           (pos.x <= bb.x + bb.width) &&
           (pos.y >= bb.y) &&
           (pos.y <= bb.y + bb.height);
}

float point_segment_distance(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ab = { b.x - a.x, b.y - a.y };
    Vector2 ap = { p.x - a.x, p.y - a.y };
    float t = (ap.x * ab.x + ap.y * ab.y) / (ab.x * ab.x + ab.y * ab.y);
    t = fmaxf(0.0f, fminf(1.0f, t));
    Vector2 closest = { a.x + t * ab.x, a.y + t * ab.y };
    float dx = p.x - closest.x;
    float dy = p.y - closest.y;
    return sqrtf(dx*dx + dy*dy);
}

Rectangle inkpad_entity_calculate_bb(Entity entity) {
	switch (entity.kind) {
	case ENTITY_PATH: {
		Vector2 max = {0};
		Vector2 min = {0};
		for (size_t i = 0; i < entity.path.count; i++) {
			Vector2 point = entity.path.items[i];
			if (i == 0) min = point;
			if (point.x >= max.x) max.x = point.x;
			if (point.y >= max.y) max.y = point.y;
			if (min.x >= point.x) min.x = point.x;
			if (min.y >= point.y) min.y = point.y;
		}
		return (Rectangle) {
			min.x - entity.stroke.thick/2,
			min.y - entity.stroke.thick/2,
			max.x - min.x + entity.stroke.thick,
			max.y - min.y + entity.stroke.thick,
		};
	} break;
	case ENTITY_LINE: {
		return (Rectangle) {
			fminf(entity.line.start.x, entity.line.end.x) - entity.stroke.thick/2,
			fminf(entity.line.start.y, entity.line.end.y) - entity.stroke.thick/2,
			fmaxf(entity.line.start.x, entity.line.end.x) - fminf(entity.line.start.x, entity.line.end.x) + entity.stroke.thick,
			fmaxf(entity.line.start.y, entity.line.end.y) - fminf(entity.line.start.y, entity.line.end.y) + entity.stroke.thick,
		};
	} break;
	case ENTITY_RECT: {
		return (Rectangle) {
			entity.rect.bb.x - entity.stroke.thick/2 - 5,
			entity.rect.bb.y - entity.stroke.thick/2 - 5,
			entity.rect.bb.width + entity.stroke.thick + 10,
			entity.rect.bb.height + entity.stroke.thick + 10,
		};
	} break;
	case ENTITY_CIRCLE: {
		return (Rectangle) {
			entity.circle.center.x - entity.circle.radius - entity.stroke.thick,
			entity.circle.center.y - entity.circle.radius - entity.stroke.thick,
			entity.circle.radius*2 + entity.stroke.thick*2,
			entity.circle.radius*2 + entity.stroke.thick*2,
		};
	} break;
	case ENTITY_TEXT: {
		Vector2 size = MeasureTextEx(global_font, entity.text.content, entity.stroke.thick * 2, 1.0f);
		return (Rectangle) {
			entity.text.position.x,
			entity.text.position.y - entity.stroke.thick,
			size.x,
			size.y,
		};
	} break;
	}
}

void inkpad_entity_free(Entity* entity) {
	switch (entity->kind) {
	case ENTITY_PATH: {
		if (entity->path.items) free(entity->path.items);
		entity->path.items = NULL;
		entity->path.count = 0;
		entity->path.capacity = 0;
	} break;
	default:
		break;
	}
}

// gc: garbage-collect
void inkpad_canvas_history_gc(Canvas* canvas) {
	History* history = &canvas->history;
	if (history->top < history->count) {
		for (size_t i = history->top; i < history->count; i++) {
			Action action = history->items[i];
			if (action.kind == ACTION_ADD_ENTITY) {
				TraceLog(LOG_INFO, "Deallocating unreachable entity #%zu", action.entity_index+1);
				inkpad_entity_free(&canvas->items[action.entity_index]);
			}
		}
		history->count = history->top;
	}
}

void inkpad_canvas_add_entity(Canvas* canvas, Entity entity) {
	History* history = &canvas->history;

	inkpad_canvas_history_gc(canvas);

	size_t entity_index = canvas->count;
	da_append(canvas, entity);
	
	da_append(history, ((Action) {
		.kind = ACTION_ADD_ENTITY,
		.entity_index = entity_index,
	}));
	history->top++;
	
	canvas->redraw = true;
}

void inkpad_canvas_remove_entity(Canvas* canvas, size_t index) {
	History* history = &canvas->history;
	
	inkpad_canvas_history_gc(canvas);

	canvas->items[index].deleted = true;
	da_append(history, ((Action) {
		.kind = ACTION_REMOVE_ENTITY,
		.entity_index = index,
	}));
	history->top++;	
	canvas->redraw = true;
}

void inkpad_canvas_clear(Canvas* canvas) {
	History* history = &canvas->history;

	inkpad_canvas_history_gc(canvas);

	da_append(history, ((Action) {
		.kind = ACTION_CLEAR,
		.canvas_start = canvas->start
	}));
	history->top++;
	canvas->start = canvas->count;
	canvas->redraw = true;
}


bool inkpad_canvas_history_undo(Canvas* canvas) {
	History* history = &canvas->history;
	if (history->top <= 0) return false;
	Action action = history->items[--history->top];
	switch (action.kind) {
	case ACTION_ADD_ENTITY: {
		canvas->count--;
	} break;
	case ACTION_REMOVE_ENTITY: {
		canvas->items[action.entity_index].deleted = false;
	} break;
	case ACTION_CLEAR: {
		canvas->start = action.canvas_start;
	} break;
	}
	canvas->redraw = true;
	return true;
}

bool inkpad_canvas_history_redo(Canvas* canvas) {
	History* history = &canvas->history;
	if (history->top >= history->count) return false;
	Action action = history->items[history->top++];
	switch (action.kind) {
	case ACTION_ADD_ENTITY: {
		canvas->count++;
	} break;
	case ACTION_REMOVE_ENTITY: {
		canvas->items[action.entity_index].deleted = true;
	} break;
	case ACTION_CLEAR: {
		canvas->start = canvas->count;
	} break;
	}
	canvas->redraw = true;
	return true;
}

bool inkpad_entity_collision(Context* context, Vector2 position, size_t* target) {
	Canvas* canvas = context->canvas;
	for (size_t i = canvas->start; i < canvas->count; i++) {
		Entity entity = canvas->items[i];
		if (entity.deleted) continue;
		switch (entity.kind) {
		case ENTITY_PATH: {
			for (size_t j = 1; j < entity.path.count; j++) {
				Vector2 a = entity.path.items[j-1];
				Vector2 b = entity.path.items[j];
				float d = point_segment_distance(position, a, b);
				if (d <= entity.stroke.thick/2) {
					*target = i;
					return true;
				}
			}
		} break;
		case ENTITY_LINE: {
			if (point_segment_distance(position, entity.line.start, entity.line.end) <= entity.stroke.thick/2) {
				*target = i;
				return true;
			}
		} break;
		case ENTITY_RECT: {
		    Rectangle inner = entity.rect.bb;
			Rectangle outer = {
		        entity.rect.bb.x - entity.stroke.thick/2,
		        entity.rect.bb.y - entity.stroke.thick/2,
		        entity.rect.bb.width  + entity.stroke.thick,
		        entity.rect.bb.height + entity.stroke.thick,
		    };
		    if (check_boundingbox(outer, position) && !check_boundingbox(inner, position)) {
		        *target = i;
		        return true;
		    }
		} break;
		case ENTITY_CIRCLE: {
			size_t dist = Vector2Distance(position, entity.circle.center);
			size_t r = entity.circle.radius;
			if (dist <= r + entity.stroke.thick && dist >= r) {
				*target = i;
				return true;
			}
		} break;
		case ENTITY_TEXT: {
			if (check_boundingbox(entity.bb, position)) {
				*target = i;
				return true;
			}
		} break;
		}
	}
	return false;
}

void inkpad_draw_message(unsigned int x, unsigned int y, const char* text) {
	DrawText(text, x, y, 25, WHITE);
}

void inkpad_show_stroke_tooltip(Vector2 position, const char* text) {
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

void inkpad_show_stroke(Context* context, unsigned int x, unsigned int y) {
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

void inkpad_draw_color_option(Rectangle* boundingbox, unsigned int x, unsigned int y, bool selected, Color color) {
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

void inkpad_draw_page_option(Rectangle* boundingbox, bool selected, int number, unsigned int x, unsigned int y) {
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

void inkpad_draw_path(Vector2 start, Vector2 end, Stroke s) {
	float thick = s.thick;
	Color color = s.color;
	DrawCircleV(start, thick/2, color);
	DrawLineEx(start, end, thick, color);
	DrawCircleV(end, thick/2, color);
}

void inkpad_draw_line(Vector2 start, Vector2 end, Stroke s) {
	DrawCircleV(start, s.thick/2, s.color);
	DrawLineEx(start, end, s.thick, s.color);
	DrawCircleV(end, s.thick/2, s.color);
}

void inkpad_draw_rect(Rectangle rect, Stroke s) {
	DrawRectangleRoundedLinesEx(rect, 0.01f, 15, s.thick, s.color);
}

// Main draw function that handles when you click LMB
void inkpad_draw(Context* context) {
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
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			ent->bb = inkpad_entity_calculate_bb(*ent);
			entity_done = true;
		}
		
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
			ent->bb = inkpad_entity_calculate_bb(*ent);
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
			ent->bb = inkpad_entity_calculate_bb(*ent);
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
				ent->bb = inkpad_entity_calculate_bb(*ent);
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
	case MODE_ERASE: {
		size_t entity_index = 0;
		if (inkpad_entity_collision(context, mouse_current_position, &entity_index)) {
			DrawRectangleLinesEx(context->canvas->items[entity_index].bb, 1.0f, WHITE);
			if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
				inkpad_canvas_remove_entity(context->canvas, entity_index);
		}
	} break;
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
			ent->bb = inkpad_entity_calculate_bb(*ent);
			entity_done = true;
		}
		break;
	}
	if (entity_done) {
		inkpad_canvas_add_entity(context->canvas, context->current_entity);
		TraceLog(LOG_INFO, "Added entity #%zu", context->canvas->count);
		memset(&context->current_entity, 0, sizeof(Entity));
	}
}

// Function to handle the drawing of the stroke before you press LMB (preview)
void inkpad_draw_preview(Context* context) {
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
				inkpad_draw_path(entity.path.items[j], entity.path.items[j+1], entity.stroke);
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
			inkpad_draw_line(*last_point, pos, (Stroke) {
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
			inkpad_draw_rect(rect, (Stroke) {
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

void inkpad_set_status_caption(Context* context, const char* message) {
	strcpy(context->status_caption, message);
	context->timer = DEFAULT_SLEEP_TIME;
}

void inkpad_debug_text(char* txt, int x, int y) {
	DrawTextEx(global_font, txt, (Vector2) { x, y }, 20.0f, 1.0f, GREEN);
}

void inkpad_draw_panel(
	Context* context,
	int window_width,
	int window_height,
	Color* color_options,
	int n_color_options
) {
	DrawRectangle(0, window_height - PANEL_HEIGHT, window_width, PANEL_HEIGHT, PANEL_BGCOLOR);
	DrawRectangleLines(0, window_height - PANEL_HEIGHT, window_width, PANEL_HEIGHT, GRAY);
	// Show stroke information
	DrawTextEx(global_font, "Stroke", (Vector2) { PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
	inkpad_show_stroke(context, PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING+20);

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
		bool selected = inkpad_color_eq(context->s.color, color_options[i]);
		inkpad_draw_color_option(&bb, starting_pos + bb.width*i, window_height - PANEL_HEIGHT + PANEL_PADDING+20, selected, color_options[i]);
		if (check_boundingbox(bb, context->mouse_current_position) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			context->s.color = color_options[i];
		}
	}
	starting_pos += bb.width*len;

	// Draw page buttons
	starting_pos += PANEL_PADDING;
	DrawTextEx(global_font, "Pages", (Vector2) { starting_pos, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
	for (size_t i = 0; i < context->n_pages; i++) {
		inkpad_draw_page_option(&bb, (size_t) context->current_page == i, i+1, starting_pos + bb.width*i, window_height - PANEL_HEIGHT + PANEL_PADDING+20);
		if (check_boundingbox(bb, context->mouse_current_position) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			context->current_page = i;
			context->canvas = &context->pages[context->current_page];
			inkpad_set_status_caption(context, TextFormat("Changed to page %d", context->current_page + 1));
		}
	}
	starting_pos += bb.width*MAX_PAGES;

	// Status text
	DrawTextEx(global_font, "Status", (Vector2) { starting_pos + PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING }, 17.0f, 1.0f, (Color) {210, 210, 210, 255});
	DrawTextEx(global_font, context->status_caption, (Vector2) { starting_pos + PANEL_PADDING + 5, window_height - PANEL_HEIGHT + PANEL_PADDING+25 }, window_width/100, 1.0f, GREEN);
	DrawRectangleLines(starting_pos + PANEL_PADDING, window_height - PANEL_HEIGHT + PANEL_PADDING+20, window_width - starting_pos - PANEL_PADDING*2, bb.height, WHITE);

	if (context->timer > 0) context->timer--;
	else memset(context->status_caption, 0, MAX_TEXT_SIZE);
}

// Render canvas' cache texture on the screen
void inkpad_render_canvas(const Canvas* canvas, int x, int y) {
	Texture2D tex = canvas->cache.texture;
	DrawTextureRec(
		tex,
		(Rectangle){
			0, 0,
			(float)tex.width,
			(float)-tex.height
		}, // Flipped rectangle to fix OpenGL's coordinate system
		(Vector2){ x, y },
		WHITE
	);
}

// Redraw the canvas' elements into its cache texture if refresh is needed
void inkpad_refresh_canvas(Canvas* canvas) {
	if (!canvas->redraw) return;
	BeginTextureMode(canvas->cache);
	ClearBackground(BLANK);
	for (size_t i = canvas->start; i < canvas->count; i++) {
		Entity entity = canvas->items[i];
		if (entity.deleted) continue;
		switch (entity.kind) {
		case ENTITY_PATH: {
			if (entity.path.count <= 0) break;
			for (size_t j = 0; j < entity.path.count - 1; j++) {
				inkpad_draw_path(entity.path.items[j], entity.path.items[j+1], entity.stroke);
			}
		} break;
		case ENTITY_LINE: {
			inkpad_draw_line(entity.line.start, entity.line.end, entity.stroke);
		} break;
		case ENTITY_RECT: {
			inkpad_draw_rect(entity.rect.bb, entity.stroke);
		} break;
		case ENTITY_CIRCLE: {
			DrawRing(entity.circle.center, entity.circle.radius, entity.circle.radius + entity.stroke.thick, 0, 360, 60, entity.stroke.color);
		} break;
		case ENTITY_TEXT: {
			Vector2 offset_text_pos = {
				entity.text.position.x,
				entity.text.position.y - entity.stroke.thick
			};
			DrawTextEx(global_font, entity.text.content, offset_text_pos, entity.stroke.thick * 2, 1.0f, entity.stroke.color);
		} break;
		}
	}
	canvas->redraw = false;
	EndTextureMode();
}

bool inkpad_main(Context* context, size_t* window_width, size_t* window_height) {
	static Color color_options[] = { BLACK, WHITE, BEIGE, RED, ORANGE, YELLOW, GREEN, LIME, SKYBLUE, BLUE, PURPLE };

	context->mouse_current_position = GetMousePosition();

	inkpad_refresh_canvas(context->canvas);
	
	bool is_on_canvas = check_boundingbox(
		(Rectangle) {
			0, 0,
			*window_width,
			*window_height - (context->show_panel ? PANEL_HEIGHT : 0) - context->s.thick/2
		},
		context->mouse_current_position
	);
	if (is_on_canvas) {
		inkpad_draw(context);
	}

	// Canvas rendered image drawing
	ClearBackground(DEFAULT_BGCOLOR);
	inkpad_render_canvas(context->canvas, 0, 0);
	
	if (context->canvas->redraw) {
		inkpad_refresh_canvas(context->canvas);
		inkpad_render_canvas(context->canvas, 0, 0);
	}

	if (context->show_panel) inkpad_draw_panel(
		context,
		*window_width,
		*window_height,
		color_options,
		sizeof(color_options)/sizeof(Color)
	);
	
	// Show Coordinates
	char pos_text[32];
	sprintf(pos_text, "X: %.2f Y: %.2f", context->mouse_current_position.x, context->mouse_current_position.y);
	DrawTextEx(global_font, pos_text, (Vector2) { 10, 10 }, 15.0f, 1.0f, WHITE);

	// Page number
	char page_number_text[16];
	sprintf(page_number_text, "%zu/%d", context->current_page+1, MAX_PAGES);
	int height = MeasureTextEx(global_font, page_number_text, 25.0f, 1.0f).y;
	DrawTextEx(
		global_font,
		page_number_text,
		(Vector2) {
			PANEL_PADDING,
			*window_height - (context->show_panel ? PANEL_HEIGHT : 0) - height - PANEL_PADDING
		},
		25.0f,
		1.0f,
		WHITE
	);

	// Input
	if (!context->typing) {
		// Thickness Operations
		if (IsKeyPressed(KEY_ONE))   context->s.thick = DEFAULT_THICK;
		if (IsKeyPressed(KEY_TWO))   context->s.thick = DEFAULT_THICK + 5.0f;
		if (IsKeyPressed(KEY_THREE)) context->s.thick = DEFAULT_THICK + 10.0f;
		if (IsKeyPressed(KEY_FOUR))  context->s.thick = DEFAULT_THICK + 15.0f;
		if (IsKeyPressed(KEY_FIVE))  context->s.thick = DEFAULT_THICK + 20.0f;
		if (IsKeyPressed(KEY_ZERO))  context->s.thick = DEFAULT_THICK/2;

		// Fullscreen and window resizing
		if (IsKeyPressed(KEY_F11)) {
			context->show_panel = !context->show_panel;
		}
		
		if (IsWindowResized()) {
			*window_width = GetScreenWidth();
			*window_height = GetScreenHeight();
		}

		// Quit and Cancel key
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) return false;
		if (context->cancel) inkpad_set_status_caption(context, "Action cancelled");

		// Undo
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z)) {
			if (inkpad_canvas_history_undo(context->canvas))
				inkpad_set_status_caption(context, "Undid action");
		}

		// Redo
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Y)) {
			if (inkpad_canvas_history_redo(context->canvas))
				inkpad_set_status_caption(context, "Redid action");
		}
		
		// Change Thickness by Mouse wheel (LEFT ALT)
		if (IsKeyDown(KEY_LEFT_ALT)) {
			double wheel = GetMouseWheelMove() * 4;
			context->s.thick += clamped_increment(context->s.thick, wheel, DEFAULT_THICK/2, DEFAULT_THICK+20.0);
			Vector2 pos = (Vector2){context->mouse_current_position.x+context->s.thick/2, context->mouse_current_position.y};
			inkpad_show_stroke_tooltip(pos, TextFormat("Thickness: %.2f", context->s.thick));
		}

		// Change color by Mouse wheel (TAB)
		if (IsKeyDown(KEY_TAB)) {
			int wheel = (int)GetMouseWheelMove();
			size_t len = sizeof(color_options)/sizeof(Color);
			Color color = context->s.color;
			size_t index = 0;
			for (size_t i = 0; i < len; i++) {
				if (inkpad_color_eq(color, color_options[i])) {
					index = i;
				}
			}
			Vector2 pos = context->mouse_current_position;
			size_t pad = context->s.thick/2;

			size_t selected = (index-wheel);
			inkpad_draw_color_option(NULL, pos.x + pad, pos.y + pad, false, color);					
			context->s.color = color_options[selected <= len ? selected : len-1];
		}

		// Stroke smoothness by Mouse wheel
		if (IsKeyDown(KEY_S) && context->mode == MODE_DRAW) {
			double wheel = GetMouseWheelMove();
			context->s.smoothness += clamped_increment(context->s.smoothness, wheel, 0, MAX_SMOOTH_LEASH_SIZE);
			DrawCircleLinesV(context->mouse_current_position, context->s.smoothness, context->s.color);
			Vector2 pos = (Vector2){context->mouse_current_position.x+context->s.smoothness, context->mouse_current_position.y};
			inkpad_show_stroke_tooltip(pos, TextFormat("Smoothness: %.2f", (float)context->s.smoothness));
		}
		
		// Quick erase
		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
		{ context->last_mode = context->mode;
		  context->mode = MODE_ERASE;
		  context->s.thick *= 2;
	    }
		if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
		{ context->mode = context->last_mode;
	      context->s.thick /= 2;
		}

		// Quick line
	    if (IsKeyPressed(KEY_LEFT_SHIFT))
	    { context->last_mode = context->mode;
	      context->mode = MODE_LINE;
	    }
		if (IsKeyReleased(KEY_LEFT_SHIFT)) context->mode = context->last_mode;

		// Modes
		if (IsKeyPressed(KEY_A)) context->mode = MODE_DRAW;
		if (IsKeyPressed(KEY_L)) context->mode = MODE_LINE;
		if (IsKeyPressed(KEY_X)) {
			if (IsKeyDown(KEY_LEFT_CONTROL)) {
				inkpad_canvas_clear(context->canvas);
				inkpad_set_status_caption(context, "Cleared screen");
			} else {
				context->mode = MODE_ERASE;
			}
		}
		if (IsKeyPressed(KEY_SPACE)) context->mode = MODE_TEXT;
		if (IsKeyPressed(KEY_R))     context->mode = MODE_RECT;
		if (IsKeyPressed(KEY_C))     context->mode = MODE_CIRCLE;

		// Page shortcuts
		if (IsKeyPressed(KEY_PERIOD))
		{ context->current_page = context->current_page < MAX_PAGES-1 ? context->current_page + 1 : context->current_page;
	      context->canvas = &context->pages[context->current_page];
	      inkpad_set_status_caption(context, TextFormat("Changed to page %d", context->current_page + 1));
		}
		if (IsKeyPressed(KEY_COMMA))
		{ context->current_page = context->current_page > 0 ? context->current_page - 1 : context->current_page;
	      context->canvas = &context->pages[context->current_page];
	      inkpad_set_status_caption(context, TextFormat("Changed to page %d", context->current_page + 1));
		}
	}
	
	// Draw stroke preview
	if (!is_on_canvas)
		ShowCursor();
	else {
		HideCursor();
		inkpad_draw_preview(context);
	}
	
	context->mouse_last_position = context->mouse_current_position;
	
	return true;
}

void inkpad_context_init(Context* context, size_t window_width, size_t window_height) {
	context->s = (Stroke){
		.thick = DEFAULT_THICK,
		.color = GREEN,
		.smoothness = DEFAULT_SMOOTH_LEASH_SIZE,
	};
	context->mode = MODE_DRAW;
	static Canvas pages[MAX_PAGES] = {0};
	for (size_t i = 0; i < MAX_PAGES; i++) {
		pages[i] = (Canvas){ .cache = LoadRenderTexture(window_width, window_height), .redraw = false };
	}
	context->pages = pages;
	context->n_pages = MAX_PAGES;
	context->current_page = 0;
	context->canvas = &context->pages[context->current_page];
	context->show_panel = true;
}

int main(void) {
	// Global context
	static Context context = {0};

	// Initialization
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_MAXIMIZED);
	InitWindow(0, 0, "Inkpad "VERSION);
	SetWindowMinSize(640, 480);
	SetTargetFPS(120);
	SetExitKey(0);

	size_t window_width = GetScreenWidth();
	size_t window_height = GetScreenHeight();

#ifdef DEBUG
	TraceLog(LOG_INFO, "HOME DIRECTORY: %s", INKPAD_HOME);
	TraceLog(LOG_INFO, "Window Width: %zu", window_width);
	TraceLog(LOG_INFO, "Window Height: %zu", window_height);
#endif

	// Loading assets and configuration
	global_font = LoadFontFromMemory(".ttf", IBMPlexMono_SemiBold_ttf, IBMPlexMono_SemiBold_size, 100, NULL, 0);;;;;;

	inkpad_context_init(&context, window_width, window_height);
	
	// Welcome message
	inkpad_set_status_caption(&context, "Welcome to Inkpad. Press F11 to hide the Panel.");
	
	// Window
	while (!WindowShouldClose()) {
		BeginDrawing();
			bool _continue = inkpad_main(&context, &window_width, &window_height);
			if (!_continue) break;
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
