# Inkpad
A simple drawing/whiteboarding software.

<img src="./screenshot.png" width="300" alt="Screenshot">

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
- Rect: Draw a rectangle
- Text: Draw arbitraty text on the canvas
- Erase: Erase drawings on the canvas

To enable the modes, use the following keys:
- Free: `A`
- Line: `L`
- Rect: `R`
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

### Clear screen
To clear the entire canvas, press `C`.

### Grid
To toggle grid behind the canvas, press `G`.

---

> By Marcio Dantas
