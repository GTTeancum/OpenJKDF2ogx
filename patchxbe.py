#!/usr/bin/env python
# patchxbe.py - Patch PE subsystem to Xbox (14), run imagebld, then inject
# D3D8/XGRAPHC library version entries so CXBX-Reloaded enables D3D8 HLE.
#
# Usage: patchxbe.py <exe_path> [xbe_out_path]
# If xbe_out_path is omitted, outputs to default.xbe in the same directory.

import sys
import os
import struct
import shutil
import subprocess

exe_path = sys.argv[1]
if len(sys.argv) >= 3:
    xbe_path = sys.argv[2]
else:
    xbe_path = os.path.join(os.path.dirname(exe_path), 'default.xbe')

# ── Step 1: Patch PE subsystem to Xbox (14) ──────────────────────────────

with open(exe_path, 'rb') as f:
    data = bytearray(f.read())

pe_offset = struct.unpack_from('<I', data, 0x3C)[0]

if data[pe_offset:pe_offset+4] != b'PE\0\0':
    print("ERROR: Not a valid PE file")
    sys.exit(1)

subsystem_offset = pe_offset + 24 + 68
old_subsystem = struct.unpack_from('<H', data, subsystem_offset)[0]
print("Original subsystem: %d" % old_subsystem)

struct.pack_into('<H', data, subsystem_offset, 14)
print("Patched subsystem to: 14 (Xbox)")

temp_exe = exe_path + '.xbox.tmp'
with open(temp_exe, 'wb') as f:
    f.write(data)

# ── Step 1b: Strip KERNEL32.dll import descriptor ────────────────────────
# libcd.lib pulls in RtlUnwind from KERNEL32 via SEH. imagebld rejects any
# PE that imports from Win32 DLLs. We zero out the KERNEL32 import descriptor
# so the import table only contains xboxkrnl.exe entries.

with open(temp_exe, 'rb') as f:
    pdata = bytearray(f.read())

pe_off = struct.unpack_from('<I', pdata, 0x3C)[0]
opt_off = pe_off + 24
# Import table RVA and size from data directory entry 1
import_rva  = struct.unpack_from('<I', pdata, opt_off + 104)[0]
import_size = struct.unpack_from('<I', pdata, opt_off + 108)[0]

# Find the section that contains the import table to convert RVA -> file offset
num_sections = struct.unpack_from('<H', pdata, pe_off + 6)[0]
sections_off = opt_off + struct.unpack_from('<H', pdata, opt_off - 2 + 2)[0]  # SizeOfOptionalHeader
sections_off = pe_off + 24 + struct.unpack_from('<H', pdata, pe_off + 20)[0]

def rva_to_offset(rva):
    for i in range(num_sections):
        s = sections_off + i * 40
        vaddr = struct.unpack_from('<I', pdata, s + 12)[0]
        vsize = struct.unpack_from('<I', pdata, s + 16)[0]
        raw   = struct.unpack_from('<I', pdata, s + 20)[0]
        if vaddr <= rva < vaddr + vsize:
            return raw + (rva - vaddr)
    return None

import_off = rva_to_offset(import_rva)
if import_off is not None:
    # Walk import descriptors (20 bytes each, terminated by all-zero entry)
    pos = import_off
    while True:
        desc = pdata[pos:pos+20]
        if desc == b'\x00' * 20:
            break
        name_rva = struct.unpack_from('<I', desc, 12)[0]
        name_off = rva_to_offset(name_rva)
        if name_off is not None:
            dll_name = pdata[name_off:name_off+32].split(b'\x00')[0].decode('ascii', errors='replace')
            if dll_name.upper() == 'KERNEL32.DLL':
                print("Zeroing KERNEL32.dll import descriptor at offset 0x%X" % pos)
                pdata[pos:pos+20] = b'\x00' * 20
        pos += 20

with open(temp_exe, 'wb') as f:
    f.write(pdata)
print("KERNEL32 import descriptor stripped.")

# ── Step 2: Run imagebld ─────────────────────────────────────────────────

imagebld = r'C:\XDK_5558\XDK\xbox\bin\imagebld.exe'
cmd = [imagebld, '/IN:' + temp_exe, '/OUT:' + xbe_path]
cmd.append('/TESTNAME:Star Wars Jedi Knight')
cmd.append('/TESTID:0x4C410001')
cmd.append('/STACK:0x40000')
cmd.append('/TESTMEDIATYPES:0xFFFFFFFF')

# Title image (128x128 BMP) — shown in Xbox dashboard
script_dir = os.path.dirname(os.path.abspath(__file__))
title_icon = os.path.join(script_dir, 'resource', 'icon_title.bmp')
save_icon = os.path.join(script_dir, 'resource', 'icon_save.bmp')
if os.path.isfile(title_icon):
    cmd.append('/TITLEIMAGE:' + title_icon)
    print("Using title image: " + title_icon)
# /SAVEIMAGE not supported by all imagebld versions — skip if unsupported
# if os.path.isfile(save_icon):
#     cmd.append('/SAVEIMAGE:' + save_icon)
#     print("Using save image: " + save_icon)

print("Running: " + ' '.join(cmd))
result = subprocess.call(cmd)

os.remove(temp_exe)

if result != 0:
    print("imagebld failed with code %d" % result)
    sys.exit(result)

print("XBE created: " + xbe_path)

# ── Step 3: Inject D3D8 + XGRAPHC library version entries ────────────────
# We link d3d8ltcg.lib + xgraphicsltcg.lib, which show up in the XBE library
# table as D3D8LTCG and XGRAPHCLTCG.  Retail games link d3d8.lib/xgraphics.lib
# and have entries named D3D8/XGRAPHC.  The Xbox kernel reads this table
# during XBE load to configure hardware (DAC scanout path among others)
# based on detected library versions; if it only recognises D3D8/XGRAPHC
# (not the LTCG variants) our XBE gets the wrong DAC config — which is
# consistent with the corrupted-output-regardless-of-back-buffer-format
# behaviour observed on hardware.  Inject D3D8 and XGRAPHC entries so the
# kernel finds names it knows.
#
# (Previous comment claimed this might "confuse the kernel" — that was a
# guess made before we had the corruption evidence; the opposite is more
# likely true.  Override with PATCHXBE_INJECT_LIBS=0 to disable again.)

if os.environ.get('PATCHXBE_INJECT_LIBS', '0') != '1':
    print("Skipping D3D8/XGRAPHC library injection (real D3D8 linked from XDK 5558)")
    sys.exit(0)

print("Injecting D3D8/XGRAPHC library version entries...")

with open(xbe_path, 'rb') as f:
    xbe = bytearray(f.read())

base_addr = struct.unpack_from('<I', xbe, 0x104)[0]

# Read current library version table info
lib_count  = struct.unpack_from('<I', xbe, 0x160)[0]
lib_va     = struct.unpack_from('<I', xbe, 0x164)[0]
lib_offset = lib_va - base_addr  # File offset of the table

print("  Current library count: %d" % lib_count)
print("  Library table VA: 0x%08X (file offset 0x%X)" % (lib_va, lib_offset))

# Read existing entries to get the build number
if lib_count > 0:
    first_entry = xbe[lib_offset:lib_offset+16]
    existing_build = struct.unpack_from('<H', first_entry, 12)[0]
else:
    existing_build = 5849
print("  Using build number: %d" % existing_build)

# Build the two new 16-byte library version entries
# Format: 8-char name (padded with nulls) + WORD major + WORD minor + WORD build + WORD flags
def make_lib_entry(name, build, flags=0x4001):
    name_bytes = name.encode('ascii')[:8].ljust(8, b'\x00')
    return name_bytes + struct.pack('<HHHH', 1, 0, build, flags)

d3d8_entry    = make_lib_entry('D3D8',    existing_build)
xgraphc_entry = make_lib_entry('XGRAPHC', existing_build)

# Strategy: append the new entries right after the existing table,
# then update the count. This works because imagebld leaves padding
# or we can safely extend into the space after the table.
#
# First check what's after the current table
table_end = lib_offset + lib_count * 16
bytes_after = xbe[table_end:table_end+32]
print("  Bytes after table: %s" % bytes_after.hex())

# We need 32 bytes (2 entries). Append at end of file and update pointers.
# Actually, the safest approach: append new entries to end of XBE file,
# then rewrite the header to point to a new combined table there.

# Build complete new library table: existing entries + D3D8 + XGRAPHC
new_entries = bytearray()
for i in range(lib_count):
    off = lib_offset + i * 16
    new_entries += xbe[off:off+16]
new_entries += d3d8_entry
new_entries += xgraphc_entry
new_count = lib_count + 2

# Align file size to 4 bytes before appending
while len(xbe) % 4 != 0:
    xbe.append(0)

# New table goes at end of file
new_table_file_offset = len(xbe)
new_table_va = new_table_file_offset + base_addr

# Append the new table
xbe += new_entries

# Update header: count and table pointer
struct.pack_into('<I', xbe, 0x160, new_count)
struct.pack_into('<I', xbe, 0x164, new_table_va)

# Update kernel and XAPI library version pointers to point into new table
# XAPI is entry[0], kernel is the XBOXKRNL entry
for i in range(new_count):
    off = new_table_file_offset + i * 16
    name = xbe[off:off+8].rstrip(b'\x00').decode('ascii', errors='replace')
    entry_va = new_table_va + i * 16
    if name.startswith('XAPILIB'):
        struct.pack_into('<I', xbe, 0x16C, entry_va)
        print("  XAPI lib version -> entry[%d] VA 0x%08X" % (i, entry_va))
    elif name == 'XBOXKRNL':
        struct.pack_into('<I', xbe, 0x168, entry_va)
        print("  Kernel lib version -> entry[%d] VA 0x%08X" % (i, entry_va))

print("  New library count: %d" % new_count)
print("  New table VA: 0x%08X (file offset 0x%X)" % (new_table_va, new_table_file_offset))

# Print final table
for i in range(new_count):
    off = new_table_file_offset + i * 16
    name = xbe[off:off+8].rstrip(b'\x00').decode('ascii', errors='replace')
    major, minor, build, flags = struct.unpack_from('<HHHH', xbe, off + 8)
    print("  [%d] %-8s v%d.%d.%d flags=0x%04X" % (i, name, major, minor, build, flags))

with open(xbe_path, 'wb') as f:
    f.write(xbe)

print("XBE patched successfully.")
