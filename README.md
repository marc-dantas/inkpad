# Inkpad
A simple drawing/whiteboarding software.

<img src="./screenshot.png" width="800" alt="Screenshot">

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

### Installation (optional)
If you are on Linux, you can move the compiled executable to `/usr/local/bin`
to be able to run it from the terminal, you can even create a `*.desktop` file
to make your system threat it as a proper application.

If you are on Windows, you can put it anywhere you want and then create a
shortcut to desktop, start menu and/or taskbar.

## Tutorial

### Modes
Inkpad works by changing **stroke modes**.
The modes that can be selected are the following:
- Free: Freehand draw
- Line: Draw a straight line
- Rect: Draw a rectangle
- Circle: Draw a circle
- Text: Draw arbitrary text on the canvas
- Erase: Erase drawings on the canvas

To enable the modes, use the following keys:
- Free: `A`
- Line: `L`
- Rect: `R`
- Circle: `O`
- Text: `T`
- Erase: `X`

The bottom **status panel** will show what mode you are currently in.

#### Usage of modes

**Free mode**
Free mode works like a normal pen.
Press left mouse button and drag the mouse with left mouse button down
and you will be able to draw freely. 

**Line mode**
Press and hold left mouse button to define the start of a
straight line. Release left mouse button to draw the desired line. 

**Rect mode**
Press and hold left mouse button to define the position of the
rectangle. Release left mouse button to draw the rectangle at that position. 

**Text mode**
Press left mouse button at the position you want to draw the text
and start typing the desired text, to erase the last character typed, press backspace.
When done, press enter to confirm and draw the text.

**Circle mode**
Press and hold left mouse button and drag it to define the radius of the circle.
Release the left mouse button to draw it. 

**Erase mode**
Press left mouse button and drag the mouse with left mouse button down
and you will be able to erase the drawing on the canvas. 

### Stroke properties
Stroke has 3 properties:
- Mode: Related to the mode you are currently in
- Thickness: Stroke thickness
- Color: Stroke color

To select a color for the stroke, click the desired color square in the status panel.

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

### Other

#### _Quick erase_
To quickly change to erase mode press and hold `right mouse button` and use `left mouse button` to erase.
To go back to the previous mode, release the `right mouse button`.

#### Clear screen
To clear the entire canvas, press `C`.

---

> By Marcio Dantas
