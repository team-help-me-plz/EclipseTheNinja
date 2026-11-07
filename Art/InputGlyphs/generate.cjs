/* Regenerate with Node.js and sharp. Output paths are relative to this file. */
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');

const root = __dirname;
const palette = { ink: '#0d121d', tile: '#242c44', edge: '#44516c', gold: '#c49666', light: '#e6be94', paper: '#f7f0e5', mist: '#a4b5cb' };
const entries = [];
const images = new Map();
const escape = value => String(value).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');

function text(value, x, y, size, fill = palette.paper, weight = 600) {
    return `<text x="${x}" y="${y}" font-family="Segoe UI,Malgun Gothic,sans-serif" font-size="${size}" font-weight="${weight}" text-anchor="middle" dominant-baseline="central" fill="${fill}">${escape(value)}</text>`;
}

function svg(body, width = 256, height = 256) {
    return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}"><defs><linearGradient id="surface" x2="0" y2="1"><stop stop-color="${palette.tile}"/><stop offset="1" stop-color="${palette.ink}"/></linearGradient></defs>${body}</svg>`;
}

function arrow(direction, x = 128, y = 125, size = 44, color = palette.paper) {
    const rotation = { Up: 0, Right: 90, Down: 180, Left: 270 }[direction];
    return `<path d="M${x} ${y + size}V${y - size}m${-size * .7} ${size * .7}L${x} ${y - size}l${size * .7} ${size * .7}" fill="none" stroke="${color}" stroke-width="10" stroke-linecap="round" stroke-linejoin="round" transform="rotate(${rotation} ${x} ${y})"/>`;
}

function windowsIcon(x, y) {
    return [0, 1, 2, 3].map(i => `<rect x="${x + (i % 2) * 42}" y="${y + Math.floor(i / 2) * 42}" width="34" height="34" rx="2" fill="${palette.paper}"/>`).join('');
}

function keyboardBody(label, width, options) {
    let body = `<rect x="15" y="24" width="${width - 30}" height="218" rx="25" fill="${palette.ink}" stroke="${palette.edge}" stroke-width="3"/>
        <rect x="15" y="14" width="${width - 30}" height="218" rx="25" fill="url(#surface)" stroke="${palette.gold}" stroke-width="3"/>
        <path d="M38 31H${width - 38}" stroke="${palette.light}" stroke-opacity=".3" stroke-width="2"/>
        <path d="M41 216H${width - 41}" stroke="${palette.edge}" stroke-width="2"/>`;
    if (options.direction) {
        body += arrow(options.direction, width / 2);
    } else if (options.windows) {
        body += windowsIcon(width / 2 - 38, 86);
    } else if (options.menu) {
        body += `<rect x="${width / 2 - 38}" y="79" width="76" height="88" rx="7" fill="none" stroke="${palette.paper}" stroke-width="6"/>`;
        for (const y of [101, 123, 145]) body += `<path d="M${width / 2 - 22} ${y}h44" stroke="${palette.paper}" stroke-width="5"/>`;
    } else {
        const lines = label.split('\n');
        const size = Math.min(lines.length > 1 ? 48 : 84, (width - 58) / Math.max(...lines.map(line => line.length)) / .64);
        lines.forEach((line, index) => { body += text(line, width / 2, 122 + (index - (lines.length - 1) / 2) * 58, size); });
    }
    if (options.shifted) body += text(options.shifted, 53, 56, 35, palette.mist);
    if (options.side) body += text(options.side, 44, 53, 25, palette.gold);
    if (options.numpad) body += text('NUM', width / 2, 185, 22, palette.mist);
    return body;
}

async function save(family, id, label, body, width, engineKey, category, rasterSource = null) {
    const relative = `${family}/${id}`;
    const source = svg(body, width);
    fs.mkdirSync(path.join(root, family), { recursive: true });
    fs.writeFileSync(path.join(root, relative + '.svg'), source);
    const renderer = rasterSource ? sharp(path.join(root, rasterSource)).resize(width, 256) : sharp(Buffer.from(source));
    const png = await renderer.png().toBuffer();
    fs.writeFileSync(path.join(root, relative + '.png'), png);
    images.set(relative, { png, width, height: 256 });
    entries.push({ id, family, label: label.replaceAll('\n', ' '), engine_key: engineKey, category, png: relative + '.png', svg: relative + '.svg', width, height: 256, source: rasterSource || undefined });
}

async function key(id, label, options = {}) {
    const width = options.width || 256;
    const engineKey = options.engineKey === undefined ? id : options.engineKey;
    await save('Keyboard', id, label, keyboardBody(label, width, options), width, engineKey, options.category || 'standard_106');
}

async function makeKeyboard() {
    await key('Escape', 'Esc');
    for (let i = 1; i <= 12; i++) await key(`F${i}`, `F${i}`);
    await key('PrintScreen', 'Prt\nSc', { engineKey: null });
    await key('ScrollLock', 'Scroll\nLock');
    await key('Pause', 'Pause');
    await key('Tilde', '`', { shifted: '~' });
    const names = ['Zero', 'One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine'];
    const shifts = [')', '!', '@', '#', '$', '%', '^', '&', '*', '('];
    for (let i = 0; i <= 9; i++) await key(names[i], String(i), { shifted: shifts[i] });
    const symbols = [
        ['Hyphen', '-', '_'], ['Equals', '=', '+'], ['LeftBracket', '[', '{'], ['RightBracket', ']', '}'],
        ['Backslash', '\\', '|'], ['Semicolon', ';', ':'], ['Apostrophe', "'", '"'],
        ['Comma', ',', '<'], ['Period', '.', '>'], ['Slash', '/', '?'],
    ];
    for (const [id, label, shifted] of symbols) await key(id, label, { shifted });
    for (const letter of 'ABCDEFGHIJKLMNOPQRSTUVWXYZ') await key(letter, letter);
    await key('BackSpace', 'Backspace', { width: 448 });
    await key('Tab', 'Tab', { width: 320 });
    await key('CapsLock', 'Caps Lock', { width: 384 });
    await key('Enter', 'Enter', { width: 448 });
    await key('SpaceBar', 'Space', { width: 896 });
    for (const side of ['Left', 'Right']) {
        await key(side + 'Shift', 'Shift', { side: side[0], width: 448 });
        await key(side + 'Control', 'Ctrl', { side: side[0], width: 320 });
        await key(side + 'Alt', 'Alt', { side: side[0] });
        await key(side + 'Command', 'Win', { side: side[0], windows: true });
    }
    await key('ContextMenu', 'Menu', { menu: true, engineKey: null });
    for (const [id, label] of [['Insert', 'Ins'], ['Home', 'Home'], ['PageUp', 'PgUp'], ['Delete', 'Del'], ['End', 'End'], ['PageDown', 'PgDn']]) await key(id, label);
    for (const direction of ['Up', 'Down', 'Left', 'Right']) await key(direction, direction, { direction });
    await key('NumLock', 'Num\nLock');
    for (let i = 0; i <= 9; i++) await key('NumPad' + names[i], String(i), { numpad: true, width: i === 0 ? 384 : 256 });
    for (const [id, label] of [['Divide', '/'], ['Multiply', '×'], ['Subtract', '−'], ['Add', '+'], ['Decimal', '.']]) await key(id, label, { numpad: true });
    await key('NumPadEnter', 'Enter', { numpad: true, engineKey: 'Enter' });
    await key('Hangul', 'Lang', { engineKey: null });
    await key('Hanja', 'Hanja', { engineKey: null });

    for (let i = 13; i <= 24; i++) await key(`F${i}`, `F${i}`, { category: 'extended', engineKey: null });
    const extras = [
        ['Fn', 'Fn'], ['AltGr', 'AltGr'], ['Shift', 'Shift'], ['Control', 'Ctrl'], ['Alt', 'Alt'],
        ['Command', 'Cmd'], ['Option', 'Option'], ['InternationalBackslash', '\\ |'],
        ['VolumeMute', 'Mute'], ['VolumeDown', 'Vol −'], ['VolumeUp', 'Vol +'],
        ['MediaPlayPause', 'Play\nPause'], ['MediaStop', 'Stop'], ['MediaPrevious', 'Prev'], ['MediaNext', 'Next'],
        ['BrowserBack', 'Back'], ['BrowserForward', 'Forward'], ['BrowserRefresh', 'Refresh'], ['BrowserStop', 'Stop'],
        ['BrowserSearch', 'Search'], ['BrowserFavorites', 'Favorite'], ['BrowserHome', 'Home'],
        ['BrightnessDown', 'Bright −'], ['BrightnessUp', 'Bright +'], ['Power', 'Power'], ['Sleep', 'Sleep'], ['Wake', 'Wake'],
        ['Unknown', '?'],
    ];
    for (const [id, label] of extras) await key(id, label, { category: 'extended', engineKey: null, width: label.length > 6 ? 320 : 256 });
}

function circle(body) {
    return `<circle cx="128" cy="134" r="109" fill="${palette.ink}" stroke="${palette.edge}" stroke-width="3"/><circle cx="128" cy="123" r="109" fill="url(#surface)" stroke="${palette.gold}" stroke-width="3"/><circle cx="128" cy="123" r="92" fill="none" stroke="${palette.edge}" stroke-width="2"/>${body}`;
}

function shapeSymbol(symbol, color) {
    const style = `fill="none" stroke="${color}" stroke-width="11" stroke-linecap="round" stroke-linejoin="round"`;
    const paths = {
        cross: `<path d="M91 86l74 74m0-74l-74 74" ${style}/>`,
        circle: `<circle cx="128" cy="123" r="46" ${style}/>`,
        square: `<rect x="84" y="79" width="88" height="88" rx="3" ${style}/>`,
        triangle: `<path d="M128 72l54 94H74Z" ${style}/>`,
    };
    return paths[symbol];
}

function dpad(direction) {
    return `<image href="../Sources/DPadGold/${direction}.png" width="256" height="256"/>`;
}

async function makeGamepad(family) {
    const ps = family === 'PlayStation';
    const faces = ps
        ? [['Bottom', 'Cross', 'cross', '#a4bfe5'], ['Right', 'Circle', 'circle', '#ec9d99'], ['Left', 'Square', 'square', '#daa7c4'], ['Top', 'Triangle', 'triangle', '#98d1c2']]
        : [['Bottom', 'A', null, '#a9cf9c'], ['Right', 'B', null, '#ec9d99'], ['Left', 'X', null, '#a4bfe5'], ['Top', 'Y', null, '#e6c580']];
    for (const [position, label, symbol, color] of faces) {
        const body = symbol ? shapeSymbol(symbol, color) : text(label, 128, 122, 115, color);
        const id = 'Gamepad_FaceButton_' + position;
        await save(family, id, label, circle(body), 256, id, 'face');
    }
    for (const side of ['Left', 'Right']) {
        const l = side[0];
        for (const [suffix, label] of [['Shoulder', ps ? l + '1' : l + 'B'], ['Trigger', ps ? l + '2' : l + 'T']]) {
            const id = `Gamepad_${side}${suffix}`;
            await save(family, id, label, keyboardBody(label, 320, {}), 320, id, 'shoulder_trigger');
        }
        const thumb = `Gamepad_${side}Thumbstick`;
        await save(family, thumb, ps ? l + '3' : l + 'S', circle(text(ps ? l + '3' : l + 'S', 128, 113, 79) + text('PRESS', 128, 178, 22, palette.mist)), 256, thumb, 'stick');
        const axis = `Gamepad_${side}2D`;
        await save(family, axis, l + 'S', circle(text(l + 'S', 128, 123, 80)), 256, axis, 'stick');
        for (const direction of ['Up', 'Down', 'Left', 'Right']) {
            const id = `Gamepad_${side}Stick_${direction}`;
            const body = circle(text(ps ? l + ' stick' : l + 'S', 128, 72, 34, palette.mist) + arrow(direction, 128, 144, 35));
            await save(family, id, l + ' stick ' + direction, body, 256, id, 'stick_direction');
        }
    }
    for (const direction of ['Up', 'Down', 'Left', 'Right']) {
        const id = 'Gamepad_DPad_' + direction;
        await save(family, id, 'D-pad ' + direction, dpad(direction), 256, id, 'dpad', `Sources/DPadGold/${direction}.png`);
    }
    for (const [side, label] of [['Left', ps ? 'Touchpad' : 'View'], ['Right', ps ? 'Options' : 'Menu']]) {
        const id = 'Gamepad_Special_' + side;
        let body;
        if (ps) {
            body = circle(text(label, 128, 123, label === 'Touchpad' ? 36 : 43));
        } else if (side === 'Left') {
            body = circle(`<rect x="79" y="80" width="72" height="64" rx="5" fill="none" stroke="${palette.paper}" stroke-width="6"/><rect x="105" y="104" width="72" height="64" rx="5" fill="${palette.ink}" stroke="${palette.paper}" stroke-width="6"/>`);
        } else {
            body = circle([92, 123, 154].map(y => `<path d="M84 ${y}h88" stroke="${palette.paper}" stroke-width="8" stroke-linecap="round"/>`).join(''));
        }
        await save(family, id, label, body, 256, id, 'system');
    }
    if (ps) {
        await save(family, 'Touchpad', 'Touchpad', keyboardBody('Touchpad', 448, {}), 448, 'Gamepad_Special_Left_Touched', 'system');
        await save(family, 'Create', 'Create', circle(text('Create', 128, 123, 48)), 256, null, 'system');
        await save(family, 'MicrophoneMute', 'Mic mute', circle(text('MIC', 128, 104, 55) + text('MUTE', 128, 158, 30, palette.mist)), 256, null, 'system');
    } else {
        await save(family, 'Share', 'Share', circle(text('Share', 128, 123, 51)), 256, null, 'system');
    }
    await save(family, 'Guide', 'Guide', circle(text('Home', 128, 123, 50)), 256, null, 'system');
}

async function board(family, title, subtitle, selected, columns) {
    const width = 1800;
    const cellWidth = 165;
    const cellHeight = 169;
    const margin = (width - columns * cellWidth) / 2;
    const height = 198 + Math.ceil(selected.length / columns) * cellHeight + 56;
    const layers = [{ input: Buffer.from(svg(`<rect width="${width}" height="${height}" fill="${palette.ink}"/>
        ${text(title, width / 2, 65, 42, palette.paper)}${text(subtitle, width / 2, 120, 18, palette.mist)}
        <path d="M72 163H1728" stroke="${palette.gold}" stroke-width="1"/>`, width, height)), left: 0, top: 0 }];
    for (let i = 0; i < selected.length; i++) {
        const entry = selected[i];
        const asset = images.get(entry.family + '/' + entry.id);
        const rendered = await sharp(asset.png).resize({ width: 150, height: 119, fit: 'inside' }).toBuffer({ resolveWithObject: true });
        const x = Math.round(margin + (i % columns) * cellWidth);
        const y = 191 + Math.floor(i / columns) * cellHeight;
        layers.push({ input: rendered.data, left: x + Math.round((cellWidth - rendered.info.width) / 2), top: y + Math.round((119 - rendered.info.height) / 2) });
        const label = svg(text(entry.id.replace('Gamepad_', '').replaceAll('_', ' '), cellWidth / 2, 18, 12, palette.mist), cellWidth, 38);
        layers.push({ input: Buffer.from(label), left: x, top: y + 120 });
    }
    fs.mkdirSync(path.join(root, 'Preview'), { recursive: true });
    await sharp({ create: { width, height, channels: 4, background: palette.ink } }).composite(layers).png().toFile(path.join(root, 'Preview', family + '.png'));
}

async function main() {
    await makeKeyboard();
    await makeGamepad('Xbox');
    await makeGamepad('PlayStation');
    const aliases = { Underscore: 'Hyphen', Ampersand: 'Seven', Asterix: 'Eight', Caret: 'Six', Colon: 'Semicolon', Dollar: 'Four', Exclamation: 'One', LeftParantheses: 'Nine', RightParantheses: 'Zero', Quote: 'Apostrophe' };
    const lookup = { Keyboard: {}, Xbox: {}, PlayStation: {} };
    for (const entry of entries) {
        if (!entry.engine_key) continue;
        if (lookup[entry.family][entry.engine_key]) continue;
        lookup[entry.family][entry.engine_key] = entry.png;
    }
    for (const [keyName, glyph] of Object.entries(aliases)) lookup.Keyboard[keyName] = 'Keyboard/' + glyph + '.png';
    for (const family of ['Xbox', 'PlayStation']) {
        for (const side of ['Left', 'Right']) {
            for (const axis of ['X', 'Y']) lookup[family][`Gamepad_${side}${axis}`] = `${family}/Gamepad_${side}2D.png`;
            lookup[family][`Gamepad_${side}TriggerAxis`] = `${family}/Gamepad_${side}Trigger.png`;
        }
    }
    const manifest = { version: 1, style: 'Eclipse', dpad_style: 'gold-tactile', padding_px: 14, color_space: 'sRGB', alpha: 'straight', fallback: 'Keyboard/Unknown.png', lookup, entries };
    fs.writeFileSync(path.join(root, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
    const keyboard = entries.filter(entry => entry.family === 'Keyboard' && entry.category === 'standard_106');
    if (keyboard.length !== 106) throw new Error(`Expected 106 standard keys, got ${keyboard.length}`);
    await board('Keyboard', 'ECLIPSE / KEYBOARD', '106-KEY FULL SIZE / ENGLISH LABELS / INDIVIDUAL TRANSPARENT PNG', keyboard, 10);
    await board('KeyboardExtended', 'ECLIPSE / EXTENDED KEYS', 'FUNCTION / MEDIA / SYSTEM / GENERIC MODIFIERS', entries.filter(entry => entry.family === 'Keyboard' && entry.category === 'extended'), 10);
    await board('Gamepad', 'ECLIPSE / GAMEPAD', 'XBOX / PLAYSTATION / SAME ENHANCED INPUT KEY, PLATFORM-SPECIFIC GLYPH', entries.filter(entry => entry.family !== 'Keyboard'), 10);
    for (const family of ['Xbox', 'PlayStation']) await board(family, 'ECLIPSE / ' + family.toUpperCase(), 'FACE / SHOULDER / TRIGGER / STICK / D-PAD / SYSTEM', entries.filter(entry => entry.family === family), 8);
    const sample = [
        ...['Escape', 'A', 'W', 'J', 'SpaceBar', 'LeftShift', 'Left', 'NumPadOne'].map(id => entries.find(entry => entry.family === 'Keyboard' && entry.id === id)),
        ...['Xbox', 'PlayStation'].flatMap(family => ['Gamepad_FaceButton_Bottom', 'Gamepad_FaceButton_Right', 'Gamepad_FaceButton_Left', 'Gamepad_FaceButton_Top', 'Gamepad_LeftShoulder', 'Gamepad_LeftTrigger', 'Gamepad_DPad_Left', 'Gamepad_LeftThumbstick'].map(id => entries.find(entry => entry.family === family && entry.id === id))),
    ];
    await board('Overview', 'ECLIPSE / INPUT GLYPHS', '146 KEYBOARD / 28 XBOX / 30 PLAYSTATION / INDIVIDUAL TRANSPARENT PNG', sample, 8);
    await board('DPadGold', 'ECLIPSE / D-PAD', 'GOLD TACTILE / UP / DOWN / LEFT / RIGHT', entries.filter(entry => entry.family === 'Xbox' && entry.category === 'dpad'), 4);
    console.log(JSON.stringify({ total: entries.length, counts: ['Keyboard', 'Xbox', 'PlayStation'].map(family => ({ family, count: entries.filter(entry => entry.family === family).length })) }));
}

main().catch(error => { console.error(error); process.exitCode = 1; });
