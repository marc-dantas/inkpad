# Inkpad
A whiteboard tool for programmers.

<img src="./screenshot.png" width="800" alt="Screenshot">

## Getting started

### Download Inkpad
Get pre-built Inkpad binaries in the [releases page](https://github.com/marc-dantas/inkpad/releases/latest).

### Building from source-code

#### Dependencies
Inkpad depends on [Raylib](https://www.raylib.com/).

#### Instruction
Clone the repository and build Inkpad using [`nob`](https://github.com/tsoding/nob.h).

Provide one of the following command line arguments below to `nob` executable to build for different platforms:
- `linux-x86_64`: build for Linux x86_64
- `windows-x86_64`: build for Windows x86_64 (using [mingw-w64](https://en.wikipedia.org/wiki/MinGW))

If you do not provide any of them, the default is Linux x86_64.

If it is your first time building the same version, you need to pack the necessary assets for the
software to work. In case you don't, the build will not succeed.

To pack the assets, use the built-in asset packer:
```console
./nob pack
```

---

In a nutshell, this is what you need to do:

```console
$ cc -o nob nob.c
$ ./nob pack && ./nob
```

After build, run the executable at `bin/` folder.
```console
$ ./bin/inkpad
```

#### Installation (optional)
If you are on Linux, you can move the compiled executable to `/usr/local/bin`
to be able to run it from the terminal, you can even create a `*.desktop` file
to make your system threat it as a proper application.

If you are on Windows, you can put it anywhere you want and then create a
shortcut to desktop, start menu and/or taskbar.

## Tutorial
Read the full tutorial [here](./tutorial/).

---

> By Marcio Dantas
