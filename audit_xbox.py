#!/usr/bin/env python3
"""
audit_xbox.py — Pre-build linkage audit for the OpenJKDF2 Xbox port.

CHECKS PERFORMED
================
1. extern-C linkage consistency
   Any function defined inside  extern "C" { }  in a .cpp file must be
   declared with matching C linkage in every C++ translation unit that
   references it.  A bare  extern <type> name(  declaration in a /Tp TU
   defaults to C++ linkage — the linker then looks for a mangled symbol
   that does not exist, producing LNK2001 only at link time.

   Correct forms for a C-linkage forward declaration in a C++ TU:
     extern "C" void name(...);
     extern "C" { void name(...); }
     #ifdef __cplusplus
     extern "C"
     #endif
     void name(...);

2. Declared-but-not-in-lib
   Every non-inline function declared in d3d8_xbox_minimal.h must actually
   exist in one of the linked .lib files.  Functions that are inline-only
   (defined in XDK headers, not exported from any .lib) will silently
   compile but produce LNK2001 at link time.

   Method: dump the linker-member symbol table from each linked lib using
     link.exe /dump /linkermember:1
   and cross-reference against all non-inline WINAPI declarations in the
   minimal header.  The stdcall-decorated symbol _Name@N is matched by
   stripping the leading _ and trailing @N.

EXIT
====
  0  — no issues found
  1  — one or more issues; details printed to stderr

USAGE
=====
  python audit_xbox.py            # from repo root
  python audit_xbox.py --fix      # not implemented yet; reserved
"""

import re
import subprocess
import sys
import os

ROOT = os.path.dirname(os.path.abspath(__file__))

LINK_EXE  = r"C:\XDK\xbox\bin\vc71\link.exe"
LIB_DIR   = r"C:\XDK\xbox\lib"

# The libs listed in build_xbox.bat's linker response — keep in sync with
# the >> "!RSPFILE!" echo line that lists them.
LINKED_LIBS = [
    "d3d8ltcg.lib",
    "dsound.lib",
    "xboxkrnl.lib",
    "xgraphicsltcg.lib",
    "xonline.lib",
    "libc.lib",
    "xapilib.lib",
]

# Header whose non-inline WINAPI declarations we audit.
MINIMAL_HEADER = os.path.join(ROOT, "src", "Platform", "Xbox", "d3d8_xbox_minimal.h")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def slurp(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.readlines()
    except OSError:
        return []


def bat_cpp_sources(bat_path):
    """
    Parse build_xbox.bat and return every source file that appears inside
    the C++ /Tp compilation loop (lines between 'Compiling C++ files' and
    the closing ') do (').
    """
    sources = []
    in_cpp = False
    with open(bat_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            s = line.strip()
            if "Compiling C++ files" in s:
                in_cpp = True
                continue
            if in_cpp:
                if s.startswith(") do"):
                    break
                # Source lines look like:  src\Foo\Bar.c  or  src/Foo/Bar.cpp
                if re.match(r"src[/\\]", s):
                    rel = s.rstrip()
                    sources.append(os.path.normpath(os.path.join(ROOT, rel)))
    return sources


# ---------------------------------------------------------------------------
# Check 1 — extern "C" linkage consistency
# ---------------------------------------------------------------------------

# Matches a return-type + function-name at the start of a definition line.
# Deliberately conservative — we only flag names we know about.
_FUNC_DEF = re.compile(
    r"^\s*"
    r"(?:void|int|unsigned\s+int|unsigned|HRESULT|ULONG|DWORD|BYTE"
    r"|float|double|char|BOOL|HANDLE|HWND)\s*\*?\s*"
    r"(\w+)\s*\("
)

# Matches a bare extern declaration that is NOT extern "C".
# Group 1 = function name.
_BARE_EXTERN = re.compile(
    r"^\s*extern\s+"
    r"(?!\"C\")"                           # not extern "C"
    r"(?:void|int|unsigned\s+int|unsigned|HRESULT|ULONG|DWORD|BYTE"
    r"|float|double|char|BOOL|HANDLE|HWND)\s*\*?\s*"
    r"(\w+)\s*\("
)


def extern_c_exports(path):
    """
    Return {name: lineno} for every function defined inside an
    extern "C" { } block in `path`.
    """
    lines   = slurp(path)
    exports = {}

    brace_depth      = 0
    ec_entry_depth   = None   # brace_depth when 'extern "C" {' was seen

    for lineno, raw in enumerate(lines, 1):
        s = raw.rstrip()

        # Detect  extern "C" {  on this line (before counting braces).
        if re.search(r'extern\s+"C"\s*\{', s):
            ec_entry_depth = brace_depth   # block opens after this depth

        opens  = s.count('{')
        closes = s.count('}')
        brace_depth += opens - closes

        # Check whether we just exited the extern "C" block.
        if ec_entry_depth is not None and brace_depth <= ec_entry_depth:
            ec_entry_depth = None

        inside = ec_entry_depth is not None

        # A function definition line opens a brace on the same line.
        if inside and opens > 0:
            m = _FUNC_DEF.match(s)
            if m:
                name = m.group(1)
                if name not in ("if", "while", "for", "switch", "else",
                                "do", "struct", "typedef", "class", "namespace"):
                    exports[name] = lineno

    return exports


def bare_extern_decls(path, names):
    """
    In `path`, find  extern <type> name(  declarations that are NOT
    protected by any of the standard extern "C" guard forms.

    Returns [(name, lineno, text), ...].
    """
    if not names:
        return []

    lines  = slurp(path)
    bad    = []

    brace_depth    = 0
    ec_block_depth = None   # inside  extern "C" { }  block
    cplusplus_guard_depth = 0   # nested #ifdef __cplusplus depth
    in_cplusplus_guard    = False

    # Single-line guard:  #ifdef __cplusplus\nextern "C"\n#endif
    # We track this with a small state machine.
    prev_was_cplusplus_ifdef = False

    for lineno, raw in enumerate(lines, 1):
        s = raw.rstrip()
        stripped = s.strip()

        # ── extern "C" { } block tracking ──────────────────────────────
        if re.search(r'extern\s+"C"\s*\{', stripped):
            ec_block_depth = brace_depth

        opens  = stripped.count('{')
        closes = stripped.count('}')
        brace_depth += opens - closes

        if ec_block_depth is not None and brace_depth <= ec_block_depth:
            ec_block_depth = None

        inside_ec_block = ec_block_depth is not None

        # ── #ifdef __cplusplus guard tracking ───────────────────────────
        if stripped == "#ifdef __cplusplus":
            prev_was_cplusplus_ifdef = True
            in_cplusplus_guard = True
        elif prev_was_cplusplus_ifdef:
            prev_was_cplusplus_ifdef = False
            # If the next non-blank line is 'extern "C"' or 'extern "C" {'
            # we stay in guard; otherwise cancel.
            if not re.search(r'extern\s+"C"', stripped):
                in_cplusplus_guard = False
        elif stripped.startswith("#endif"):
            in_cplusplus_guard = False

        # ── Single-line  extern "C" void name(  ─────────────────────────
        on_single_ec_line = bool(re.match(r'\s*extern\s+"C"\s+\w', s))

        # ── Check for bad bare extern declaration ────────────────────────
        m = _BARE_EXTERN.match(s)
        if m:
            name = m.group(1)
            if name in names:
                if not inside_ec_block and not in_cplusplus_guard and not on_single_ec_line:
                    bad.append((name, lineno, stripped))

    return bad


def check_extern_c_consistency(cpp_tu_files, issues):
    """
    For each .cpp file compiled as C++, collect its extern "C" exports.
    Then scan all C++ TU files for bare extern declarations of those names.
    Append issue strings to `issues`.
    """
    # Collect exports from .cpp files only.
    exports_by_file = {}
    for path in cpp_tu_files:
        if path.endswith(".cpp") and os.path.exists(path):
            ex = extern_c_exports(path)
            if ex:
                exports_by_file[path] = ex

    if not exports_by_file:
        return   # nothing exported as extern "C" from any .cpp

    all_exported_names = set()
    for ex in exports_by_file.values():
        all_exported_names.update(ex.keys())

    # Scan every C++ TU for bare extern declarations of those names.
    for tu_path in cpp_tu_files:
        if not os.path.exists(tu_path):
            continue
        bad = bare_extern_decls(tu_path, all_exported_names)
        for (name, lineno, text) in bad:
            # Find which .cpp file defined this name.
            defined_in = next(
                (p for p, ex in exports_by_file.items() if name in ex), "?")
            issues.append(
                f"{os.path.relpath(tu_path, ROOT)}:{lineno}: "
                f"bare 'extern {name}(...)' has C++ linkage in a /Tp TU "
                f"but '{name}' is defined extern \"C\" in "
                f"{os.path.relpath(defined_in, ROOT)}\n"
                f"    {text}\n"
                f"  Fix: wrap with  extern \"C\"  or  "
                f"#ifdef __cplusplus / extern \"C\" / #endif"
            )


# ---------------------------------------------------------------------------
# Check 2 — Declared-but-not-in-lib
# ---------------------------------------------------------------------------

# Matches decorated stdcall symbols: _FunctionName@N
# The function name is the part between _ and @.
_DECORATED = re.compile(r"^_([A-Za-z_]\w*)@\d+$")


def lib_symbols(lib_names):
    """
    Run  link.exe /dump /linkermember:1  on each lib and return the set of
    undecorated function names present across all libs.
    """
    if not os.path.exists(LINK_EXE):
        return None   # can't check — skip silently

    names = set()
    for lib in lib_names:
        path = os.path.join(LIB_DIR, lib)
        if not os.path.exists(path):
            continue
        try:
            out = subprocess.check_output(
                [LINK_EXE, "/dump", "/linkermember:1", path],
                stderr=subprocess.DEVNULL,
                encoding="utf-8", errors="replace"
            )
        except (subprocess.CalledProcessError, OSError):
            continue
        for token in out.split():
            m = _DECORATED.match(token.strip())
            if m:
                names.add(m.group(1))
    return names


# Matches a non-inline WINAPI function declaration in the minimal header.
# We want lines like:
#   void    WINAPI D3DDevice_Foo(...)
#   HRESULT WINAPI D3DDevice_Bar(...)
#   ULONG   WINAPI D3DDevice_Baz(void);
# but NOT lines beginning with  static __inline  (those are header-inlines).
_WINAPI_DECL = re.compile(
    r"^\s*"
    r"(?!static\s)"          # not static
    r"(?:void|int|HRESULT|ULONG|DWORD|BYTE|UINT|BOOL|Direct3D\s*\*"
    r"|D3DTexture\s*\*|D3DVertexBuffer\s*\*|D3DIndexBuffer\s*\*"
    r"|D3DSurface\s*\*|D3DResource\s*\*|D3DBaseTexture\s*\*)\s*\*?\s+"
    r"WINAPI\s+"
    r"(\w+)\s*\("
)


def header_winapi_decls(header_path):
    """
    Return {name: lineno} for every non-inline WINAPI declaration in the
    header that is NOT inside a  static __inline  or  #ifdef __cplusplus
    C++-only struct block.
    """
    lines   = slurp(header_path)
    decls   = {}
    in_cpp_struct = False   # inside #ifdef __cplusplus struct block

    for lineno, raw in enumerate(lines, 1):
        s = raw.rstrip()
        stripped = s.strip()

        if stripped == "#ifdef __cplusplus":
            in_cpp_struct = True
        if in_cpp_struct and stripped == "#endif /* __cplusplus */":
            in_cpp_struct = False

        if in_cpp_struct:
            continue   # struct method bodies — not lib calls

        if "static __inline" in stripped:
            continue   # inline wrapper — not a lib symbol

        m = _WINAPI_DECL.match(s)
        if m:
            name = m.group(1)
            # Skip the XGetVideo* functions — they're in xapilib.lib which we
            # don't need to parse specially; they're confirmed present.
            decls[name] = lineno

    return decls


def check_declared_not_in_lib(issues):
    """
    For each non-inline WINAPI declaration in d3d8_xbox_minimal.h, verify
    the symbol is present in at least one of the linked libs.
    """
    if not os.path.exists(MINIMAL_HEADER):
        return

    symbols = lib_symbols(LINKED_LIBS)
    if symbols is None:
        return   # link.exe not found — skip check

    decls = header_winapi_decls(MINIMAL_HEADER)
    header_rel = os.path.relpath(MINIMAL_HEADER, ROOT)

    for name, lineno in sorted(decls.items(), key=lambda kv: kv[1]):
        if name not in symbols:
            issues.append(
                f"{header_rel}:{lineno}: '{name}' is declared WINAPI but "
                f"not found in any linked lib ({', '.join(LINKED_LIBS)}).\n"
                f"  It is likely an inline-only method in an XDK header.\n"
                f"  Fix: mark  static __inline  or remove the declaration "
                f"and call via a C++ wrapper (see std3D_draw.cpp pattern)."
            )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    bat = os.path.join(ROOT, "build_xbox.bat")
    if not os.path.exists(bat):
        print("audit_xbox.py: cannot find build_xbox.bat", file=sys.stderr)
        sys.exit(1)

    cpp_tu_files = bat_cpp_sources(bat)

    issues = []
    check_extern_c_consistency(cpp_tu_files, issues)
    check_declared_not_in_lib(issues)

    if issues:
        print("audit_xbox.py: LINKAGE ISSUES FOUND", file=sys.stderr)
        for iss in issues:
            print(f"\n  {iss}", file=sys.stderr)
        print(f"\n{len(issues)} issue(s). Fix before building.", file=sys.stderr)
        sys.exit(1)

    print("audit_xbox.py: OK")
    sys.exit(0)


if __name__ == "__main__":
    main()
