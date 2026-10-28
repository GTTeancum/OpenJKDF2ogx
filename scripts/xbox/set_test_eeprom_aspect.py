"""Set aspect and scan flags in a disposable EEPROM copy.
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

def set_video(path, aspect='System', scan='System'):
    data = bytearray(path.read_bytes())
    if len(data) != 256 or checksum(data) != 0xffffffff:
        raise ValueError('Invalid test EEPROM length or user checksum')
    original = bytes(data)
    flags = struct.unpack_from('<I', data, 0x94)[0]
    if aspect != 'System':
        flags = (flags & ~0x110000) | (0x10000 if aspect == '16:9' else 0)
    if scan != 'System':
        flags &= ~(0x80000 | 0x20000)
        if scan == '480p':
            flags |= 0x80000
        elif scan == '720p':
            flags |= 0x80000 | 0x20000  # Keep 480p available as fallback.
    struct.pack_into('<I', data, 0x94, flags)
    struct.pack_into('<I', data, 0x60, 0)
    struct.pack_into('<I', data, 0x60, checksum(data) ^ 0xffffffff)
    assert checksum(data) == 0xffffffff
    assert all(a == b or i in range(0x60, 0x64) or i in range(0x94, 0x98)
               for i, (a, b) in enumerate(zip(original, data)))
    path.write_bytes(data)
    print(f'Test EEPROM video: aspect={aspect} scan={scan}')

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--test-copy', required=True, type=Path)
    p.add_argument('--aspect', choices=['System','4:3','16:9'], default='System')
    p.add_argument('--scan', choices=['System','480i','480p','720p'], default='System')
    args = p.parse_args()
    if args.aspect == 'System' and args.scan == 'System':
        p.error('specify --aspect or --scan')
    set_video(args.test_copy, args.aspect, args.scan)
