#!/usr/bin/env python3
"""Writes a GameCube disc image from an extracted disc, for the programs that read a whole image.

The decompilation and the port's importer read the disc as extracted, orig/<version>/sys and orig/<version>/files.
The port before step 10e, such as the old GX front-end version (README.md, The old GX front-end version), opens a
whole disc image. This script lays the extract out as one: the system areas at their fixed offsets, the boot
program and the file table where the disc header places them, and each file at the offset the file table records.
The image is a sparse file, so the gaps between files take no space on file systems that keep holes.

    python3 port/tools/disc_image.py orig/GAFE01_00 GAFE01_00.iso
"""
import struct
import sys
from pathlib import Path

DISC_SIZE = 1459978240  # a GameCube disc's size in bytes
ENTRY = 12  # a file table entry: name offset and flag word, then offset/parent, then size/next


def read(path):
    return path.read_bytes()


def write_image(extract, out):
    sys_dir = extract / "sys"
    files_dir = extract / "files"
    boot = read(sys_dir / "boot.bin")
    dol_offset, fst_offset, _ = struct.unpack(">III", boot[0x420:0x42C])
    fst = read(sys_dir / "fst.bin")
    count = struct.unpack(">I", fst[8:12])[0]
    names = fst[count * ENTRY:]

    def name(offset):
        return names[offset:names.index(b"\0", offset)].decode("shift_jis")

    with open(out, "wb") as image:
        image.truncate(DISC_SIZE)

        def put(offset, data):
            image.seek(offset)
            image.write(data)

        put(0, boot)
        put(0x440, read(sys_dir / "bi2.bin"))
        put(0x2440, read(sys_dir / "apploader.img"))
        put(dol_offset, read(sys_dir / "main.dol"))
        put(fst_offset, fst)

        def walk(first, end, directory):
            index = first
            while index < end:
                word, offset, size = struct.unpack(">III", fst[index * ENTRY:(index + 1) * ENTRY])
                path = directory / name(word & 0xFFFFFF)
                if word >> 24:  # a directory: size is the index after its last entry
                    walk(index + 1, size, path)
                    index = size
                    continue
                data = read(path)
                if len(data) != size:
                    sys.exit(f"disc_image: {path} holds {len(data)} bytes, the file table says {size}")
                put(offset, data)
                index += 1

        walk(1, count, files_dir)
    print(f"disc_image: wrote {out}, {boot[:6].decode('ascii', 'replace')} with {count - 1} entries")


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: disc_image.py EXTRACT IMAGE")
    write_image(Path(sys.argv[1]), Path(sys.argv[2]))


if __name__ == "__main__":
    main()
