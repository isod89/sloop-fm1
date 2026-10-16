#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Build an FM-1 update package (.fwsc) from the Felucca app and update loader.

  flash.bin  head [0, 0x4000): flash header (JieLi SDK default values), JLFS
             entries, the SDK SPL (uboot.boot, Apache-2.0) and an isd_config
             blob that decodes to the chip key. The update loader never writes
             the head.
             app area 0x4000..: app_area_head, app.bin, the SDK cfg_tool.bin and
             eq_cfg_hw.bin, the region descriptors; SFC-encrypted with the chip key
  ota.bin    the Felucca update loader (firmware/loader)

The SDK files come from the JieLi AC79 SDK (AC79_SDK, or --sdk).
All integrity checks in the format are CRC16 (poly 0x1021, init 0).

  fm1pkg_make.py APP.bin OTA.bin OUT.fwsc [--board fm1|smk37] [--product FM-1_9XY] [--sdk DIR]

--board smk37 (M-Vave SMK-37 Pro, same AC791N, SPL and loader) selects the SMK flash layout: PRCT
0x9D000 and the stock v016 region table (VM, BTIF, USRTRIM, USRFLASH, USR; see the fm1-to-smk37 guide).
"""
import argparse
import os
import struct
from pathlib import Path

APP_SLOT = 0x8DFBC
FLASH_SIZE = 0x93000
BOARD = "fm1"
# the region descriptors after app.bin / cfg_tool.bin: (offset, size, flags, resvd, index, name16 or name).
# The offsets are ABSOLUTE flash offsets. The 16 name bytes after the names are SDK packer fields.
REGIONS = {
    "fm1": [(0x93000, 352256, 0x12, 0x80, 0, bytes.fromhex("564d00ffffffffffffffffff00800000")),     # VM
            (0, None, 0x92, 0x82, 0, bytes.fromhex("5052435400ffffffffffffff434f4445")),         # PRCT = FLASH_SIZE
            (0xE9000, 0x1000, 0x92, 0x80, 0, bytes.fromhex("4254494600ffffffffffffff4155544f")),  # BTIF
            (0xEA000, 0x12000, 0x92, 0x81, 1, "USR")],
    # SMK-37 Pro v016, exactly as the stock package (decrypted flash 0x4060..0x4100)
    "smk37": [(0x9D000, 0x23000, 0x12, 0x81, 0, bytes.fromhex("564d00ffffffffffffffffff00800000")),    # VM
              (0, None, 0x92, 0x82, 0, bytes.fromhex("5052435400ffffffffffffff434f4445")),        # PRCT = FLASH_SIZE
              (0xC0000, 0x1000, 0x92, 0x82, 0, bytes.fromhex("4254494600ffffffffffffff4155544f")), # BTIF
              (0xC1000, 0x1000, 0x92, 0x81, 0, bytes.fromhex("5553525452494d00ffffffffffffffff")), # USRTRIM
              (0xC2000, 0x29000, 0x92, 0x81, 0, bytes.fromhex("555352464c41534800ffffffffffffff")), # USRFLASH
              (0xF4000, 0xA000, 0x92, 0x81, 1, "USR")],
}
# per board: (FLASH_SIZE = PRCT = the end of what the update loader writes, APP_SLOT, default product)
BOARDS = {"fm1": (0x93000, 0x8DFBC, "FM-1_900"),
          "smk37": (0x9D000, 0x97400, "SMK-37 Pro_900")}   # SMK: the stock app is 0x974B8; build.py sets app.ld XIP


def set_board(board):
    """select the flash layout of a board ("fm1" default, "smk37")"""
    global BOARD, FLASH_SIZE, APP_SLOT
    if board not in BOARDS:
        raise SystemExit(f"fm1pkg_make: unknown board {board!r} (fm1, smk37)")
    BOARD = board
    FLASH_SIZE, APP_SLOT, _ = BOARDS[board]
KEY = 0x980F
# flash header: JieLi SDK defaults (burner_size 544, VID "0.01", flash_size 0xFF000,
# fs_ver 0x10, PID "AC791N_STORY"), plain, without its CRC
FLASH_HDR = bytes.fromhex("20022f104cc900f00f001010a4ff4e5d0b41e0cd781dc856417fb367cf9f")
UFW_CHIP = b"AC791N"
SDK = None


def _crc_table():
    t = []
    for i in range(256):
        c = i << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else (c << 1)
        t.append(c & 0xFFFF)
    return t


_T = _crc_table()


def crc16(data, crc=0):
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ _T[((crc >> 8) ^ b) & 0xFF]
    return crc


def enc(buf, off, size, key=0xFFFF):
    """JieLi ENC stream cipher (in place)"""
    for i in range(size):
        buf[off + i] ^= key & 0xFF
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xFFFF


def sfc(buf, off, size, base, key):
    """SFC cipher: ENC per 32-byte block, key mixed with the block address"""
    for i in range(0, size, 32):
        enc(buf, off + i, min(size - i, 32), key ^ ((off + i - base) >> 2))


def chipkey_decode(d):
    """chip key from the 32-byte isd_config blob"""
    thesum = sum(d[:16]) & 0xFF
    thesum = 0xAA if thesum >= 0xE0 else 0x55 if thesum <= 0x10 else thesum
    return sum(1 << i for i in range(16) if (d[16 + i] ^ d[15 - i]) < thesum)


def entry(dcrc, offset, size, flags, resvd, index, name, fill=0xFF):
    if isinstance(name, bytes):                     # 16 raw bytes: name + the packer's trailing fields
        nm = name
    else:
        nm = name.encode() + b"\0"
        nm = nm + bytes([fill]) * (16 - len(nm))
    body = struct.pack("<HIIBBH16s", dcrc, offset & 0xFFFFFFFF, size & 0xFFFFFFFF, flags, resvd, index, nm)
    return struct.pack("<H", crc16(body)) + body


def key_blob(key):
    """32 bytes that chipkey_decode() turns into `key` (+ CRC16)"""
    d = bytearray([0x10] * 16) + bytearray(16)        # sum 0 -> threshold 0x55
    for i in range(16):
        d[16 + i] = d[15 - i] ^ (0x00 if key >> i & 1 else 0xFF)
    assert chipkey_decode(bytes(d)) == key
    return bytes(d) + struct.pack("<H", crc16(d))


def sdk_file(rel):
    root = Path(SDK or os.environ.get("AC79_SDK", ""))
    path = root / "cpu" / "wl82" / "tools" / rel
    try:
        return path.read_bytes()
    except OSError as e:
        raise SystemExit(f"fm1pkg_make: {path}: {e.strerror}. Set AC79_SDK to a JieLi AC79 SDK checkout.")


def flash_image(app, key):
    spl = sdk_file("uboot.boot")                         # SDK SPL, BANKCB-encoded as in the SDK
    cfg_tool = sdk_file("cfg_tool.bin")
    eq = sdk_file("cfg/eq_cfg_hw.bin")
    isd = key_blob(key) + b"[FELUCCA]\r\n"
    f = bytearray(b"\xFF" * FLASH_SIZE)
    # ---- head
    spl_off = 0xA0
    isd_off = spl_off + len(spl)
    assert isd_off + len(isd) <= 0x4000
    f[spl_off:spl_off + len(spl)] = spl
    f[isd_off:isd_off + len(isd)] = isd
    top = (entry(crc16(spl), spl_off, len(spl), 0x00, 0x00, 0, "uboot.boot") +
           entry(crc16(isd), isd_off, len(isd), 0x02, 0x80, 0, "isd_config.ini") +
           entry(0xFFFF, 0x4000, 0xFFFFFFFF, 0x81, 0xFF, 0, "app_dir_head") +
           entry(0xFFFF, 0xFF000, 0x1000, 0x12, 0x01, 0xFFFF, "key_mac"))
    hdr = struct.pack("<H", crc16(FLASH_HDR)) + FLASH_HDR
    head = bytearray(hdr + top)
    for o in range(0, len(head), 32):                   # header and each JLFS entry: ENC 0xFFFF
        enc(head, o, 32)
    f[0:len(head)] = head
    # ---- app area (plain first, then SFC)
    if len(app) > APP_SLOT:
        raise SystemExit(f"app is {len(app)} B, the slot is {APP_SLOT} B")
    app = app + b"\xFF" * (APP_SLOT - len(app))
    blk = 0x120 + APP_SLOT + len(cfg_tool)              # app_area_head block size
    area = bytearray(b"\xFF" * blk)
    area[0x120:0x120 + APP_SLOT] = app
    area[0x120 + APP_SLOT:blk] = cfg_tool
    ents = [entry(crc16(app), 0x120, APP_SLOT, 0x82, 0xFF, 0, "app.bin"),
            entry(crc16(cfg_tool), 0x120 + APP_SLOT, len(cfg_tool), 0x82, 0xFF, 0, "cfg_tool.bin"),
            # region descriptors; the bytes after the names are SDK packer fields (VM size, "CODE", "AUTO")
            *(entry(0xFFFF, o, FLASH_SIZE if sz is None else sz, fl, rs, ix, nm)
              for o, sz, fl, rs, ix, nm in REGIONS[BOARD])]
    area[32:32 + len(b"".join(ents))] = b"".join(ents)
    area[0:32] = entry(crc16(area[32:blk]), 0x02000120, blk, 0x83, 0xFF, 0, "app_area_head")
    # nested cfg directory right after the block: SDK default EQ table
    eq_e = entry(crc16(eq), 0x40, len(eq), 0x82, 0xFF, 1, "eq_cfg_hw.bin")
    cfg_body = eq_e + eq
    cfg = entry(crc16(cfg_body), 0x20, 0x20 + len(cfg_body), 0x83, 0xFF, 1, "cfg") + cfg_body
    region = bytearray(area + cfg)
    sfc(region, 0, len(region), 0, key)
    if 0x4000 + len(region) > FLASH_SIZE:          # (the update loader writes up to FLASH_SIZE only)
        raise SystemExit(f"app area too large: ends at 0x{0x4000 + len(region):x}, past 0x{FLASH_SIZE:x}")
    f[0x4000:0x4000 + len(region)] = region
    return bytes(f)


def ufw(flash, ota, product):
    files = [(0, "flash.bin", flash), (100, "ota.bin", ota)]
    off, ents, data = 0x400, [], bytearray()
    for i, (typ, name, d) in enumerate(files):
        off = (off + 0xFF) & ~0xFF
        e = bytearray(0x50)
        struct.pack_into("<HHHHIII", e, 0, typ, i, crc16(d), 0, off, len(d), (len(d) + 31) & ~31)
        e[0x40:0x40 + len(name)] = name.encode()
        ents.append((e, off, d))
        off += len(d)
    total = off
    hdr = bytearray(0x40)
    struct.pack_into("<IH", hdr, 4, total, len(files))
    struct.pack_into("<HHH", hdr, 10, 0x0004, 0x0200, 0)     # header fields as written by the SDK packer
    hdr[16:16 + len(UFW_CHIP)] = UFW_CHIP
    lst = bytearray()
    for e, _, _ in ents:
        x = bytearray(e)
        enc(x, 0, 0x50)
        lst += x
    struct.pack_into("<H", hdr, 2, crc16(lst))
    struct.pack_into("<H", hdr, 0, crc16(hdr[2:0x40]))
    enc(hdr, 0, 0x40)
    logical = bytearray(b"\xFF" * total)
    logical[0:0x40] = hdr
    logical[0x40:0x40 + len(lst)] = lst
    for e, o, d in ents:
        logical[o:o + len(d)] = d
    # .fwsc: one marker byte after each of the first 20 0x2F-byte blocks (the identity)
    if len(product) > 20 or any((ord(c) + i + 1) & 0xFF == 0x7D for i, c in enumerate(product)):
        raise SystemExit(f"product {product!r}: at most 20 characters, none encoding to the unused marker")
    raw = bytearray()
    for i in range(20):
        ch = product[i] if i < len(product) else None
        m = (ord(ch) + i + 1) & 0xFF if ch else 0x7D
        raw += logical[i * 0x2F:(i + 1) * 0x2F] + bytes([m])
    raw += logical[20 * 0x2F:]
    return bytes(raw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("app", type=Path)
    ap.add_argument("ota", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--board", choices=sorted(BOARDS), default="fm1")
    ap.add_argument("--product", help="package identity (default: FM-1_900; smk37: SMK-37 Pro_900)")
    ap.add_argument("--key", type=lambda s: int(s, 0), default=KEY)
    ap.add_argument("--sdk", type=Path, help="JieLi AC79 SDK checkout (default: $AC79_SDK)")
    a = ap.parse_args()
    global SDK
    SDK = a.sdk
    set_board(a.board)
    a.product = a.product or BOARDS[a.board][2]
    pkg = ufw(flash_image(a.app.read_bytes(), a.key), a.ota.read_bytes(), a.product)
    a.out.write_bytes(pkg)
    print(f"package {a.out}: {len(pkg)} B, identity {a.product}, chip key {a.key:#06x}")


if __name__ == "__main__":
    main()
