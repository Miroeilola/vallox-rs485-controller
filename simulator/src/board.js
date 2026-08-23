// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Where things are on the rev A board, in KiCad board coordinates: millimetres,
// origin at the top-left corner of the outline, x to the right, y DOWN (the
// KiCad convention). Read from the committed hardware/vallox-rs485-controller.kicad_pcb
// (kicad-cli pcb export pos) and the DS1 footprint; not from the GLB's node
// names, which the optimiser may drop.
//
// The GLB exported with `--user-origin 0x0mm` puts board x on its x axis, board
// y on its +z axis and height on +y (board top face at y = BOARD.thick mm).
export const BOARD = { w: 104, h: 66, thick: 1.595 };

// DS1 active area (30.6 x 40.8 mm module AA, landscape on the board): footprint
// at (33.9, 26.1) rotated 90°, AA rect local x ±15.3, y -23.26..17.54.
// Pixel (0,0) is the top-left corner; 0.1275 mm per pixel both ways.
export const DISPLAY = { x0: 10.64, y0: 10.8, w: 40.8, h: 30.6, top: 2.05, glassTop: 2.05 };

// SW1..SW4 = − + OK ←, 13 mm pitch, PTS645 6 x 6 mm tact, cap top ~5 mm above the board.
export const BUTTONS = [14.5, 27.5, 40.5, 53.5].map((x, i) => ({ idx: i, x, y: 52, w: 6.4, h: 6.4, top: 5.0, name: ['SW1', 'SW2', 'SW3', 'SW4'][i] }));

// D4 PWR (yellow-green), D6 BUS (yellow), D5 FAULT (red); 0603, lens ~0.6 mm up.
export const LEDS = [
  { bit: 1, x: 63.8, y: 7.8,  colour: 0x9dff3a, name: 'D4 PWR' },
  { bit: 2, x: 63.8, y: 10.8, colour: 0xffd23a, name: 'D6 BUS' },
  { bit: 4, x: 63.8, y: 13.8, colour: 0xff3a3a, name: 'D5 FAULT' },
];

export const MM = 0.001;   // the GLB is in metres
