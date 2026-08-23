// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The side panel: machine state read from the model, controls written to it,
// fault injection, panel language, the LED and backlight readouts, and the bus
// log formatted from the raw six-byte frames. Updated from the animation loop
// at a lower rate than the display (every 6th frame ≈ 10 Hz); the log appends
// only new entries.
const ADDR_NAMES = { 0x10: 'all machines', 0x11: 'machine', 0x20: 'all panels', 0x21: 'panel', 0x28: 'LON' };
const hex = (b) => b.toString(16).toUpperCase().padStart(2, '0');
const addrName = (a) => ADDR_NAMES[a] ?? (a >= 0x21 && a <= 0x29 ? `panel ${a - 0x20}` : `0x${hex(a)}`);

export function formatFrame(sim, e) {
  const [, sender, receiver, reg, value] = e.raw;
  const raw = e.raw.map(hex).join(' ');
  const t = (e.t / 1000).toFixed(2).padStart(8);
  let what;
  if (reg === 0x00) what = `poll ${sim.regName(value) || '0x' + hex(value)}`;
  else what = `${sim.regName(reg) || '0x' + hex(reg)} = 0x${hex(value)}`;
  const arrow = e.dir === 0 ? '→' : '←';
  return `${t}  ${raw}  ${addrName(sender)} ${arrow} ${addrName(receiver)}  ${what}`;
}

export class SidePanel {
  constructor(sim, root, { onRestart, onLang }) {
    this.sim = sim; this.root = root; this.onRestart = onRestart; this.onLang = onLang;
    this.$ = (id) => root.querySelector('#' + id);
    this.lastLogSeq = 0; this.frame = 0;
    this.$('ms-version').textContent = sim.version();
    // controls → machine
    const outdoor = this.$('in-outdoor'), outOutdoor = this.$('out-outdoor');
    const applyOutdoor = () => { sim.setOutdoor(Number(outdoor.value)); outOutdoor.textContent = `${outdoor.value} °C`; };
    outdoor.addEventListener('input', applyOutdoor); applyOutdoor();
    this.$('in-timescale').addEventListener('change', (e) => sim.setTimeScale(Number(e.target.value)));
    this.$('in-delay').addEventListener('change', (e) => sim.setReplyDelay(Number(e.target.value)));
    this.$('in-lang').addEventListener('change', (e) => { sim.uiSetLang(Number(e.target.value)); onLang?.(Number(e.target.value)); });
    this.$('in-fault').addEventListener('change', (e) => { const c = Number(e.target.value); if (c) sim.fault(c); else sim.faultClear(); });
    this.$('btn-reset').addEventListener('click', () => onRestart?.());
  }
  // Called after sim_init() (and on restart): controls re-applied to the fresh model.
  applyControls() {
    this.sim.setOutdoor(Number(this.$('in-outdoor').value));
    this.sim.setTimeScale(Number(this.$('in-timescale').value));
    this.sim.setReplyDelay(Number(this.$('in-delay').value));
    const c = Number(this.$('in-fault').value); if (c) this.sim.fault(c);
    this.$('in-lang').value = String(this.sim.uiLang());
    this.lastLogSeq = 0; this.$('bus-log').textContent = '';
  }
  update() {
    if (++this.frame % 6) return;
    const sim = this.sim, $ = this.$;
    const speed = sim.fanSpeed();
    $('ms-speed').textContent = speed >= 1 && speed <= 8 ? `${speed} / 8` : '–';
    for (let i = 0; i < 4; i++) $(`ms-t${i}`).textContent = `${sim.temp(i).toFixed(1)} °C`;
    const f = sim.flags();
    $('ms-heater').textContent = f & 1 ? 'on' : 'off';
    $('ms-bypass').textContent = f & 2 ? 'summer bypass' : 'heat recovery';
    $('ms-frost').textContent = f & 4 ? 'supply fan stopped' : 'no';
    const fault = sim.reg(0x36);
    $('ms-fault').textContent = fault > 0 ? `0x${hex(fault)} ${sim.faultName(fault)}` : 'none';
    const leds = sim.leds();
    $('led-pwr').classList.toggle('on', !!(leds & 1));
    $('led-bus').classList.toggle('on', !!(leds & 2));
    $('led-fault').classList.toggle('on', !!(leds & 4));
    $('ms-backlight').textContent = `${sim.backlight()} / 255${sim.uiDimmed() ? ' (dimmed)' : ''}`;
    // bus log: append what is new, keep the last 200 lines
    const total = sim.logTotal();
    if (total > this.lastLogSeq) {
      const from = Math.max(this.lastLogSeq, total - 200);
      const lines = [];
      for (let s = from; s < total; s++) { const e = sim.log(s); if (e) lines.push(formatFrame(sim, e)); }
      const pre = $('bus-log');
      const stick = pre.scrollTop + pre.clientHeight >= pre.scrollHeight - 4;
      pre.textContent += (pre.textContent ? '\n' : '') + lines.join('\n');
      const all = pre.textContent.split('\n'); if (all.length > 200) pre.textContent = all.slice(-200).join('\n');
      if (stick) pre.scrollTop = pre.scrollHeight;
      this.lastLogSeq = total;
      $('log-count').textContent = `${total} frames`;
    }
  }
}
