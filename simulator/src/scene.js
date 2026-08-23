// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The 3D view: the board's GLB (KiCad export, meshopt-compressed), a plane on
// the display's active area carrying the panel canvas as a texture, a glass
// plane over it, three LED planes with a soft additive glow, and invisible hit
// boxes over the four switches for clicks. Positions come from board.js
// (board coordinates), never from GLB node names; if the GLB does carry a
// node named SW1..SW4 the press animation sinks it 0.3 mm, otherwise only the
// hit box sinks (invisible) and the display still reacts.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { MeshoptDecoder } from 'three/addons/libs/meshopt_decoder.module.js';
import { BOARD, DISPLAY, BUTTONS, LEDS, MM } from './board.js';

const PRESS_DEPTH_MM = 0.3;
const toScene = (bx, by, h) => new THREE.Vector3(bx * MM, (BOARD.thick + h) * MM, by * MM);

export class BoardScene {
  constructor(canvas, displayCanvas, { onPress, onRelease }) {
    this.canvas = canvas; this.onPress = onPress; this.onRelease = onRelease;
    this.boardLoaded = false; this.enclosureLoaded = false;
    const renderer = this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance' });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.0;
    renderer.outputColorSpace = THREE.SRGBColorSpace;
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;

    const scene = this.scene = new THREE.Scene();
    scene.background = new THREE.Color(0x15171c);
    const pmrem = new THREE.PMREMGenerator(renderer);
    scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;   // procedural studio light, no HDRI file
    pmrem.dispose();

    this.target = toScene(BOARD.w / 2, BOARD.h / 2 - 2, 0);
    const camera = this.camera = new THREE.PerspectiveCamera(32, 1, 0.003, 3);
    const controls = this.controls = new OrbitControls(camera, canvas);
    controls.target.copy(this.target);
    controls.enableDamping = true; controls.dampingFactor = 0.12;
    controls.minDistance = 0.03; controls.maxDistance = 0.6;
    controls.maxPolarAngle = Math.PI / 2 - 0.05;   // never under the table
    this.threeQuarterView(false);

    const key = new THREE.DirectionalLight(0xffffff, 2.2);
    key.position.set(0.08, 0.25, 0.18); key.castShadow = true;
    key.shadow.mapSize.set(1024, 1024);
    key.shadow.camera.near = 0.05; key.shadow.camera.far = 0.8;
    key.shadow.camera.left = key.shadow.camera.bottom = -0.1; key.shadow.camera.right = key.shadow.camera.top = 0.1;
    key.shadow.bias = -0.0002;
    scene.add(key);
    scene.add(new THREE.AmbientLight(0xffffff, 0.25));
    // a "table" that only receives the contact shadow
    const floor = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), new THREE.ShadowMaterial({ opacity: 0.35 }));
    floor.rotation.x = -Math.PI / 2; floor.position.copy(toScene(BOARD.w / 2, BOARD.h / 2, -BOARD.thick - 0.0005));
    floor.receiveShadow = true; scene.add(floor);

    // display plane: the panel canvas as a texture
    const tex = this.displayTexture = new THREE.CanvasTexture(displayCanvas);
    tex.colorSpace = THREE.SRGBColorSpace; tex.minFilter = THREE.LinearFilter; tex.magFilter = THREE.LinearFilter;
    tex.generateMipmaps = false; tex.anisotropy = Math.min(8, renderer.capabilities.getMaxAnisotropy());
    this.displayMaterial = new THREE.MeshBasicMaterial({ map: tex, toneMapped: false });
    const plane = new THREE.Mesh(new THREE.PlaneGeometry(DISPLAY.w * MM, DISPLAY.h * MM), this.displayMaterial);
    plane.rotation.x = -Math.PI / 2;       // face up; the plane's +y (v = 1, top row) points to -z = the board's top edge
    plane.position.copy(toScene(DISPLAY.x0 + DISPLAY.w / 2, DISPLAY.y0 + DISPLAY.h / 2, DISPLAY.top + 0.03));
    plane.name = 'display'; scene.add(plane);
    // glass over it: a faint reflection from the environment
    const glass = new THREE.Mesh(new THREE.PlaneGeometry((DISPLAY.w + 3) * MM, (DISPLAY.h + 3) * MM),
      new THREE.MeshPhysicalMaterial({ color: 0xffffff, transparent: true, opacity: 0.10, roughness: 0.05, metalness: 0, envMapIntensity: 1.2, depthWrite: false }));
    glass.rotation.x = -Math.PI / 2; glass.position.copy(toScene(DISPLAY.x0 + DISPLAY.w / 2, DISPLAY.y0 + DISPLAY.h / 2, DISPLAY.glassTop + 0.08));
    scene.add(glass);

    // LEDs: a small emissive plane plus an additive glow sprite
    this.leds = LEDS.map((l) => {
      const mat = new THREE.MeshBasicMaterial({ color: l.colour, toneMapped: false });
      const m = new THREE.Mesh(new THREE.PlaneGeometry(1.2 * MM, 0.7 * MM), mat);
      m.rotation.x = -Math.PI / 2; m.position.copy(toScene(l.x, l.y, 0.75)); m.name = l.name; scene.add(m);
      const glow = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTexture(), color: l.colour, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, opacity: 0.0 }));
      glow.scale.set(6 * MM, 6 * MM, 1); glow.position.copy(toScene(l.x, l.y, 1.2)); scene.add(glow);
      return { ...l, mesh: m, mat, glow, on: null };
    });
    this.setLeds(0);

    // buttons: invisible hit boxes from board coordinates
    this.raycaster = new THREE.Raycaster();
    this.pointer = new THREE.Vector2();
    this.hitRestY = [];
    this.hits = BUTTONS.map((b) => {
      const m = new THREE.Mesh(new THREE.BoxGeometry(b.w * MM, 3 * MM, b.h * MM), new THREE.MeshBasicMaterial({ visible: false }));
      m.position.copy(toScene(b.x, b.y, b.top - 1.5)); m.userData.idx = b.idx; m.name = `hit-${b.name}`; scene.add(m);
      this.hitRestY[b.idx] = m.position.y;
      return m;
    });
    this.pressedIdx = null; this.buttonNodes = [null, null, null, null];
    this.buttonRestY = [null, null, null, null];
    this.sunk = [false, false, false, false];
    canvas.addEventListener('pointerdown', (e) => this._pointerDown(e));
    window.addEventListener('pointerup', () => this._pointerUp());
    window.addEventListener('pointercancel', () => this._pointerUp());

    // board: placeholder slab until (and unless) the GLB loads
    const slab = this.placeholder = new THREE.Mesh(new THREE.BoxGeometry(BOARD.w * MM, BOARD.thick * MM, BOARD.h * MM),
      new THREE.MeshStandardMaterial({ color: 0x2f6b3a, roughness: 0.6, metalness: 0.1 }));
    slab.position.copy(toScene(BOARD.w / 2, BOARD.h / 2, -BOARD.thick / 2)); slab.receiveShadow = true; slab.castShadow = true;
    scene.add(slab);

    this._resize();
    window.addEventListener('resize', () => this._resize());
    renderer.setAnimationLoop(() => { controls.update(); renderer.render(scene, camera); });
  }

  async loadBoard(url) {
    const loader = new GLTFLoader(); loader.setMeshoptDecoder(MeshoptDecoder);
    const gltf = await loader.loadAsync(url);
    gltf.scene.traverse((o) => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; } });
    this.scene.remove(this.placeholder);
    this.scene.add(gltf.scene);
    this.board = gltf.scene;
    for (let i = 0; i < 4; i++) {
      const node = gltf.scene.getObjectByName(BUTTONS[i].name) || null;
      this.buttonNodes[i] = node;
      if (node) {
        this.buttonRestY[i] = node.position.y;
        // a press held across the load (idx already sunk) must apply to the new node too
        node.position.y = this.buttonRestY[i] + (this.sunk[i] ? -PRESS_DEPTH_MM * MM : 0);
      }
    }
    this.boardLoaded = true;
    return gltf;
  }
  // Reserved for the enclosure: a second GLB in the same frame, toggled by the checkbox.
  async loadEnclosure(url) {
    const loader = new GLTFLoader(); loader.setMeshoptDecoder(MeshoptDecoder);
    const gltf = await loader.loadAsync(url);
    this.enclosure = gltf.scene; this.enclosure.visible = false; this.scene.add(this.enclosure);
    this.enclosureLoaded = true;
    return gltf;
  }
  setEnclosureVisible(v) { if (this.enclosure) this.enclosure.visible = !!v; }

  // display brightness follows the backlight PWM (26/255 = the dimmed state)
  setBacklight(level) { const k = Math.max(0.04, level / 255); this.displayMaterial.color.setScalar(k); }
  setLeds(bits) {
    for (const l of this.leds) {
      const on = (bits & l.bit) !== 0;
      if (on === l.on) continue;
      l.on = on; l.mat.color.set(on ? l.colour : 0x2a2a2a); l.glow.material.opacity = on ? 0.55 : 0.0;
    }
  }
  threeQuarterView(animate = true) { this._moveCamera(new THREE.Vector3(0.02, 0.125, 0.15), animate); }
  frontView(animate = true) { this._moveCamera(new THREE.Vector3(0, 0.19, 0.0008), animate); }
  _moveCamera(offset, animate) {
    const to = this.target.clone().add(offset);
    if (!animate) { this.camera.position.copy(to); this.controls.update(); return; }
    const from = this.camera.position.clone(); const t0 = performance.now();
    const step = (t) => { const k = Math.min(1, (t - t0) / 400); const e = 1 - Math.pow(1 - k, 3);
      this.camera.position.lerpVectors(from, to, e); this.controls.update(); if (k < 1) requestAnimationFrame(step); };
    requestAnimationFrame(step);
  }
  _resize() {
    const w = this.canvas.clientWidth || 800, h = this.canvas.clientHeight || 600;
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h; this.camera.updateProjectionMatrix();
  }
  _pickButton(e) {
    const r = this.canvas.getBoundingClientRect();
    this.pointer.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
    this.raycaster.setFromCamera(this.pointer, this.camera);
    const hit = this.raycaster.intersectObjects(this.hits, false)[0];
    return hit ? hit.object.userData.idx : null;
  }
  _pointerDown(e) {
    const idx = this._pickButton(e);
    if (idx === null) return;
    e.preventDefault();
    this.controls.enabled = false;          // a press is not an orbit
    this.pressedIdx = idx;
    this.onPress(idx);
  }
  _pointerUp() {
    if (this.pressedIdx === null) return;
    const idx = this.pressedIdx; this.pressedIdx = null;
    this.controls.enabled = true;
    this.onRelease(idx);
  }
  _sink(idx, down) {
    if (this.sunk[idx] === down) return;
    this.sunk[idx] = down;
    const off = down ? -PRESS_DEPTH_MM * MM : 0;
    this.hits[idx].position.y = this.hitRestY[idx] + off;
    if (this.buttonNodes[idx]) this.buttonNodes[idx].position.y = this.buttonRestY[idx] + off;
  }
  // Same as clicking the hit box, for the keyboard and the smoke test.
  pressByIndex(idx, down) { this._sink(idx, down); }
}

let _glowTex = null;
function glowTexture() {
  if (_glowTex) return _glowTex;
  const c = document.createElement('canvas'); c.width = c.height = 64;
  const g = c.getContext('2d'); const grd = g.createRadialGradient(32, 32, 0, 32, 32, 32);
  grd.addColorStop(0, 'rgba(255,255,255,1)'); grd.addColorStop(0.35, 'rgba(255,255,255,0.35)'); grd.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = grd; g.fillRect(0, 0, 64, 64);
  _glowTex = new THREE.CanvasTexture(c); return _glowTex;
}
