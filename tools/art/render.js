// Renders the vector art (tools/art/sprites.js, scenes, icons) to PNGs with
// headless Chromium, for tools/bake_sprites.py to quantize into C arrays.
// Usage: node tools/art/render.js          rectangle build (tools/art/out)
//        node tools/art/render.js --round  round StopWatch build (tools/art/out_round)
// (needs `npm i playwright` somewhere on NODE_PATH)
const path = require('path');
const fs = require('fs');
const { chromium } = require('playwright');

const ROUND = process.argv.includes('--round');
const ROOT = path.resolve(__dirname, '..', '..');
const OUT = path.join(__dirname, ROUND ? 'out_round' : 'out');
fs.mkdirSync(OUT, { recursive: true });
// Pixel sizes: creature box, scene, icon.
const SZ = ROUND ? { spr: 200, icon: 28 } : { spr: 100, icon: 16 };

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

// The round build's scene fills the whole 466 px circle. The horizon sits
// near y 280 so the creature (drawn 200 px tall, feet near y 280) stands on
// it, and the darker ground below carries the name, needs and the button.
function sceneRound(sp, tod) {
  const t = TOD[tod];
  const [a, b, c] = t.sky;
  const S = 466;
  let stars = '';
  if (tod === 'night' || tod === 'dusk')
    for (let i = 0; i < 46; i++) {
      const x = (i * 97 + 31) % S, y = 40 + (i * 53 + 17) % 200;
      stars += `<circle cx="${x}" cy="${y}" r="${i % 3 ? 1.3 : 2}" fill="#F2E6B8" opacity="${0.3 + ((i * 7) % 5) / 10}"/>`;
    }
  const ground = `<path d="M0 290 Q116 272 233 282 T466 276 V466 H0Z" fill="${t.ground}"/>` +
    `<path d="M0 340 Q140 326 260 336 T466 330 V466 H0Z" fill="#000000" opacity=".22"/>`;
  let back = '', front = '';
  if (sp === 'mothman') {
    back = `<path d="M0 282 Q60 240 120 252 T240 240 T360 248 T466 230 V290 H0Z" fill="#141a1c" opacity=".7"/>`;
    front = `<rect x="356" y="150" width="9" height="140" fill="#2a2620"/>` +
      `<rect x="338" y="122" width="44" height="34" rx="5" fill="#3a3122"/>` +
      (t.lit ? `<circle cx="360" cy="140" r="70" fill="#F5A642" opacity=".10"/><circle cx="360" cy="140" r="38" fill="#F5A642" opacity=".14"/>` : '') +
      `<rect x="344" y="127" width="32" height="24" rx="3" fill="${t.lit ? '#F5D08A' : '#8a8070'}"/>`;
  }
  if (sp === 'bigfoot') {
    const tree = (x, top, w, col) => `<path d="M${x - w} 290 L${x} ${top} L${x + w} 290Z" fill="${col}"/>` +
      `<path d="M${x - w * 0.7} ${top + (290 - top) * 0.55} L${x} ${top - 20} L${x + w * 0.7} ${top + (290 - top) * 0.55}Z" fill="${col}"/>`;
    back = tree(150, 150, 34, '#1a2620') + tree(320, 160, 30, '#1a2620');
    front = tree(64, 92, 52, '#16201a') + tree(402, 100, 52, '#16201a') + tree(18, 170, 34, '#1c2a20') + tree(452, 176, 34, '#1c2a20');
  }
  if (sp === 'nessie') {
    back = `<path d="M0 236 L80 196 L170 226 L280 182 L360 220 L466 194 L466 272 L0 272Z" fill="#141c22"/>`;
    front = `<rect x="0" y="268" width="466" height="198" fill="#1b3a40"/>` +
      `<path d="M0 272 Q24 265 48 272 T96 272 T144 272 T192 272 T240 272 T288 272 T336 272 T384 272 T432 272 T480 272" stroke="#4FA3A5" stroke-width="2" fill="none" opacity=".7"/>` +
      `<path d="M30 340 Q60 334 90 340 M300 360 Q330 354 360 360 M150 400 Q180 394 210 400" stroke="#4FA3A5" stroke-width="2" fill="none" opacity=".35"/>`;
  }
  if (sp === 'nightcrawler') {
    back = `<rect x="40" y="190" width="70" height="96" fill="#161a22"/><rect x="360" y="204" width="60" height="82" fill="#161a22"/>` +
      `<rect x="58" y="212" width="12" height="14" fill="${tod === 'night' ? '#F5D08A' : '#2a3040'}" opacity=".7"/>`;
    front = `<rect x="0" y="282" width="466" height="184" fill="#1d211f"/><path d="M0 282 H466" stroke="#3a403a" stroke-width="2"/>` +
      `<rect x="372" y="120" width="10" height="162" fill="#2a2a2e"/><rect x="356" y="110" width="42" height="16" fill="#3a3a40"/>` +
      (tod === 'night' ? '<path d="M362 126 L330 282 H426 L392 126Z" fill="#ffffff" opacity=".05"/><circle cx="377" cy="120" r="34" fill="#ffffff" opacity=".07"/>' : '');
  }
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${S}" height="${S}" viewBox="0 0 ${S} ${S}"><defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="${a}"/><stop offset=".55" stop-color="${b}"/><stop offset=".64" stop-color="${c}"/><stop offset="1" stop-color="${c}"/></linearGradient></defs><rect width="${S}" height="${S}" fill="url(#g)"/>${stars}${back}${ground}${front}</svg>`;
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
        const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${SZ.spr}" height="${SZ.spr}" viewBox="0 0 100 100">${inner}</svg>`;
        save(`spr_${sp}_${form}_${MOOD_NAMES[m]}`, await raster(svg, SZ.spr, SZ.spr));
        n++;
      }
  for (const sp of SPECIES)
    for (const tod of Object.keys(TOD)) {
      save(`scene_${sp}_${tod}`, ROUND ? await raster(sceneRound(sp, tod), 466, 466) : await raster(scene(sp, tod), 144, 128));
      n++;
    }
  for (const [k, body] of Object.entries(ICONS)) {
    save(`icon_${k}`, await raster(`<svg xmlns="http://www.w3.org/2000/svg" width="${SZ.icon}" height="${SZ.icon}" viewBox="0 0 12 12">${body}</svg>`, SZ.icon, SZ.icon));
    n++;
  }
  console.log(`rendered ${n} images to ${OUT}`);
  await browser.close();
})();
