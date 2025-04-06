# Inkpad
A concise whiteboard software made to be simple.

![Screenshot](./screenshot.png)

## Getting started
### Dependencies
Inkpad depends on [Raylib](https://www.raylib.com/).

### Building
Build Inkpad for Linux using the script `build.sh` 
```console
$ chmod +x ./build.sh
$ ./build.sh
```

### Usage
After build, run the executable at `bin/` folder.
```console
$ ./bin/inkpad
```

## Tutorial

### Modes
Inkpad works by changing **stroke modes**.
The modes that can be selected are the following:
- Free: Freehand draw
- Line: Draw a straight line
- Erase: Erase drawings on the canvas

To enable the modes, use the following keys:
- Free: `A`
- Line: `L`
- Erase: `X`

The mouse cursor and the bottom **status panel** will show what mode you are currently in.

#### Usage of modes

**Free mode**
- Cursor: Cross

Free mode works like a normal pen.
Press left mouse button and drag the mouse with left mouse button down
and you will be able to draw freely. 

**Line mode**
- Cursor: Pointer

Press and hold left mouse button to define the start of a
straight line. Release left mouse button to draw the desired line. 

**Erase mode**
- Cursor: Not Allowed

Press left mouse button and drag the mouse with left mouse button down
and you will be able to erase the drawing on the canvas. 

### Stroke properties
Stroke has 3 properties:
- Mode: Related to the mode you are currently in
- Thickness: Stroke thickness
- Color: Stroke color

To select a color for the stroke, click the desired color square.

To set the thickness of the stroke, press one of the keys to set the thickness:
- `0`
- `1`
- `2`
- `3`
- `4`
- `5`

### Status panel
Status panel is the black area below the drawing canvas, it contains all information
about the current stroke and the color options.

### Grid
To show a grid behind the canvas, press `G`.

---

> By Marcio Dantas
