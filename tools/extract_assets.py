from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path

TYPES = {1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG", 6: "STRING", 7: "FONTDIR", 8: "FONT", 9: "ACCELERATOR", 10: "RCDATA", 11: "MESSAGETABLE", 12: "GROUP_CURSOR", 14: "GROUP_ICON", 16: "VERSION", 17: "DLGINCLUDE", 19: "PLUGPLAY", 20: "VXD", 21: "ANICURSOR", 22: "ANIICON", 23: "HTML", 24: "MANIFEST"}


def parse(source: Path) -> tuple[dict, list[dict]]:
    data = source.read_bytes()
    def get(fmt: str, offset: int):
        return struct.unpack_from("<" + fmt, data, offset)
    pe_offset = get("I", 0x3c)[0]
    if data[pe_offset:pe_offset + 4] != b"PE\x00\x00":
        raise ValueError("Not a PE file")
    coff = pe_offset + 4
    machine, count, timestamp, sym_offset, sym_count, optional_size, flags = get("HHIIIHH", coff)
    opt = coff + 20
    magic = get("H", opt)[0]
    if magic != 0x10b:
        raise ValueError("Expected PE32 executable")
    sections = []
    for i in range(count):
        off = opt + optional_size + 40 * i
        name, virtual_size, rva, raw_size, raw_offset, relocs, linenums, nrelocs, nline, characteristics = get("8sIIIIIIHHI", off)
        sections.append(dict(name=name.split(b"\0")[0].decode(), virtual_size=virtual_size, rva=rva, file_size=raw_size, file_offset=raw_offset, characteristics=hex(characteristics)))
    def to_offset(rva: int) -> int:
        for section in sections:
            start = section["rva"]
            if start <= rva < start + max(section["virtual_size"], section["file_size"]):
                return section["file_offset"] + rva - start
        if rva < get("I", opt + 60)[0]:
            return rva
        raise ValueError(f"Unmapped RVA 0x{rva:x}")
    directory_base = opt + 96
    def directory(idx: int):
        return get("II", directory_base + idx * 8)
    resource_rva, resource_length = directory(2)
    resource_start = to_offset(resource_rva)
    parsed = []
    def walk(off, path, depth=0):
        if depth > 6:
            raise ValueError("Resource depth exceeded")
        characteristics, stamp, major, minor, names, ids = get("IIHHHH", resource_start + off)
        for index in range(names + ids):
            key, value = get("II", resource_start + off + 16 + index * 8)
            if key & 0x80000000:
                char_offset = resource_start + (key & 0x7fffffff)
                length = get("H", char_offset)[0]
                name = data[char_offset + 2:char_offset + 2 + length * 2].decode("utf-16le", errors="replace")
            else:
                name = key
            chain = path + [name]
            if value & 0x80000000:
                walk(value & 0x7fffffff, chain, depth + 1)
            else:
                rva, size, codepage, reserved = get("IIII", resource_start + value)
                offset = to_offset(rva)
                content = data[offset:offset + size]
                if len(content) != size:
                    raise ValueError(f"Truncated resource: {chain}")
                parsed.append(dict(path=chain, type=TYPES.get(chain[0], str(chain[0])), name=chain[1] if len(chain) > 1 else None, language=chain[2] if len(chain) > 2 else None, rva=rva, file_offset=offset, size=size, codepage=codepage, sha256=hashlib.sha256(content).hexdigest(), content=content))
    if resource_rva:
        walk(0, [])
    imports = []
    imp_rva, imp_len = directory(1)
    if imp_rva:
        off = to_offset(imp_rva)
        for i in range(200):
            original, stamp, chain, name_rva, first = get("IIIII", off + i * 20)
            if not any((original, stamp, chain, name_rva, first)):
                break
            name_off = to_offset(name_rva)
            end = data.index(b"\x00", name_off)
            imports.append(data[name_off:end].decode("ascii", errors="replace"))
    delay_imports = []
    delay_rva, delay_len = directory(13)
    if delay_rva:
        off = to_offset(delay_rva)
        for i in range(100):
            values = get("IIIIIIII", off + i * 32)
            if not any(values):
                break
            name_rva = values[1] if values[0] & 1 else values[1] - get("I", opt + 28)[0]
            name_off = to_offset(name_rva)
            end = data.index(b"\x00", name_off)
            delay_imports.append(data[name_off:end].decode("ascii", errors="replace"))
    info = dict(file=source.name, sha256=hashlib.sha256(data).hexdigest(), file_size=len(data), machine=hex(machine), timestamp_utc=__import__("datetime").datetime.fromtimestamp(timestamp, __import__("datetime").timezone.utc).isoformat(), characteristics=hex(flags), link_version=f"{data[opt+2]}.{data[opt+3]}", image_base=hex(get("I", opt+28)[0]), entry_point_rva=hex(get("I", opt+16)[0]), subsystem=get("H", opt+68)[0], section_alignment=get("I", opt+32)[0], file_alignment=get("I", opt+36)[0], section_count=count, sections=sections, resource_directory=dict(rva=resource_rva, size=resource_length), imports=imports, delay_imports=delay_imports, resource_count=len(parsed))
    return info, parsed


def strings_table(content, block):
    result = {}
    off = 0
    for idx in range(16):
        length = struct.unpack_from("<H", content, off)[0]
        off += 2
        value = content[off:off + length * 2].decode("utf-16le", errors="replace")
        off += length * 2
        if value:
            result[str((int(block) - 1) * 16 + idx)] = value
    return result


def bitmap_bytes(dib):
    if len(dib) < 40:
        return None
    hsize, width, height, planes, bpp, compression, size_image, xpels, ypels, clr_used, important = struct.unpack_from("<IiiHHIIiiII", dib, 0)
    if hsize < 40 or len(dib) < hsize:
        return None
    palette_entries = (clr_used or (1 << bpp if bpp <= 8 else 0))
    rgb_mask_bytes = 12 if compression == 3 and hsize == 40 else 0
    offset_bits = 14 + hsize + palette_entries * 4 + rgb_mask_bytes
    if offset_bits > 14 + len(dib):
        return None
    return struct.pack("<2sIHHI", b"BM", len(dib) + 14, 0, 0, offset_bits) + dib


def build_icon(group, icon_lookup):
    content = group["content"]
    if len(content) < 6:
        return None
    reserved, kind, count = struct.unpack_from("<HHH", content)
    if kind != 1 or reserved or 6 + count * 14 > len(content):
        return None
    index = []
    payload = bytearray()
    for i in range(count):
        width, height, colors, reserved, planes, bits, nbytes, ident = struct.unpack_from("<BBBBHHIH", content, 6 + i * 14)
        raw = icon_lookup.get((ident, group["language"]))
        if raw is None:
            return None
        offset = 6 + count * 16 + len(payload)
        index.append(struct.pack("<BBBBHHII", width, height, colors, reserved, planes, bits, len(raw), offset))
        payload.extend(raw)
    return struct.pack("<HHH", 0, 1, count) + b"".join(index) + payload



def build_cursor(group, cursor_lookup):
    data = group["content"]
    if len(data) < 6:
        return None
    reserved, kind, count = struct.unpack_from("<HHH", data)
    if reserved or kind != 2 or len(data) < 6 + 14 * count:
        return None
    entries = []
    body = bytearray()
    for i in range(count):
        width, double_height, planes, bits, size, resource_id = struct.unpack_from("<HHHHIH", data, 6 + 14 * i)
        raw = cursor_lookup.get((resource_id, group["language"]))
        if not raw or len(raw) < 4:
            return None
        hotspot_x, hotspot_y = struct.unpack_from("<HH", raw)
        dib = raw[4:]
        offset = 6 + 16 * count + len(body)
        entries.append(struct.pack("<BBBBHHII", width if width < 256 else 0, double_height // 2 if double_height < 512 else 0, 0, 0, hotspot_x, hotspot_y, len(dib), offset))
        body.extend(dib)
    return struct.pack("<HHH", 0, 2, count) + b"".join(entries) + body

def main():
    cli = argparse.ArgumentParser()
    cli.add_argument("exe", type=Path)
    cli.add_argument("output", type=Path)
    args = cli.parse_args()
    info, resources = parse(args.exe)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "raw").mkdir(exist_ok=True)
    inventory = []
    string_data = {}
    lookup_icons = {(r["name"], r["language"]): r["content"] for r in resources if r["type"] == "ICON"}
    lookup_cursors = {(r["name"], r["language"]): r["content"] for r in resources if r["type"] == "CURSOR"}
    for resource in resources:
        typ = str(resource["type"])
        name = str(resource["name"])
        lang = str(resource["language"])
        outpath = Path("raw") / typ / name / f"{lang}.bin"
        target = args.output / outpath
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(resource["content"])
        meta = {k: v for k, v in resource.items() if k != "content"}
        meta["raw_path"] = outpath.as_posix()
        inventory.append(meta)
        if typ == "STRING":
            string_data.update(strings_table(resource["content"], resource["name"]))
        if typ == "BITMAP":
            bmp = bitmap_bytes(resource["content"])
            if bmp is not None:
                (args.output / f"bitmap_{name}.bmp").write_bytes(bmp)
        if typ == "MANIFEST":
            (args.output / f"manifest_{name}.xml").write_bytes(resource["content"])
        if typ == "GROUP_CURSOR":
            cursor = build_cursor(resource, lookup_cursors)
            if cursor:
                (args.output / f"cursor_{name}.cur").write_bytes(cursor)
        if typ == "GROUP_ICON":
            icon = build_icon(resource, lookup_icons)
            if icon:
                (args.output / f"icon_{name}.ico").write_bytes(icon)
    (args.output / "strings.json").write_text(json.dumps(string_data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (args.output / "resources.json").write_text(json.dumps(inventory, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (args.output.parent / "analysis" / "pe_metadata.json").parent.mkdir(parents=True, exist_ok=True)
    (args.output.parent / "analysis" / "pe_metadata.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
    print("Extracted", len(resources), "resources;", len(string_data), "non-empty strings; sha256:", info["sha256"])

if __name__ == "__main__":
    main()
