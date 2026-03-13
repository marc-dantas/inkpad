#ifndef ASSETS_H
#define ASSETS_H

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
	Image *items; \
	size_t count; \
	size_t capacity;

typedef enum {
	ASSET_PNG = 0,
	ASSET_TTF,
	ASSET_OTHER,
} AssetKind;

typedef struct {
	const char* path;
	const char* name;
	AssetKind kind;
} Asset;

typedef struct {
	da(Asset);
} Assets;

#endif
