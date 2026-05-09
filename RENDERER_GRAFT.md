# Pattern: Grafting a Third-Party Renderer

A methodology for porting a game engine to a platform where you don't have
a working renderer, by adopting a known-working third-party renderer
wholesale rather than writing your own.

This doc was extracted from the OpenJKDF2 → original Xbox port, where
several rounds of hand-rolled D3D8 attempts wedged the NV2A, and the
breakthrough came from grafting Microsoft's xquake (FakeGL) instead.
The pattern generalizes.

---

## When to use this

You have:

- An engine with its renderer behind a stable internal API.
- A target platform where your hand-rolled renderer either doesn't work,
  is unreliable, or is taking too long to debug.
- Access to a **third-party renderer that's confirmed working on that
  exact platform** — ideally one whose author had access to documentation,
  hardware, or vendor support you don't.
- License terms compatible with grafting (or you're working privately
  and fine with that).

Don't use this when:

- Your hand-rolled renderer almost works and the missing piece is
  identified.  Finish it.
- The third-party renderer's architecture is wildly incompatible with
  your engine's draw flow (e.g. retained-mode vs immediate-mode, fixed
  set of primitive types vs full GL).
- License terms forbid it for your use case.

---

## Pre-flight

1. **Confirm the third-party renderer actually works on your target.**
   Build it, run it, see something on screen.  If it doesn't run for
   *them*, it won't run for you either.

2. **Read its public interface end-to-end.**  Identify:
   - Initialization entry points
   - Per-frame entry points (begin/draw/end/swap)
   - Resource management (textures, buffers)
   - Teardown
   - Anything you'll need to call from your engine's renderer code

3. **Identify everything it depends on you to provide.**  Most renderers
   pulled out of a host engine have externs:
   - Logging/console functions
   - Globals the host set
   - Allocator hooks
   - Etc.
   Make a list.  These become your stubs.

4. **Identify build prerequisites.**  Compile flags (e.g. `_XBOX`),
   additional libraries (`d3dx8.lib`), headers it needs.

---

## The graft

### 1. Copy verbatim

Drop the third-party source into your tree **unmodified**.  Do not edit
it.  Every modification is a future merge headache and a risk that you
introduce the same kind of bug your hand-rolled code had.

If the renderer needs companion files (e.g. the platform's `gl/gl.h`
shim), copy those too.  Verbatim.

### 2. Provide its externs as stubs

Create a small adapter file (one C/C++ file) defining each symbol the
renderer expects from the host engine.  Route them somewhere safe:

- Logging → your platform's debug log.
- Globals → defaults that don't break the renderer's init path.
- Allocators → `malloc`/`free` or your platform allocator.

Keep this file small.  It's plumbing.

### 3. Add to build

- Add the renderer source files to your build script.
- Define any required platform macros (e.g. `_XBOX`).  Forgetting these
  silently selects the wrong code path and wastes hours.
- Link required libraries.
- Verify it compiles cleanly **before doing any integration work**.  A
  dead-link compile of the third-party file is the first checkpoint.

### 4. Replace your renderer with a thin adapter

This is the structural change.  Your engine has a renderer module
(`std3D` in our case, but every engine has one — `Renderer`, `r_*`,
`gfx_*`, etc.).  Replace its body with code that translates each of
your engine's renderer API calls into the third-party renderer's API.

**Do not do partial integration.**  If you wire init through the
third-party but keep doing your own draws, you have two renderers
fighting over device state.  Symptoms include silent corruption, wedges,
and inconsistent behaviour — all the same kinds of problems the graft
was supposed to eliminate.

The adapter:

- Owns no GPU resources.
- Holds no platform-specific state.
- Calls only third-party-renderer entry points + a tiny bit of bookkeeping
  for your engine's contract (frame-counter, vertex staging arrays,
  whatever your API requires).

If you find yourself reaching for the platform's native API in the
adapter (e.g. a `D3DDevice_*` call), stop.  Either the third-party
renderer exposes what you need (use that), or it doesn't and you have
a real architectural decision to make.  Don't quietly insert direct
platform calls.

### 5. Diagnostic-first

Before touching the engine's actual draw path, prove the renderer works
end-to-end with a hardcoded shape:

- In your adapter's `StartScene` (or equivalent), after the third-party's
  clear, emit a single triangle directly via the third-party API.
- Run.
- See a triangle.

This isolates "is the renderer functional" from "is the engine submitting
geometry correctly".  Without it, when nothing's on screen you have two
unknowns; with it, you have one.

Once the diagnostic triangle is visible, remove it and wire the real
engine geometry into the adapter.

---

## Lessons learned (concrete pitfalls)

These are specific to the OpenJKDF2 → Xbox port but each illustrates a
class of problem.

### Z range with right-handed projection

xquake's `glOrtho(0, vid.width, vid.height, 0, -99999, 99999)` looked
arbitrary.  It isn't.  Under `D3DXMatrixOrthoOffCenterRH`'s formula,
narrow near/far ranges (`(0, 1)`) map positive view-z values to negative
NDC-z — outside D3D's `[0, 1]` clip range — and the geometry gets
front-clipped, invisibly.  Wide ranges keep any reasonable z inside the
clip cube.

**Lesson:** when copying numerically "weird" parameters from a working
reference, copy them.  They are usually load-bearing.

### Platform macros silently change the code path

`fakeglx.cpp` has `#ifdef _XBOX` blocks selecting different `D3DPRESENT_PARAMETERS`,
different swap effects, different surface formats.  Without `/D_XBOX=1`
in our build flags, it took the PC path and produced nothing usable.

**Lesson:** every `#ifdef <PLATFORM>` in the third-party source is a
required define.  Audit them before assuming any build is meaningful.

### "Initial commit" with mixed history

(Not strictly a renderer issue, but bit us in cleanup.)  After grafting,
our local repo had hundreds of debug-cycle commits.  When we created the
public GitHub repo we squashed to a single root commit so the public
history started at the working state.  Decide early whether you want
debug history visible.

### Don't modify the third-party source

When the renderer was missing C-callable wrappers for a function we
wanted (`glBindTexture` was only behind `wglGetProcAddress`), the
temptation was to add `extern "C"` wrappers inside the third-party file.
Resisted.  Either go through the documented entry point, or add wrappers
in *our* adapter file.  The third-party source must stay byte-identical
to its origin so it remains a known-good import, not a fork-with-our-
changes-mixed-in.

### Beware undefined-behaviour traps in the third-party path

D3D8's spec lists "undefined behaviour" cases — e.g. `D3DTOP_DISABLE` on
COLOROP without a matching ALPHAOP.  A third-party renderer that's
"working" may simply never hit those paths in its host engine's usage
pattern.  Your engine might.  When the diagnostic triangle worked but
the engine's textured draws didn't, this was the kind of bug to look
for — *not* a bug in the renderer, but a usage path the renderer wasn't
exercised on.

---

## Concrete example: OpenJKDF2 → original Xbox

| Step | Artifact |
|---|---|
| Third-party renderer | xquake's `gl_fakegl.cpp` + `gl/gl.h` from Microsoft's OG Xbox SDK source tree |
| Externs needed | `Con_Printf` (log), `DIBWidth`/`DIBHeight` (globals, set to 0 — Xbox path doesn't read them) |
| Platform macro | `/D_XBOX=1` |
| Extra library | `d3dx8.lib` (matrix stack) |
| Adapter file | `src/Platform/Xbox/std3D.c` — exposes OpenJKDF2's `std3D_*` API, body translates each call to `gl*` / `wgl*` / `FakeSwapBuffers` |
| Stubs file | `src/Platform/Xbox/quake_stubs.c` (27 lines) |
| Diagnostic | RGB triangle hardcoded in `std3D_StartScene` after `glClear`, visible on retail Xbox + CXBX-R |

End state: one renderer (xquake's, unchanged); one adapter (~400 lines);
zero direct D3D8 calls in our code; first pixel on hardware.
