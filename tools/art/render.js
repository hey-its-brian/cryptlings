// Renders the vector art (tools/art/sprites.js, scenes, icons) to PNGs with
// headless Chromium, for tools/bake_sprites.py to quantize into C arrays.
// Usage: node tools/art/render.js   (needs `npm i playwright` somewhere on NODE_PATH)
const path = require('path');
const fs = require('fs');
const { chromium } = require('playwright');

const ROOT = path.resolve(__dirname, '..', '..');
const OUT = path.join(__dirname, 'out');
fs.mkdirSync(OUT, { recursive: true });

const SPECIES = ['mothman', 'bigfoot', 'nessie', 'nightcrawler'];
const FORMS = ['egg', 'whisper', 'rumor', 'sighting', 'legend', 'blurry'];
const MOODS = [['happy', false], ['neutral', false], ['sad', false], ['sleep', false], ['neutral', true]];
const MOOD_NAMES = ['happy', 'neutral', 'sad', 'sleep', 'blink'];

const TOD = {
  dusk:  { sky: ['#2a2238', '#43304a', '#5a4050'], ground: '#1c2420', lit: true },
  night: { sky: ['#070a14', '#0d1424', '#18213a'], ground: '#121a16', lit: true },
  dawn:  { sky: ['#2b3550', '#6a5a6e', '#b08070'], ground: '#1f2a22', lit: false },
  day:   { sky: ['#3e5068', '#6f8399', '#9aa8b4'], ground: '#2b3a2c', lit: false },
};

function scene(sp, tod) {
  const t = TOD[tod];
  const [a, b, c] = t.sky;
  let stars = '';
  if (tod === 'night' || tod === 'dusk')
    for (let i = 0; i < 14; i++) {
      const x = (i * 37 + 11) % 160, y = (i * 23 + 7) % 60;
      stars += `<circle cx="${x}" cy="${y}" r="${i % 3 ? 0.6 : 0.9}" fill="#F2E6B8" opacity="${0.35 + ((i * 7) % 5) / 10}"/>`;
    }
  let prop = '';
  if (sp === 'mothman') prop = `<rect x="124" y="56" width="3" height="66" fill="#2a2620"/><rect x="118" y="48" width="15" height="12" rx="2" fill="#3a3122"/>${t.lit ? '<circle cx="125.5" cy="54" r="18" fill="#F5A642" opacity=".12"/>' : ''}<rect x="120" y="50" width="11" height="8" rx="1.5" fill="${t.lit ? '#F5D08A' : '#8a8070'}"/>`;
  if (sp === 'bigfoot') prop = `<g fill="#16201a"><path d="M2 122 L16 60 L30 122Z"/><path d="M6 92 L16 50 L26 92Z" fill="#1c2a20"/><path d="M116 122 L132 54 L148 122Z"/><path d="M121 90 L132 44 L143 90Z" fill="#1c2a20"/></g>`;
  if (sp === 'nessie') prop = `<rect x="0" y="104" width="144" height="24" fill="#1b3a40"/><path d="M0 106 Q10 103 20 106 T40 106 T60 106 T80 106 T100 106 T120 106 T140 106 T160 106" stroke="#4FA3A5" stroke-width="1" fill="none" opacity=".7"/><path d="M0 92 L30 80 L60 90 L95 76 L120 88 L144 80 L144 104 L0 104Z" fill="#141c22"/>`;
  if (sp === 'nightcrawler') prop = `<rect x="0" y="100" width="144" height="28" fill="#1d211f"/><path d="M0 100 H144" stroke="#3a403a" stroke-width="1"/><rect x="126" y="40" width="4" height="60" fill="#2a2a2e"/><rect x="120" y="36" width="16" height="7" fill="#3a3a40"/>${tod === 'night' ? '<circle cx="128" cy="39" r="14" fill="#ffffff" opacity=".06"/>' : ''}`;
  return `<svg xmlns="http://www.w3.org/2000/svg" width="144" height="128" viewBox="0 0 144 128"><defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="${a}"/><stop offset=".65" stop-color="${b}"/><stop offset="1" stop-color="${c}"/></linearGradient></defs><rect width="144" height="128" fill="url(#g)"/>${stars}<path d="M0 106 Q36 98 72 104 T144 100 V128 H0Z" fill="${t.ground}"/>${prop}</svg>`;
}

const ICONS = {
  belly: '<path d="M6 10.9 C3.8 10.9 2.4 9.2 2.4 7 C2.4 5 3.8 3.6 6 3.6 C8.2 3.6 9.6 5 9.6 7 C9.6 9.2 8.2 10.9 6 10.9Z" fill="#B9773A" stroke="#1a1622" stroke-width="1"/><path d="M2.6 5.2 Q6 2 9.4 5.2" fill="#6B4A2A" stroke="#1a1622" stroke-width="1"/><path d="M6 2.6 V1" stroke="#1a1622" stroke-width="1"/>',
  cheer: '<path d="M6 10.5 C1 7 1 3 3.6 2.6 C5 2.4 5.8 3.4 6 4.2 C6.2 3.4 7 2.4 8.4 2.6 C11 3 11 7 6 10.5Z" fill="#E58FA0" stroke="#1a1622" stroke-width="1"/>',
  rest: '<path d="M8.5 1.5 A4.6 4.6 0 1 0 10.5 8 A3.6 3.6 0 0 1 8.5 1.5Z" fill="#C9C3E0" stroke="#1a1622" stroke-width="1"/>',
  myst: '<path d="M1 6 Q6 1 11 6 Q6 11 1 6Z" fill="#2C3547" stroke="#8FA3B8" stroke-width="1"/><circle cx="6" cy="6" r="1.8" fill="#4FA3A5"/>',
  tidy: '<circle cx="5" cy="6.5" r="3.4" fill="none" stroke="#7FB069" stroke-width="1.2"/><circle cx="9" cy="3.2" r="1.6" fill="none" stroke="#7FB069" stroke-width="1"/><path d="M3.4 5.2 Q4 4.4 5 4.4" stroke="#E7E1D2" stroke-width=".8" fill="none"/>',
  play: '<circle cx="6" cy="6" r="4" fill="#8E7CC3" stroke="#1a1622"/><path d="M2.4 5 Q6 7.5 9.6 5" stroke="#E7E1D2" fill="none" stroke-width=".9"/>',
  lights: '<rect x="4" y="1.5" width="4" height="1.5" fill="#5b4a2c"/><path d="M3.5 3 H8.5 L9 9 H3 Z" fill="#F5A642" stroke="#1a1622" stroke-width="1"/><rect x="3" y="9" width="6" height="1.6" fill="#5b4a2c"/>',
  case: '<rect x="1.5" y="3" width="9" height="7" rx="1" fill="#D9C79A" stroke="#2A2320"/><path d="M4 3 V2 H8 V3" fill="none" stroke="#2A2320"/>',
  loch: '<path d="M6 1.5 C6 1.5 2.5 6 2.5 8 A3.5 3.5 0 0 0 9.5 8 C9.5 6 6 1.5 6 1.5Z" fill="#4FA3A5" stroke="#1a1622" stroke-width="1"/>',
  walk: '<ellipse cx="4" cy="8" rx="1.6" ry="2.4" fill="#EDEDF2" stroke="#1a1622" stroke-width=".8"/><ellipse cx="8" cy="4.5" rx="1.6" ry="2.4" fill="#EDEDF2" stroke="#1a1622" stroke-width=".8"/>',
  pants: '<path d="M3 1.5 H9 L10 10.5 H7.2 L6 5 L4.8 10.5 H2 Z" fill="#C9C3E0" stroke="#1a1622" stroke-width="1"/>',
  lamp: '<circle cx="6" cy="5" r="3.2" fill="#F5D08A" stroke="#1a1622" stroke-width="1"/><rect x="4.6" y="8" width="2.8" height="2.4" fill="#5b4a2c"/>',
  forage: '<circle cx="4.5" cy="7" r="2.2" fill="#6B5BA8" stroke="#1a1622" stroke-width=".8"/><circle cx="7.8" cy="6" r="2" fill="#6B5BA8" stroke="#1a1622" stroke-width=".8"/><path d="M6 4 Q7 1.5 9 2" stroke="#7FB069" fill="none" stroke-width="1"/>',
  treat: '<rect x="2" y="4" width="8" height="5" rx="1" fill="#E58FA0" stroke="#1a1622" stroke-width="1"/><path d="M2 6.5 H10" stroke="#F2E6B8" stroke-width="1"/>',
};

(async () => {
  const exe = fs.existsSync('/opt/pw-browsers/chromium') ? '/opt/pw-browsers/chromium' : undefined;
  const browser = await chromium.launch(exe ? { executablePath: exe } : {});
  const page = await browser.newPage();
  await page.setContent('<html><body style="margin:0;background:transparent"></body></html>');
  await page.addScriptTag({ content: fs.readFileSync(path.join(__dirname, 'sprites.js'), 'utf8') });

  // Draw an SVG string to a canvas of w x h and return a PNG data URL.
  const raster = (svg, w, h) => page.evaluate(async ({ svg, w, h }) => {
    const img = new Image();
    img.src = 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(svg);
    await img.decode();
    const c = document.createElement('canvas');
    c.width = w; c.height = h;
    c.getContext('2d').drawImage(img, 0, 0, w, h);
    return c.toDataURL('image/png');
  }, { svg, w, h });
  const save = (name, url) => fs.writeFileSync(path.join(OUT, name + '.png'), Buffer.from(url.split(',')[1], 'base64'));

  let n = 0;
  for (const sp of SPECIES)
    for (const form of FORMS)
      for (let m = 0; m < MOODS.length; m++) {
        const inner = await page.evaluate(({ sp, form, mood, blink }) =>
          window.CRYPTID_SPRITES[sp][form]({ mood, blink, t: 0 }), { sp, form, mood: MOODS[m][0], blink: MOODS[m][1] });
        const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100" viewBox="0 0 100 100">${inner}</svg>`;
        save(`spr_${sp}_${form}_${MOOD_NAMES[m]}`, await raster(svg, 100, 100));
        n++;
      }
  for (const sp of SPECIES)
    for (const tod of Object.keys(TOD)) { save(`scene_${sp}_${tod}`, await raster(scene(sp, tod), 144, 128)); n++; }
  for (const [k, body] of Object.entries(ICONS)) {
    save(`icon_${k}`, await raster(`<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 12 12">${body}</svg>`, 16, 16));
    n++;
  }
  console.log(`rendered ${n} images to ${OUT}`);
  await browser.close();
})();
