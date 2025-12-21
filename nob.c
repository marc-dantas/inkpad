#include <string.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

Cmd cmd = {0};

#define OUTPUT_DIR "bin/"
#define RELEASE_DIR "dist/"
#define INPUT_FILES "src/main.c"

bool release(const char* name, const char* build_path) {
	char* name_dir = strdup(temp_sprintf(RELEASE_DIR"%s", name));
	if (!mkdir_if_not_exists(RELEASE_DIR)) return false;
	if (!mkdir_if_not_exists(name_dir)) return false;
	if (!copy_directory_recursively(build_path, name_dir)) return false;

	cmd_append(&cmd, "zip", "-r", temp_sprintf("%s.zip", name), name_dir);

	return cmd_run(&cmd);
}

bool build_linux_x86_64(void) {
	const char* output = OUTPUT_DIR"linux-x86_64/inkpad";
	if (!mkdir_if_not_exists(OUTPUT_DIR"linux-x86_64")) return false;

	// basic compilation
	cmd_append(&cmd, "gcc", "-o", output, INPUT_FILES);
	
	// compiler flags
	cmd_append(&cmd, "-Wall", "-Wextra");
	
	// linking and include
	cmd_append(&cmd, "-I/usr/local/include", "-lraylib", "-lm");

	return cmd_run(&cmd);
}

bool build_windows_x86_64(void) {
	const char* output = OUTPUT_DIR"windows-x86_64/inkpad";
	if (!mkdir_if_not_exists(OUTPUT_DIR"windows-x86_64")) return false;
		
	// basic compilation
	
	cmd_append(&cmd, "x86_64-w64-mingw32-gcc-win32", "-o", OUTPUT_DIR"windows-x86_64/inkpad.exe", INPUT_FILES);
	
	// compiler flags
	cmd_append(&cmd, "-Wall", "-Wextra");

	// linking and include
	cmd_append(&cmd, "-I/usr/local/include", "-L./lib", "-lraylib", "-lwinmm", "-lgdi32");

	if (!cmd_run(&cmd)) return false;
	
	return true;
}

typedef struct {
	const char* name;
	bool (*build)(void);
} Platform;

static const Platform platforms[] = {
	{ // 0th item is the default platform to build
		.name = "linux-x86_64",
		.build = &build_linux_x86_64,
	},
	{
		.name = "windows-x86_64",
		.build = &build_windows_x86_64,
	},
};

bool delete_dir(const char* path) {
	File_Paths dir = {0};
	if (!nob_read_entire_dir(path, &dir)) return false;
	da_foreach(const char*, p, &dir) {
		if (strcmp(*p, ".") == 0 || strcmp(*p, "..") == 0) continue;

		const char* real_path = temp_sprintf("%s/%s", path, *p);
		File_Type ty = get_file_type(real_path);
		
		if (ty == NOB_FILE_REGULAR) {
			if (!delete_file(real_path)) return false;
		} else if (ty == NOB_FILE_DIRECTORY) {
			if (!delete_dir(real_path)) return false;
		}
	}
	if (!delete_file(path)) return false;
	return true;
}

// NOTE: Do not forget to delete files/folders here
// if you create them in other parts of the build system 
bool clean(void) {
	File_Paths dir = {0};
	if (!nob_read_entire_dir(get_current_dir_temp(), &dir)) return false;
	da_foreach(const char*, path, &dir) {
		String_View sv = sv_from_cstr(*path);
		if (sv_end_with(sv, ".zip")) {
			if (!delete_file(sv.data)) return false;
		}
	}
	
	if (!delete_dir(OUTPUT_DIR)) return false;
	if (!delete_dir(RELEASE_DIR)) return false;

	return true;
}

int main(int argc, char** argv) {
	NOB_GO_REBUILD_URSELF(argc, argv);

	if (!mkdir_if_not_exists(OUTPUT_DIR)) return 1;

	assert(ARRAY_LEN(platforms) >= 1);
	if (argc <= 1) {
		Platform plat = platforms[0];
		nob_log(NOB_WARNING, "Platform not provided. Defaulting to %s", plat.name);
		nob_log(NOB_INFO, "Building Inkpad for %s", plat.name);
		if (!(*plat.build)()) return 1;
		return 0;
	}

	if (strcmp(argv[1], "release") == 0) {
		for (size_t i = 0; i < ARRAY_LEN(platforms); i++) {
			Platform plat = platforms[i];
			nob_log(NOB_INFO, "Releasing Inkpad for %s", plat.name);
			if (argc < 3) {
				if (!(*plat.build)()) return 1;
				release(temp_sprintf("inkpad-%s", plat.name), temp_sprintf(OUTPUT_DIR"%s", plat.name));
			} else if (strcmp(argv[1], plat.name) == 0) {
				if (!(*plat.build)()) return 1;
				release(temp_sprintf("inkpad-%s", plat.name), temp_sprintf(OUTPUT_DIR"%s", plat.name));
				return 0;
			}
		}
		return 0;
	}
	
	for (size_t i = 0; i < ARRAY_LEN(platforms); i++) {
		Platform plat = platforms[i];
		if (strcmp(argv[1], plat.name) == 0) {
			nob_log(NOB_INFO, "Building Inkpad for %s", plat.name);
			if (!(*plat.build)()) return 1;
			return 0;
		}
	}

	if (strcmp(argv[1], "help") == 0) {
		nob_log(NOB_INFO, "Usage: nob COMMAND");
		nob_log(NOB_INFO, "Available commands:");
		printf("    PLATFORM: Build for a platform (see available platforms below)\n");
		printf("    `release [PLATFORM]`: Release for a platform (or all if not provided)\n");
		printf("    `clean`: Clean up build garbage\n");
		printf("    `help`: Show this help message\n");
		nob_log(NOB_INFO, "Available platforms:");
		for (size_t i = 0; i < ARRAY_LEN(platforms); i++) {
			printf("    `%s`: Build Inkpad for %s\n", platforms[i].name, platforms[i].name);
		}
		return 0;
	}

	if (strcmp(argv[1], "clean") == 0) {
		if (!clean()) return 1;
		return 0;
	}

	nob_log(NOB_ERROR, "Invalid platform or command `%s`", argv[1]);
	nob_log(NOB_INFO, "Use `help` to see help message");

	return 0;
}
