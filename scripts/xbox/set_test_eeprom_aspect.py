"""Set only aspect flags/checksum in a disposable EEPROM copy.
Layout: https://github.com/Ernegien/XboxEepromEditor/blob/master/XboxEepromEditor/Eeprom.cs
Flags: https://github.com/Ernegien/XboxEepromEditor/blob/master/XboxEepromEditor/Types/VideoSettings.cs
Checksum: https://github.com/xemu-project/xemu/blob/master/hw/xbox/eeprom_generation.c
"""
import argparse, struct
from pathlib import Path

def checksum(data):
    total = sum(struct.unpack('<24I', data[0x60:0xc0]))
    while total > 0xffffffff:
        total = (total & 0xffffffff) + (total >> 32)
    return total

def set_aspect(path, wide):
    data = bytearray(path.read_bytes())
    if len(data) != 256 or checksum(data) != 0xffffffff:
        raise ValueError('Invalid test EEPROM length or user checksum')
    original = bytes(data)
    flags = struct.unpack_from('<I', data, 0x94)[0]
    flags = (flags & ~0x110000) | (0x10000 if wide else 0)
    struct.pack_into('<I', data, 0x94, flags)
    struct.pack_into('<I', data, 0x60, 0)
    struct.pack_into('<I', data, 0x60, checksum(data) ^ 0xffffffff)
    assert checksum(data) == 0xffffffff
    assert all(a == b or i in range(0x60, 0x64) or i in range(0x94, 0x98)
               for i, (a, b) in enumerate(zip(original, data)))
    path.write_bytes(data)
    print('Test EEPROM aspect: ' + ('16:9' if wide else '4:3'))

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--test-copy', required=True, type=Path)
    p.add_argument('--aspect', choices=['4:3','16:9'], required=True)
    args = p.parse_args()
    set_aspect(args.test_copy, args.aspect == '16:9')
