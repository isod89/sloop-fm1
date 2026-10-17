#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Install a Felucca package (.fwsc) on an FM-1 (or an M-Vave SMK-37 Pro) over USB-MIDI.

The same update as the web installer (web/fm1ota.js): step 1, the
running firmware reads parts of the package and starts the update loader;
step 2, the loader reads the whole package and writes it. In both steps the
device asks (SysEx read requests on the logical image) and we answer. Then
the FM-1 restarts and the installed identity is checked.

  fm1_install.py PACKAGE.fwsc [--port NAME] [--yes] [--force] [--fm1]
  fm1_install.py --info [--port NAME]

If the FM-1 is still in update mode (an earlier install was cut off), the
install finishes the write. Needs mido with python-rtmidi.

Exit codes: 0 done, 1 cancelled or other error, 2 bad arguments or package,
3 FM-1 not found, 4 connection lost or the device stopped, 5 timeout (no
loader / no restart), 6 wrong model, another identity after the install, or the
FM-1 is in the update mode of another firmware (only SLOOP's own loader,
ota-FM-1_9XX or ota-SMK-37 Pro_9XX, is resumed).

Packages: 20 interleave blocks (FM-1, SLOOP, SMK-37 Pro up to v011) or more (the stock SMK-37 Pro
v016 has 36: the extra markers are 0x7D); the count is detected and checked with the files' CRCs.

A stock package (without SLOOP's loader) is checked by its contents, not by the identity it or the
device reports: one laid out for the FM-1 (flash.bin's length, its region table) is never written to an
SMK-37, and to a device that reports an FM-1 only when the user confirms it is one (--fm1, or the
question): an SMK-37 running a third-party FM-1 firmware reports an FM-1 too.
"""
import argparse
import queue
import re
import sys
import time

HS_QUERY = bytes([0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7])
UPGRADE = bytes([0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7])
FINISH_CHECK, FINISH_WRITE = 0xE0000000, 0xF0000000
MAXDATA = 512
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F         # BLOCKS: the identity blocks (more may follow, see block_count)
MAX_BLOCKS = 64
LOADER_MARK = b"FELUCCA-LOADER-1"
PORT_RE = re.compile(r"fm-1|smk|elite|mke-p37|starry|felucca|sloop|clipper|melodee|fomni|x0x|makkuro|maku|rainbow|snoop|vector|vox|ota|composite|sinco|usb-midi", re.I)   # never probe other gear
# (the FM-1 and the SMK-37 family under SLOOP / Felucca, under CLIPPER, the other SMK-37 ports, the update loaders,
# the stock firmware: as web/fm1ota.js PORT_RE)
OWN_LOADER_RE = re.compile(r"ota-(FM-1|SMK-37 Pro|SMK-37 Elite|MKE-P37)_9\d\d", re.I)   # SLOOP's own update loader

# seconds; the tests shorten them
DELAY = {"open": 0.3, "start": 2.0, "reply": 0.01, "loader": 3.0, "reboot": 3.0, "retry": 1.0,
         "poll": 1.0, "hs": 1.0, "idle_check": 8.0, "idle_write": 180.0,
         "wait_loader": 30.0, "wait_reboot": 40.0}

EXIT = {"usage": 2, "badpkg": 2, "notfound": 3, "model": 6, "lost": 4, "stopped": 4, "badreq": 4,
        "noloader": 5, "noreturn": 5, "mismatch": 6, "foreign": 6,
        "fm1pkg": 6, "board": 6, "fm1confirm": 6}


class InstallError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


# ------------------------------------------------------------ wire format ---

def pack7(data):
    out, acc, nb = [], 0, 0
    for b in data:
        acc |= b << nb
        nb += 8
        while nb >= 7:
            out.append(acc & 0x7F)
            acc >>= 7
            nb -= 7
    if nb:
        out.append(acc & 0x7F)
    return bytes(out)


def unpack7(s):
    out, acc, nb = [], 0, 0
    for b in s:
        acc |= (b & 0x7F) << nb
        nb += 7
        while nb >= 8:
            out.append(acc & 0xFF)
            acc >>= 8
            nb -= 8
    return bytes(out)


def response(addr, data, fl=0):
    n = len(data)
    body = bytearray([0x00, 0x59, 0x30, (n + 8) & 0xFF, ((n + 8) >> 8) & 0xFF, 0, fl])
    body += addr.to_bytes(4, "little") + bytes([n & 0xFF, (n >> 8) & 0xFF, 0]) + bytes(data)
    body.append(~sum(body[6:]) & 0xFF)
    return b"\xF0" + pack7(body) + b"\xF7"


def parse_request(pkt):
    """-> (flags, addr, len) or None"""
    if len(pkt) < 2 or pkt[0] != 0xF0 or pkt[-1] != 0xF7:
        return None
    u = unpack7(pkt[1:-1])
    if len(u) != 15 or u[:3] != b"\x00\x59\x30" or ~sum(u[6:14]) & 0xFF != u[14]:
        return None
    return u[6], int.from_bytes(u[7:11], "little"), int.from_bytes(u[11:14], "little")


class Identity:
    def __init__(self, text, model, version):
        self.text, self.model, self.version = text, model, version

    @property
    def loader(self):
        return self.model.lower().startswith("ota-")


def parse_identity(pkt):
    if len(pkt) < 2 or pkt[0] != 0xF0 or pkt[-1] != 0xF7:
        return None
    d = unpack7(pkt[1:-1])
    if len(d) != 34 or d[:3] != b"\x00\x59\x11":
        return None
    txt = d[6:33].split(b"\0")[0].decode("latin-1")
    m = re.fullmatch(r"([^_]+)_(\d+)", txt)
    return Identity(txt, m[1], int(m[2])) if m else None


# --------------------------------------------------------------- package ---

def _id_char(m, i):
    c = (m - i - 1) & 0xFF
    return 0x20 <= c < 0x7F


def identity_blocks(raw):
    """the identity blocks: 20, or more when the identity is longer (the Starrykey 37 Play V016's
    "STARRYKEY-37 PLAY_016" takes 21): further identity markers count only when unused 0x7D markers follow
    them (the 36-block layout)"""
    def marker(k):
        return raw[k * BLK + KEEP] if (k + 1) * BLK <= len(raw) else None
    n = BLOCKS
    while n < MAX_BLOCKS and (m := marker(n)) is not None and m != 0x7D and _id_char(m, n):
        n += 1
    return n if n > BLOCKS and marker(n) == 0x7D else BLOCKS


def product_of(raw):
    """the package identity ("FM-1_906", "SMK-37 Pro_016"): one marker byte after each identity block (0x7D: none);
    past the first 20 only when the package really has those blocks (the layout's count)"""
    if len(raw) < BLOCKS * BLK:
        raise InstallError("badpkg", "not an FM-1 package (too short)")
    ib = identity_blocks(raw)
    n = BLOCKS if ib == BLOCKS else min(ib, deinterleave(raw)[1])
    return "".join(chr((m - i - 1) & 0xFF) for i in range(n) if (m := raw[i * BLK + KEEP]) != 0x7D)


def block_count(raw):
    """interleave blocks: the identity blocks, then more while the marker is the unused 0x7D (SMK-37 Pro v016: 36).
    A 20-block package can have 0x7D there by chance: load_package checks the CRCs and falls back."""
    n = identity_blocks(raw)
    while n < MAX_BLOCKS and (n + 1) * BLK <= len(raw) and raw[n * BLK + KEEP] == 0x7D:
        n += 1
    return n


def logical_image(raw, blocks=BLOCKS):
    """the image the device reads during an update: the package without the marker bytes"""
    out = bytearray()
    for i in range(blocks):
        out += raw[i * BLK:i * BLK + KEEP]
    return bytes(out + raw[blocks * BLK:])


def deinterleave(raw):
    """-> (logical image, block count): the detected count when the files' CRCs agree, else 20"""
    n = block_count(raw)
    image = logical_image(raw, n)
    if n != BLOCKS and package_damage(image):
        alt = logical_image(raw, BLOCKS)
        if not package_damage(alt):
            return alt, BLOCKS
    return image, n


def model_of(text):
    return text.split("_")[0].removeprefix("ota-")


# the SMK-37 Pro, SMK-37 Elite, MKE-P37, Starrykey 37 Play and the SMK-37+VA mod: one keyboard under several names
SMK_FAMILY = re.compile(r"SMK-37[ +].*|MKE-P37|STARRYKEY-37.*", re.I)


def same_keyboard(a, b):
    """the models of identities a and b are the same hardware (never an FM-1 and an SMK)"""
    ma, mb = model_of(a), model_of(b)
    return ma == mb or bool(SMK_FAMILY.fullmatch(ma) and SMK_FAMILY.fullmatch(mb))


# ------------------------------------------------------------------ MIDI ---

class MidoLink:
    """one MIDI in/out pair with a SysEx queue"""

    def __init__(self, backend, in_name, out_name):
        import mido
        self.backend, self.name, self.mido = backend, in_name, mido
        self.q = queue.Queue()
        self.inp = mido.open_input(in_name, callback=self._rx)
        try:
            self.out = mido.open_output(out_name)
        except Exception:
            self.inp.close()
            raise
        time.sleep(DELAY["open"])   # CoreMIDI may drop a message sent just after opening

    def _rx(self, msg):
        if msg.type == "sysex":
            self.q.put(bytes([0xF0, *msg.data, 0xF7]))

    @property
    def lost(self):
        return self.name not in self.backend.input_names()

    def send(self, pkt):
        try:
            self.out.send(self.mido.Message("sysex", data=pkt[1:-1]))
            return True
        except Exception:
            return False

    def read(self, timeout):
        try:
            return self.q.get(timeout=max(timeout, 0))
        except queue.Empty:
            return None

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def close(self):
        for p in (self.inp, self.out):
            try:
                p.close()
            except Exception:
                pass


class MidoBackend:
    def __init__(self):
        import mido
        self.mido = mido

    def input_names(self):
        return list(dict.fromkeys(self.mido.get_input_names()))

    def output_names(self):
        return list(dict.fromkeys(self.mido.get_output_names()))

    def open(self, in_name, out_name):
        return MidoLink(self, in_name, out_name)


def pair_output(name, outs):
    if name in outs:
        return name
    base = re.sub(r"\s+\d+$", "", name)     # Windows numbers each port
    same = [o for o in outs if re.sub(r"\s+\d+$", "", o) == base]
    if len(same) == 1:
        return same[0]
    return outs[0] if len(outs) == 1 else None


# --------------------------------------------------------------- updater ---

class Device:
    def __init__(self, link, ident, name):
        self.link, self.id, self.name = link, ident, name


def handshake(link, tries=3):
    link.drain()
    for _ in range(tries):
        if not link.send(HS_QUERY):
            return None
        end = time.monotonic() + DELAY["hs"]
        while (left := end - time.monotonic()) > 0:
            p = link.read(left)
            if p is None:
                break
            ident = parse_identity(p)
            if ident:
                return ident
    return None


class Updater:
    def __init__(self, backend, port=None):
        self.backend = backend
        self.port = port

    def candidates(self, only_port):
        names = self.backend.input_names()
        if self.port:
            mine = [n for n in names if self.port.lower() in n.lower()]
            if only_port:
                return mine
            return mine + [n for n in names if n not in mine and PORT_RE.search(n)]
        return [n for n in names if PORT_RE.search(n)]

    def find(self, want=None, only_port=True):
        """the first matching port that answers the handshake (and want(identity))"""
        outs = self.backend.output_names()
        for name in self.candidates(only_port):
            out = pair_output(name, outs)
            if not out:
                continue
            try:
                link = self.backend.open(name, out)
            except Exception:
                continue
            ident = handshake(link, 2)
            if ident and (want is None or want(ident)):
                return Device(link, ident, name)
            link.close()
        return None

    def wait_for(self, want, secs):
        end = time.monotonic() + secs
        while time.monotonic() < end:
            dev = self.find(want, only_port=False)
            if dev:
                return dev
            time.sleep(DELAY["retry"])
        return None

    def serve(self, link, image, finish, idle, progress):
        """answer read requests until the device asks for `finish`; -> (served, finished, lost)"""
        served, last = 0, time.monotonic()
        while True:
            pkt = link.read(DELAY["poll"])
            if pkt is None:
                if link.lost:
                    return served, False, True
                if time.monotonic() - last > idle:
                    return served, False, False
                continue
            r = parse_request(pkt)
            if not r:
                continue
            fl, addr, n = r
            last = time.monotonic()
            if addr in (FINISH_CHECK, FINISH_WRITE):
                link.send(response(addr, b"success\0"))
                if addr == finish:
                    return served, True, False
                continue
            if n > MAXDATA or addr + n > len(image):
                raise InstallError("badreq", f"the device asked for {addr:#x}+{n}, outside the package")
            time.sleep(DELAY["reply"])
            if not link.send(response(addr, image[addr:addr + n], fl)):
                return served, False, True
            served += 1
            progress(served, addr + n)

    def check(self, dev, image, step):
        """step 1: the running firmware checks the package and starts the loader"""
        step("start", dev.id.text)
        dev.link.send(UPGRADE)
        time.sleep(DELAY["start"])
        served, finished, lost = self.serve(dev.link, image, FINISH_CHECK, DELAY["idle_check"],
                                            lambda k, end: step("check", k))
        dev.link.close()
        if not finished:
            raise InstallError("lost" if lost else "stopped",
                               f"the FM-1 {'was disconnected' if lost else 'stopped answering'} after "
                               f"{served} requests; nothing was written")
        step("loader")
        time.sleep(DELAY["loader"])
        ota = self.wait_for(lambda i: i.loader, DELAY["wait_loader"])
        if not ota:
            raise InstallError("noloader", "the update loader did not appear. Replug the USB cable and "
                                           "run the install again: the FM-1 stays in update mode until it is done")
        return ota

    def write(self, ota, image, step):
        """step 2: the loader reads and writes the whole package, then restarts"""
        step("write", 0)
        ota.link.send(UPGRADE)
        time.sleep(DELAY["start"])
        served, finished, lost = self.serve(ota.link, image, FINISH_WRITE, DELAY["idle_write"],
                                            lambda k, end: step("write", min(99, end * 100 // len(image))))
        ota.link.close()
        if not finished:
            raise InstallError("lost" if lost else "stopped",
                               f"the loader {'was disconnected' if lost else 'stopped answering'} after "
                               f"{served} requests. Replug the USB cable and run the install again to finish")
        step("write", 100)

    def verify(self, product, step):
        step("reboot")
        time.sleep(DELAY["reboot"])
        back = self.wait_for(lambda i: not i.loader, DELAY["wait_reboot"])
        if not back:
            raise InstallError("noreturn", "the FM-1 did not come back: power-cycle it")
        back.link.close()
        if back.id.text != product:
            raise InstallError("mismatch", f"written, but the FM-1 reports {back.id.text} (expected {product})")
        step("done", back.id.text)
        return back.id.text

    def install(self, dev, image, product, step=lambda *a: None, fm1_ok=False):
        """running firmware -> loader -> new firmware; dev from find(). A device already in
        update mode only needs the write."""
        refusal = board_refusal(model_of(dev.id.text), image, fm1_ok)   # (the package's board, before anything)
        if refusal:
            dev.link.close()
            raise refusal
        if not same_keyboard(dev.id.text, product):
            dev.link.close()
            raise InstallError("model", f"the device is {dev.id.text}, the package is for {product}")
        if dev.id.loader and not OWN_LOADER_RE.fullmatch(dev.id.text):
            dev.link.close()           # another firmware's loader: its image layout may differ
            raise InstallError("foreign", f"the FM-1 is in the update mode of another firmware ({dev.id.text}): "
                                          "finish that update with its own updater (M-UPGRADE), then install again")
        ota = dev if dev.id.loader else self.check(dev, image, step)
        self.write(ota, image, step)
        return self.verify(product, step)


# ------------------------------------------------------------------- CLI ---

def load_package(path, force):
    try:
        raw = open(path, "rb").read()
    except OSError as e:
        raise InstallError("badpkg", f"cannot read {path}: {e.strerror}")
    product = product_of(raw)
    if not re.fullmatch(r"[^_]+_\d+", product):
        raise InstallError("badpkg", f"{path}: not an FM-1 package (identity {product!r})")
    if LOADER_MARK not in raw and not force:
        raise InstallError("badpkg", f"{path}: no Felucca update loader in this package; "
                                     "only Felucca's own packages are installed (--force overrides)")
    image, _ = deinterleave(raw)
    bad = package_damage(image)
    if bad:
        raise InstallError("badpkg", f"{path}: damaged package ({bad}); nothing was written. "
                                     "Download it again.")
    return product, image


def _crc16(data, crc=0):
    """CRC-16/XMODEM, as the package and the device use"""
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def _dec(b):
    """JieLi ENC stream cipher (its own inverse)"""
    out, key = bytearray(b), 0xFFFF
    for i in range(len(out)):
        out[i] ^= key & 0xFF
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xFFFF
    return bytes(out)


def package_damage(image):
    """what is wrong with the update image (header, file list, each file's CRC), or None; the device
    only checks the loader before it rewrites the firmware, so a damaged file is caught here"""
    if len(image) < 0x40:
        return "too short"
    hdr = _dec(image[0:0x40])
    if _crc16(hdr[2:0x40]) != int.from_bytes(hdr[0:2], "little"):
        return "header CRC"
    count = int.from_bytes(hdr[8:10], "little")
    if not 1 <= count <= 8 or len(image) < 0x40 + 0x50 * count:
        return "file list"
    lst = image[0x40:0x40 + 0x50 * count]
    if _crc16(lst) != int.from_bytes(hdr[2:4], "little"):
        return "file list CRC"
    for i in range(count):
        e = _dec(lst[i * 0x50:(i + 1) * 0x50])
        dcrc = int.from_bytes(e[4:6], "little")
        off = int.from_bytes(e[8:12], "little")
        size = int.from_bytes(e[12:16], "little")
        name = e[0x40:0x50].split(b"\0")[0].decode("ascii", "replace")
        if off + size > len(image):
            return f"{name}: cut short"
        typ = int.from_bytes(e[0:2], "little")
        # flash.bin (0) and ota.bin (100), what the device installs, carry the CRC of their bytes as
        # served. The stock packer's other entries (USR 50, isd_config.ini 52, script.ver, blimit.bin)
        # store a CRC of something else (decrypted contents): checked for their bounds only.
        if typ in (0, 100) and _crc16(image[off:off + size]) != dcrc:
            return f"{name}: CRC"
    return None


# ---------------------------------------------------- the package's board ---
# Read from the package itself, never from an identity: the FM-1's and the SMK-37's official packages share the
# head (0x0-0x3FFF) and the update loader, and a third-party FM-1 firmware running on an SMK-37 reports an FM-1
# identity. Installed on an SMK-37, the FM-1's stock package erases the factory calibration (BTIF / USRTRIM
# 0xC0000-0xC1FFF) and writes the FM-1's USR image over the SMK's (0xF4000). The board: flash.bin's length (SMK-37
# 0x9D000, FM-1 0x93000), its region table (app offset 0x60 = flash 0x4060-0x411F, SFC-enciphered with the chip
# key 0x980F: BTIF and USR at 0xC0000 / 0xF4000 on the SMK-37, 0xE9000 / 0xEA000 on the FM-1) and where a USR
# image (UFW entry 50) goes.
FM1_STOCK_MSG = "This is the FM-1 firmware. Your keyboard is an SMK-37? Don't install it."
FM1_CONFIRM_MSG = (FM1_STOCK_MSG + " On an SMK-37 it erases the keyboard's calibration. Install it only on an FM-1. "
                   "SMK-37 owners whose keyboard runs an FM-1 firmware (it then reports an FM-1): use the SMK-37 "
                   "package from M-Vave, never this one.")
LAYOUT = {"smk": (0x9D000, 0xC0000, (0xF4000, 0xA000)), "fm1": (0x93000, 0xE9000, (0xEA000, 0x12000))}
SMK_MODEL_RE = re.compile(r"(ota-)?(SMK-37([ +].*)?|MKE-P37|STARRYKEY-37.*)", re.I)   # the SMK-37 keyboard's names


def _sfc(data, base, key=0x980F):
    """the JieLi SFC cipher of the app area (32-byte blocks, its own inverse)"""
    out = bytearray(data)
    for i in range(0, len(out), 32):
        k = (key ^ ((base + i) >> 2)) & 0xFFFF
        for j in range(i, min(i + 32, len(out))):
            out[j] ^= k & 0xFF
            k = ((k << 1) ^ (0x1021 if k & 0x8000 else 0)) & 0xFFFF
    return bytes(out)


def package_files(image):
    """[(type, offset, size, USR target, USR size)] from the UFW file list"""
    if len(image) < 0x40:
        return []
    rd = lambda b, o, n: int.from_bytes(b[o:o + n], "little")  # noqa: E731
    count = rd(_dec(image[0:0x40]), 8, 2)
    out = []
    for i in range(min(count, 11)):
        if 0x90 + i * 0x50 > len(image):
            break
        e = _dec(image[0x40 + i * 0x50:0x90 + i * 0x50])
        out.append((rd(e, 0, 2), rd(e, 8, 4), rd(e, 12, 4), rd(e, 0x1C, 4), rd(e, 0x20, 4)))
    return out


def region_table(flash):
    """flash.bin's region table: {name: (address, size)}"""
    if len(flash) < 0x4120:
        return {}
    t, out = _sfc(flash[0x4060:0x4120], 0x60), {}
    for r in range(0, 0xC0, 32):
        name = bytearray()
        for c in t[r + 16:r + 32]:
            if not 0x20 < c < 0x7F:
                break
            name.append(c)
        if name:
            out[name.decode()] = (int.from_bytes(t[r + 4:r + 8], "little"), int.from_bytes(t[r + 8:r + 12], "little"))
    return out


def package_board(image):
    """"smk", "fm1" (any sign of the FM-1's layout) or None (not a package, or another layout)"""
    files = package_files(image)
    fl = next((f for f in files if f[0] == 0), None)
    if not fl or fl[1] + fl[2] > len(image):
        return None
    rt = region_table(image[fl[1]:fl[1] + fl[2]])
    usr = [(f[3], f[4]) for f in files if f[0] == 50]
    s_len, s_btif, s_usr = LAYOUT["smk"]
    f_len, f_btif, f_usr = LAYOUT["fm1"]
    if fl[2] == s_len and rt.get("BTIF", (0,))[0] == s_btif and rt.get("USR") == s_usr and all(u == s_usr for u in usr):
        return "smk"
    if fl[2] == f_len or rt.get("BTIF", (0,))[0] == f_btif or rt.get("USR", (0,))[0] == f_usr[0] or \
            any(u[0] == f_usr[0] for u in usr):
        return "fm1"
    return None


def is_stock(image):
    """a stock package (M-Vave's, or any without our update loader): flash.bin, and a loader that is not ours"""
    return any(f[0] == 0 for f in package_files(image)) and LOADER_MARK not in image


def board_refusal(model, image, fm1_ok=False):
    """None, or why image must not be written to a device reporting model (nothing is sent): "fm1pkg" an FM-1
    layout for an SMK-37; "board" a stock package not laid out for the SMK-37 on one; "fm1confirm" such a stock
    package on a device reporting another model (an FM-1, or an SMK-37 under an FM-1 firmware), unless the user
    confirmed the device is an FM-1 (fm1_ok)"""
    board, smk = package_board(image), bool(SMK_MODEL_RE.fullmatch(model))
    if smk and board == "fm1":
        return InstallError("fm1pkg", f"{FM1_STOCK_MSG} (The device reports {model}.) Nothing was sent.")
    if not is_stock(image) or board == "smk":
        return None
    if smk:
        return InstallError("board", "this package is not laid out for the SMK-37 (flash.bin, region table): "
                                     "nothing was sent")
    return None if fm1_ok else InstallError("fm1confirm", FM1_CONFIRM_MSG + " If the device is an FM-1: --fm1.")


def not_found(up):
    names = up.backend.input_names()
    where = f"no MIDI port matching {up.port!r} answered" if up.port else "FM-1 not found"
    return InstallError("notfound", f"{where} (USB data cable? another app using it?)"
                        f"\n  MIDI inputs: {', '.join(names) if names else '(none)'}")


class Progress:
    def __init__(self, out):
        self.out, self.line = out, False

    def __call__(self, k, a=None):
        if k == "check":
            self.put(f"checking the package ({a} reads)")
        elif k == "write":
            self.put(f"writing {a:3d}%")
        else:
            self.end()
            msg = {"start": f"starting the update on {a}", "loader": "switching to update mode",
                   "reboot": "restarting", "done": f"done: the FM-1 runs {a}"}[k]
            print(msg, file=self.out, flush=True)

    def put(self, s):
        print(f"\r{s}\033[K", end="", file=self.out, flush=True)
        self.line = True

    def end(self):
        if self.line:
            print(file=self.out, flush=True)
            self.line = False


def run(a, backend, out, ask):
    up = Updater(backend, a.port)
    if a.info:
        dev = up.find()
        if not dev:
            raise not_found(up)
        dev.link.close()
        mode = "update loader (update not finished)" if dev.id.loader else "running"
        print(f"{dev.id.text}  [{mode}]  port: {dev.name}", file=out)
        return 0
    product, image = load_package(a.package, a.force)
    print(f"package: {product}  ({a.package})", file=out)
    dev = up.find()
    if not dev:
        raise not_found(up)
    print(f"device:  {dev.id.text}  ({dev.name})" + ("  in update mode: the write will be finished" if dev.id.loader else ""),
          file=out)
    if not a.yes and not ask("Install? Do not unplug the FM-1 while writing. [y/N] "):
        dev.link.close()
        print("cancelled", file=out)
        return 1
    fm1_ok = a.fm1                       # a stock package not laid out for the SMK-37: only on an FM-1, confirmed
    if (not fm1_ok and not a.yes and is_stock(image) and package_board(image) != "smk"
            and not SMK_MODEL_RE.fullmatch(model_of(dev.id.text))):
        print(FM1_CONFIRM_MSG, file=out)
        fm1_ok = ask("Is the device an FM-1 (not an SMK-37)? [y/N] ")
    prog = Progress(out)
    try:
        up.install(dev, image, product, prog, fm1_ok)
    finally:
        prog.end()
    return 0


def ask_tty(prompt):
    try:
        return input(prompt).strip().lower() in ("y", "yes")
    except EOFError:
        return False


def main(argv=None, backend=None, out=sys.stdout, ask=ask_tty):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("package", nargs="?", help="Felucca package (.fwsc)")
    ap.add_argument("--info", action="store_true", help="print the identity of the connected FM-1")
    ap.add_argument("--port", metavar="NAME", help="MIDI port to use (part of its name)")
    ap.add_argument("--yes", action="store_true", help="do not ask for confirmation")
    ap.add_argument("--force", action="store_true", help="install a package without the Felucca loader marker")
    ap.add_argument("--fm1", action="store_true", help="the device is an FM-1: a stock package not laid out for the SMK-37 may be written (never to an SMK-37)")
    a = ap.parse_args(argv)
    if bool(a.info) == bool(a.package):
        ap.error("give a PACKAGE.fwsc or --info")
    try:
        if backend is None:
            try:
                backend = MidoBackend()
            except ImportError:
                raise InstallError("usage", "needs mido and python-rtmidi (pip install mido python-rtmidi)")
        return run(a, backend, out, ask)
    except InstallError as e:
        print(f"error: {e}", file=sys.stderr)
        return EXIT.get(e.code, 1)
    except KeyboardInterrupt:
        print("\ninterrupted: if the FM-1 is in update mode, run the install again to finish", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
