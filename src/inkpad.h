#ifndef INKPAD_H
#define INKPAD_H

// General constants
#define DEFAULT_BGCOLOR           (Color){ 15, 15, 15, 255 }
#define PANEL_BGCOLOR             (Color){ 18, 18, 18, 130 }
#define PANEL_PADDING             15         // Gap between elements inside the panel (pixels)
#define DEFAULT_THICK             8.0f       // Default thickness of stroke
#define PANEL_HEIGHT              90         // Height of the panel (pixels)
#define MAX_PAGES                 5          // Number of pages
#define DEFAULT_SLEEP_TIME        240        // Time (in frames) to show a caption in status
#define CANCEL_KEY                KEY_ESCAPE // Key to press to cancel action
#define DEFAULT_SMOOTH_LEASH_SIZE 4          // How long (delayed) is the pen in draw stroke mode (in pixels).
                                             // In other words, how much you have to drag the mouse to start drawing
#define MAX_SMOOTH_LEASH_SIZE     30         // Maximum smooth level to be applied
#define MAX_TEXT_SIZE             128        // Maximum text buffer size (in bytes) in Text mode

// Version
#define VERSION "0.8 DEV"
#define VERSION_NAME "Inkpad "VERSION

//// Misc ////

#define da(T) \
	T *items; \
	size_t count; \
	size_t capacity

//// Stroke ////

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
	int   smoothness; // value in pixels of the radius of smooth leash
} Stroke;

//// History ////

typedef enum {
	ACTION_ADD_ENTITY = 0,
	ACTION_CLEAR
} ActionKind;

typedef struct {
	ActionKind kind;
	union {
		size_t canvas_start;
		size_t entity_index;
	};
} Action;

typedef struct {
	da(Action);
	size_t top;
} History;

//// Entities ////

typedef enum {
	ENTITY_PATH = 0,
	ENTITY_LINE,
	ENTITY_RECT,
	ENTITY_CIRCLE,
	ENTITY_TEXT
} EntityKind;

typedef struct {
	EntityKind kind;
	Stroke     stroke;
	union {
		struct { da(Vector2);                                        } path;
		struct { Vector2   start;    Vector2 end;                    } line;
		struct { Rectangle bb;                                       } rect;
		struct { Vector2   center;   size_t  radius;                 } circle;
		struct { Vector2   position; char    content[MAX_TEXT_SIZE]; } text;
	};
	bool deleted;
} Entity;

//// Canvas ////

typedef struct {
	da(Entity);
	size_t          start; // Index to when to start iterating the entities
	History         history;
	RenderTexture2D cache;
	bool            redraw;
} Canvas;

//// Global Context ////

typedef struct {
	bool    cancel;                        // Flag to cancel the current stroke action being done
	bool    typing;                        // When in text mode during typing, this flag is true
	bool    show_panel;                    // Show panel (toggle w/ F11)
	size_t  timer;                         // General-purpose timer
	size_t  current_page;                  // Index of the current page selected
	char    status_caption[MAX_TEXT_SIZE]; // Status message/caption
	Mode    last_mode;                     // Last mode
	Mode    mode;                          // Current mode
	Canvas* canvas;                        // Current canvas object
	Entity  current_entity;                // Current entity to be saved between frames
	Stroke  s;                             // Current stroke state
	Vector2 last_point;                    // Used to save the previous point clicked while holding LMB
	Canvas* pages;                         // Pages pointer
	size_t  n_pages;                       // Number of pages
	Vector2 mouse_last_position, mouse_current_position; // Auto-describing
} Context;

#endif
