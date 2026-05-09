"""
patch_subsystem.py - Patches PE subsystem field to 14 (Xbox)
Usage: python patch_subsystem.py <pe_file>
"""
import sys
import struct

if len(sys.argv) < 2:
    print("Usage: patch_subsystem.py <pe_file>")
    sys.exit(1)

path = sys.argv[1]

with open(path, 'r+b') as f:
    data = bytearray(f.read())

# PE signature offset is at 0x3C
pe_off = struct.unpack_from('<I', data, 0x3C)[0]

# Verify PE signature
sig = data[pe_off:pe_off+4]
if sig != b'PE\x00\x00':
    print("ERROR: Not a valid PE file (sig=%r)" % sig)
    sys.exit(1)

# Optional header starts at pe_off + 4 (sig) + 20 (COFF header)
opt_off = pe_off + 4 + 20

# Subsystem is at offset 68 within the optional header
sub_off = opt_off + 68

old_sub = struct.unpack_from('<H', data, sub_off)[0]
print("PE offset: 0x%X" % pe_off)
print("Subsystem offset: 0x%X" % sub_off)
print("Old subsystem: %d" % old_sub)

struct.pack_into('<H', data, sub_off, 14)

with open(path, 'wb') as f:
    f.write(data)

print("Patched subsystem to 14 (Xbox)")
