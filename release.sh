#!/usr/bin/sh
PLATFORM=$1
if [ "$PLATFORM" = "win32" ]; then
	echo "Releasing for Windows"
	set -xe
	chmod +x ./build-windows.sh
	./build-windows.sh
	mkdir -p dist
	mkdir -p dist/x86_64-windows
	mv bin/inkpad-x86_64-windows.exe dist/x86_64-windows/inkpad.exe
	cp -R tutorial dist/x86_64-windows/
elif [ "$PLATFORM" = "linux" ]; then
	echo "Releasing for Linux"
	set -xe
	chmod +x ./build.sh
	./build.sh
	mkdir -p dist
	mkdir -p dist/x86_64-linux
	mv bin/inkpad-x86_64-linux dist/x86_64-linux/inkpad
	cp -R tutorial dist/x86_64-linux/
else
	echo "Invalid platform"
	exit 1
fi
