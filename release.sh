#!/usr/bin/sh
PLATFORM=$1
if [ "$PLATFORM" = "w" ]; then
	echo "Releasing for Windows"
	chmod +x ./build-windows.sh
	./build-windows.sh
	mkdir -p dist
	mkdir -p dist/inkpad-x64-windows
	mv bin/inkpad-x86_64-windows.exe dist/inkpad-x64-windows/inkpad.exe
	cp -R tutorial dist/inkpad-x64-windows/
	zip -r dist/inkpad-x64-windows.zip dist/inkpad-x64-windows
elif [ "$PLATFORM" = "l" ]; then
	echo "Releasing for Linux"
	chmod +x ./build-linux.sh
	./build.sh
	mkdir -p dist
	mkdir -p dist/inkpad-x86_64-linux
	mv bin/inkpad-x86_64-linux dist/inkpad-x86_64-linux/inkpad
	cp -R tutorial dist/inkpad-x86_64-linux/
	tar -czf dist/inkpad-x86_64-linux.tar.gz dist/inkpad-x86_64-linux
else
	echo "Invalid platform"
	exit 1
fi
echo "Done"
