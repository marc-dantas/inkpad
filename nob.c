#include <string.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

Cmd cmd = {0};

#define OUTPUT_DIR "bin/"
#define INPUT_FILES "src/main.c"

bool x86_64_gcc_linux(bool release) {
	const char* output = OUTPUT_DIR"inkpad";

	// basic compilation
	cmd_append(&cmd, "gcc", "-o", output, INPUT_FILES);
	
	// compiler flags
	cmd_append(&cmd, "-Wall", "-Wextra");
	
	// linking and include
	cmd_append(&cmd, "-I/usr/local/include", "-lraylib", "-lm");

	if (release) cmd_append(&cmd, "-O3");

	if (!cmd_run(&cmd)) return false;

	if (!release) return true;

	if (!mkdir_if_not_exists("dist")) return false;
	if (!mkdir_if_not_exists("dist/inkpad-x86_64-linux")) return false;
	if (!copy_file(output, "dist/inkpad-x86_64-linux/inkpad")) return false;
	if (!copy_directory_recursively("./tutorial/", "dist/inkpad-x86_64-linux")) return false;
	
	cmd_append(&cmd, "tar", "-czf", "dist/inkpad-x86_64-linux.tar.gz", "dist/inkpad-x86_64-linux");

	if (!cmd_run(&cmd)) return false;
	
	return true;
}

bool x86_64_gcc_mingw_windows(bool release) {
	// basic compilation
	cmd_append(&cmd, "x86_64-w64-mingw32-gcc-win32", "-o", OUTPUT_DIR"inkpad.exe", INPUT_FILES);
	
	// compiler flags
	cmd_append(&cmd, "-Wall", "-Wextra");

	// linking and include
	cmd_append(&cmd, "-I/usr/local/include", "-L./lib", "-lraylib", "-lwinmm", "-lgdi32");

	if (release) cmd_append(&cmd, "-O3");

	if (!cmd_run(&cmd)) return false;
	
	return true;
}

int main(int argc, char** argv) {
	NOB_GO_REBUILD_URSELF(argc, argv);

	if (!mkdir_if_not_exists(OUTPUT_DIR)) return 1;

	if (argc > 1 && strcmp(argv[1], "linux-x86_64") == 0) {
		nob_log(NOB_INFO, "Building for Linux x86_64");
		if (!x86_64_gcc_linux(false)) return 1;
	} else if (argc > 1 && strcmp(argv[1], "win-x86_64") == 0) {
		nob_log(NOB_INFO, "Building for Windows x86_64");
		if (!x86_64_gcc_mingw_windows(false)) return 1;
	} else if (argc > 1 && strcmp(argv[1], "release") == 0) {
		nob_log(NOB_INFO, "Releasing for all platforms");
		if (!x86_64_gcc_linux(true)) return 1;
		if (!x86_64_gcc_mingw_windows(true)) return 1;
	} else if (argc > 1 && strcmp(argv[1], "run") == 0) {
		nob_log(NOB_INFO, "Building + running Inkpad");
		if (!x86_64_gcc_linux(false)) return 1;
		cmd_append(&cmd, "./bin/inkpad");
		if (!cmd_run(&cmd)) return 1;
	} else {
		nob_log(NOB_WARNING, "Platform not provided. Defaulting to linux-x86_64");
		if (!x86_64_gcc_linux(false)) return 1;
	}

	nob_log(NOB_INFO, "Success");

	return 0;
}
