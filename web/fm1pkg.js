// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// FM-1 (and M-Vave SMK-37 Pro) .fwsc packages in the browser: the package identity and the image the
// device reads during an update.

const BLOCKS = 20, BLK = 0x30, KEEP = 0x2F, MAX_BLOCKS = 64;

const printable = (m, i) => { const c = (m - i - 1) & 0xFF; return c >= 0x20 && c < 0x7F; };

// the identity blocks: 20, or more when the identity is longer (the Starrykey 37 Play V016's "STARRYKEY-37 PLAY_016"
// takes 21): further identity markers count only when unused 0x7D markers follow them (the 36-block layout)
function identityBlocks(fwsc) {
  let n = BLOCKS;
  while (n < MAX_BLOCKS && (n + 1) * BLK <= fwsc.length && fwsc[n * BLK + KEEP] !== 0x7D && printable(fwsc[n * BLK + KEEP], n)) n++;
  return n > BLOCKS && (n + 1) * BLK <= fwsc.length && fwsc[n * BLK + KEEP] === 0x7D ? n : BLOCKS;
}

// interleave blocks: the identity blocks, then more while the marker is the unused 0x7D (the stock SMK-37 Pro,
// MKE-P37 and Starrykey 37 Play V016 have 36)
export function blockCount(fwsc) {
  let n = identityBlocks(fwsc);
  while (n < MAX_BLOCKS && (n + 1) * BLK <= fwsc.length && fwsc[n * BLK + KEEP] === 0x7D) n++;
  return n;
}

// the package identity ("FM-1_904", "SMK-37 Pro_016"): one marker byte after each identity block (0x7D: none);
// past the first 20 only when the package really has those blocks (logicalImage's count)
export function productOf(fwsc) {
  if (fwsc.length < BLOCKS * BLK) throw new Error("not an FM-1 package (too short)");
  const ib = identityBlocks(fwsc), n = ib === BLOCKS ? BLOCKS : Math.min(ib, layout(fwsc).blocks);
  let s = "";
  for (let i = 0; i < n; i++) {
    const m = fwsc[i * BLK + KEEP];
    if (m !== 0x7D) s += String.fromCharCode((m - i - 1) & 0xFF);
  }
  return s;
}

function deinterleave(fwsc, blocks) {
  const out = new Uint8Array(fwsc.length - blocks);
  for (let i = 0; i < blocks; i++) out.set(fwsc.subarray(i * BLK, i * BLK + KEEP), i * KEEP);
  out.set(fwsc.subarray(blocks * BLK), blocks * KEEP);
  return out;
}

function crc16(d, c = 0) {                 // CRC-16/XMODEM, as the package and the device use
  for (const b of d) {
    c ^= b << 8;
    for (let k = 0; k < 8; k++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF;
  }
  return c;
}

function jlDec(b) {                        // JieLi ENC stream cipher (its own inverse)
  const out = Uint8Array.from(b);
  let key = 0xFFFF;
  for (let i = 0; i < out.length; i++) {
    out[i] ^= key & 0xFF;
    key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
  return out;
}

// the logical image's flash.bin (type 0) and ota.bin (type 100) match their CRCs (as fm1_install.py)
function filesOk(img) {
  if (img.length < 0x40) return false;
  const h = jlDec(img.subarray(0, 0x40)), rd = (a, o, n) => a.slice(o, o + n).reduce((v, x, i) => v + x * 2 ** (8 * i), 0);
  if (crc16(h.subarray(2, 0x40)) !== rd(h, 0, 2)) return false;
  const count = rd(h, 8, 2);
  if (count < 1 || count > 8 || img.length < 0x40 + 0x50 * count) return false;
  for (let i = 0; i < count; i++) {
    const e = jlDec(img.subarray(0x40 + i * 0x50, 0x90 + i * 0x50));
    const typ = rd(e, 0, 2), off = rd(e, 8, 4), size = rd(e, 12, 4);
    if (off + size > img.length) return false;
    if ((typ === 0 || typ === 100) && crc16(img.subarray(off, off + size)) !== rd(e, 4, 2)) return false;
  }
  return true;
}

// the image the device addresses during an update: the .fwsc without the marker bytes (20 blocks, or the
// detected count when the files' CRCs agree: a 20-block package can have 0x7D after block 20 by chance)
function layout(fwsc) {
  const n = blockCount(fwsc);
  if (n !== BLOCKS) {
    const img = deinterleave(fwsc, n);
    if (filesOk(img) || !filesOk(deinterleave(fwsc, BLOCKS))) return { blocks: n, image: img };
  }
  return { blocks: BLOCKS, image: deinterleave(fwsc, BLOCKS) };
}
export function logicalImage(fwsc) { return layout(fwsc).image; }

// The official FM-1 V15 (M-VAVE Downloads -> PC Firmware -> FM-1 V15, FM-1.fwsc), for the return to the
// official firmware: only that exact file is accepted (its SHA-256 pins every byte: the loader, the app, the
// layout; the name of the file proves nothing). The digest is the one Felucca 1.0 pins.
export const STOCK_V15_SHA256 = "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a";
export const STOCK_V15_SIZE = 699956;
export const STOCK_V15_PRODUCT = "FM-1_015";
// The official SMK-37 Pro V016 (SMK-37_Pro_016.fwsc, 36 interleave blocks), for the return of an SMK-37 Pro.
export const STOCK_SMK016_SHA256 = "2d98ce72530e71d617384963423820536f094501299e2cfec0a8d3c3fb6bcfb0";
export const STOCK_SMK016_SIZE = 705252;
export const STOCK_SMK016_PRODUCT = "SMK-37 Pro_016";

export async function sha256hex(bytes) {
  const d = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(d)].map((x) => x.toString(16).padStart(2, "0")).join("");
}

// The official SMK-37 range packages known here (M-Vave Downloads -> PC Firmware): informational only (the page
// tags them "known"); M-Vave's next release is accepted by its structure (identifyStockPackage). The files
// themselves are never shipped, the user downloads them from M-Vave. product: the identity the keyboard reports
// once it runs the package (the Starrykey's 21 characters do not fit the 20 identity blocks of its V015 package:
// that one carries "STARRYKEY-37 PLAY_01", so it is accepted only as this known file).
export const STOCK_SMK = [
  { name: "SMK-37 Pro", version: "016", product: "SMK-37 Pro_016", size: 705252, sha256: STOCK_SMK016_SHA256 },
  { name: "SMK-37 Pro", version: "015", product: "SMK-37 Pro_015", size: 701140, sha256: "f7f1831cd7c9ad8b4831b6e71ea0bdbcdff9ae4c4077276b3c965511bf4d4fff" },
  { name: "SMK-37 Elite", version: "016", product: "SMK-37 Elite_016", size: 705236, sha256: "b2ee38abfef3ae922e4eecdb8a551e14345300ae6f270265feacd33b6ef99cb6" },
  { name: "SMK-37 Elite", version: "015", product: "SMK-37 Elite_015", size: 701140, sha256: "6cb061bda6fc4a285a1a1014e4fc30782278fa62bdc3add75d14b29d4d76253e" },
  { name: "MKE-P37", version: "016", product: "MKE-P37_016", size: 705252, sha256: "4306ae5b765ab9afb790edf6d02fa75e872897dbd319049e3956ddd283a51429" },
  { name: "MKE-P37", version: "015", product: "MKE-P37_015", size: 701140, sha256: "280ee7aae59a17c38bc2e854802c7d08f076e51afff13eb5315ad3748476491f" },
  { name: "Starrykey 37 Play", version: "016", product: "STARRYKEY-37 PLAY_016", size: 705252, sha256: "3cd57cdd90ee60501c549b9dde53503ef468122e2f34dc110876eefdaf9be3b3" },
  { name: "Starrykey 37 Play", version: "015", product: "STARRYKEY-37 PLAY_015", size: 701140, sha256: "3ef5d835e88324a925593e1e6959106e16abeabbabf824263803bb236b09224b" },
];

// the SMK-37 range's model names in a stock identity, and the names the page shows
const STOCK_MODELS = { "SMK-37 PRO": "SMK-37 Pro", "SMK-37 ELITE": "SMK-37 Elite", "MKE-P37": "MKE-P37", "STARRYKEY-37 PLAY": "Starrykey 37 Play" };

function badPackage(code, msg) { const e = new Error(msg); e.code = code; return e; }

// an M-Vave stock package for the SMK-37 range -> { name, version, product, model, image, known } (image: the
// logical image the loader writes, as fm1_install.py --force: the detected block count). Accepted by structure, so
// M-Vave's next release works too: the package parses, its files' CRCs agree, its identity is an SMK-37 range model
// with a stock version (_000.._099); known: the SHA-256 is one of STOCK_SMK (informational). Refused (code):
// "fm1" (an FM-1 package, another device), "ours" (one of our firmwares, _7xx/_8xx/_9xx: installed above),
// "damaged" (the files' CRCs disagree), "unknown" (anything else)
export async function identifyStockPackage(bytes) {
  const sha = await sha256hex(bytes);
  if (bytes.length === STOCK_V15_SIZE && sha === STOCK_V15_SHA256)
    throw badPackage("fm1", "this is the official FM-1 firmware: it is for the FM-1, not for an SMK-37 keyboard");
  let product, image;
  try { product = productOf(bytes); image = logicalImage(bytes); }
  catch (_) { throw badPackage("unknown", "not an M-Vave firmware package for the SMK-37 range"); }
  const k = STOCK_SMK.find((x) => x.size === bytes.length && x.sha256 === sha);
  const m = /^(.+)_(\d{3})$/.exec(product);
  if (/^FM-1(_|$)/i.test(product))
    throw badPackage("fm1", "this is an FM-1 firmware: it is for the FM-1, not for an SMK-37 keyboard");
  if (m && /^[789]\d\d$/.test(m[2]) && (STOCK_MODELS[m[1].toUpperCase()] || /^SMK-37/i.test(m[1])))
    throw badPackage("ours", "this is one of our firmwares (" + product + "): install it above, not here");
  const known = k && k.product.startsWith(product), name = m && STOCK_MODELS[m[1].toUpperCase()];
  if (!known && (!name || Number(m[2]) > 99)) throw badPackage("unknown", "not an M-Vave firmware package for the SMK-37 range");
  if (!filesOk(image)) throw badPackage("damaged", "the package is damaged (its CRCs do not match)");
  if (known) return { ...k, model: k.product.split("_")[0], image, known: true };
  return { name, version: m[2], product, model: m[1], image, known: false };
}

export async function validateStockPackage(bytes) {
  if (bytes.length === STOCK_V15_SIZE && await sha256hex(bytes) === STOCK_V15_SHA256 && productOf(bytes) === STOCK_V15_PRODUCT)
    return { product: STOCK_V15_PRODUCT, image: logicalImage(bytes) };
  try { const k = await identifyStockPackage(bytes); return { product: k.product, image: k.image }; } catch (_) { /* below */ }
  throw new Error("select the unmodified official FM-1 V15 file (FM-1.fwsc) or an official SMK-37 firmware (.fwsc) from M-Vave");
}
