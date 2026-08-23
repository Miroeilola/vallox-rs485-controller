# Render scene for the enclosure draft

Everything under `docs/images/enclosure-draft-*` — the stills and the
exploded-view animation — comes from this directory. three.js scene, KiCad's
own colored GLB for the board, real STEP geometry for the enclosure and screws.

## Regenerate

```bash
# 1. board GLB with colors, tracks, silk and mask (origin at board 0,0)
kicad-cli pcb export glb --subst-models --no-dnp \
  --include-tracks --include-zones --include-pads --include-silkscreen --include-soldermask \
  --output <outdir>/pcb.glb ../../hardware/vallox-rs485-controller.kicad_pcb

# 2. enclosure + screws as GLBs (needs the CAD venv with build123d)
python export-render-glbs.py <outdir>

# 3. UI texture for the display
cp ../../firmware/test/host/golden/dashboard.png <outdir>/ui.png

# 4. serve and open
cp render.html <outdir>/ && cd <outdir> && python3 -m http.server 8931
# open http://localhost:8931/render.html
```

Console API of the page: `setView(az, el, dist, tx, ty, tz)`,
`setVisible({cover, screws, pcb, ...})`, `explodeAll(0..1)`,
`renderFrame(t)` (the 0..1 animation storyboard), `stats()` (fraction of
clipped pixels — keep `clipPct` at 0 before publishing), and
`captureRange(i0, i1, N)` which POSTs numbered PNG frames to
`frame-server.py <www-dir> <frames-dir>` (port 8932).

Animation assembly:

```bash
ffmpeg -framerate 30 -i frames/f%04d.png -c:v libx264 -preset slow -crf 18 \
  -pix_fmt yuv420p -movflags +faststart enclosure-draft-exploded-animation.mp4
```

## Notes

- The KiCad GLB marks **every** material metallic=1/rough=1; the page fixes the
  non-metals on load (see the `pcb` branch in the loader) — without it the board
  renders washed-out.
- The board GLB sits at the board origin; the scene lifts it +4.0 mm (the
  standoff in `../draft/enclosure.py`). The screws GLB is exported pre-placed.
- Lighting follows product-photography practice: one key direction, dim fill,
  cool rim, vignetted backdrop, faint floor reflection. Exposure is verified
  with `stats()`, not by eye.
