#!/usr/bin/env python3
"""Small Windows GUI for the OpenJKDF2x cutscene converter."""

from __future__ import annotations

import queue
import sys
import threading
from pathlib import Path
from types import SimpleNamespace
from tkinter import BooleanVar, Button, Checkbutton, Label, Tk, messagebox
from tkinter.scrolledtext import ScrolledText

import openjkdf2x_cutscene_packager as packager


def app_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent


def detect_game_root() -> Path | None:
    bases = [app_dir(), Path.cwd().resolve()]
    for base in list(bases):
        bases.append(base.parent)

    seen: set[Path] = set()
    for base in bases:
        try:
            candidate = base.resolve()
        except OSError:
            continue
        if candidate in seen:
            continue
        seen.add(candidate)
        if (candidate / "default.xbe").is_file():
            return candidate
    return None


class ConverterGui:
    def __init__(self) -> None:
        self.root = Tk()
        self.root.title("OpenJKDF2x Cutscene Converter")
        self.root.geometry("760x560")
        self.root.minsize(680, 460)

        self.game_root = detect_game_root()
        self.log_queue: queue.Queue[tuple[str, str | int]] = queue.Queue()
        self.worker: threading.Thread | None = None

        self.delete_originals = BooleanVar(value=False)
        self.force = BooleanVar(value=False)

        self.title = Label(self.root, text="OpenJKDF2x Cutscene Converter", font=("Segoe UI", 15, "bold"))
        self.title.pack(anchor="w", padx=14, pady=(12, 4))

        self.root_label = Label(self.root, justify="left", anchor="w")
        self.root_label.pack(fill="x", padx=14, pady=(0, 8))

        self.count_label = Label(self.root, justify="left", anchor="w")
        self.count_label.pack(fill="x", padx=14, pady=(0, 8))

        self.delete_check = Checkbutton(
            self.root,
            text="Delete original .SMK/.SAN videos after each matching .XMV passes validation",
            variable=self.delete_originals,
            anchor="w",
        )
        self.delete_check.pack(fill="x", padx=14)

        self.force_check = Checkbutton(
            self.root,
            text="Rebuild existing .XMV files",
            variable=self.force,
            anchor="w",
        )
        self.force_check.pack(fill="x", padx=14, pady=(0, 8))

        self.convert_button = Button(self.root, text="Convert Videos", command=self.start_conversion)
        self.convert_button.pack(anchor="w", padx=14, pady=(0, 10))

        self.log = ScrolledText(self.root, height=20, state="disabled", wrap="word", font=("Consolas", 9))
        self.log.pack(fill="both", expand=True, padx=14, pady=(0, 14))

        self.refresh_state()
        self.root.after(100, self.drain_log_queue)

    def refresh_state(self) -> None:
        if not self.game_root:
            self.root_label.config(
                text="Game folder not detected. Put this EXE beside default.xbe, or inside a cutscene_converter folder directly under it."
            )
            self.count_label.config(text="")
            self.convert_button.config(state="disabled")
            return

        sources = packager.find_video_sources(self.game_root)
        xmv_count = sum(1 for path in self.game_root.rglob("*") if path.is_file() and path.suffix.lower() == ".xmv")
        self.root_label.config(text=f"Detected game folder: {self.game_root}")
        self.count_label.config(text=f"Videos found: {len(sources)} .SMK/.SAN    Existing XMV files: {xmv_count}")
        self.convert_button.config(state="normal" if sources else "disabled")

    def append_log(self, text: str) -> None:
        self.log.config(state="normal")
        self.log.insert("end", text + "\n")
        self.log.see("end")
        self.log.config(state="disabled")

    def start_conversion(self) -> None:
        if not self.game_root:
            messagebox.showerror(
                "Game folder not detected",
                "Put this EXE beside default.xbe, or inside a cutscene_converter folder directly under it.",
            )
            return

        if self.delete_originals.get():
            ok = messagebox.askyesno(
                "Delete originals after conversion?",
                "Original .SMK/.SAN files will be deleted only after their matching .XMV files pass validation.\n\nContinue?",
            )
            if not ok:
                return

        self.convert_button.config(state="disabled")
        self.delete_check.config(state="disabled")
        self.force_check.config(state="disabled")
        self.append_log("Starting conversion...")

        self.worker = threading.Thread(target=self.run_conversion, daemon=True)
        self.worker.start()

    def run_conversion(self) -> None:
        old_log = packager.log

        def gui_log(message: str) -> None:
            self.log_queue.put(("log", message))

        packager.log = gui_log
        try:
            roots = [app_dir(), self.game_root, Path.cwd()]
            ffmpeg = packager.find_executable("ffmpeg.exe", roots) or packager.find_executable("ffmpeg", roots)
            ffprobe = packager.find_executable("ffprobe.exe", roots) or packager.find_executable("ffprobe", roots)
            if not ffmpeg or not ffprobe:
                raise RuntimeError("ffmpeg/ffprobe were not found beside the converter, bundled in the EXE, or on PATH.")

            args = SimpleNamespace(
                game_root=self.game_root,
                force=self.force.get(),
                delete_originals=self.delete_originals.get(),
                dry_run=False,
                width=packager.DEFAULT_WIDTH,
                height=packager.DEFAULT_HEIGHT,
                fps=packager.DEFAULT_FPS,
                video_bitrate=packager.DEFAULT_VIDEO_BITRATE,
            )

            sources = packager.find_video_sources(self.game_root)
            gui_log("OpenJKDF2x cutscene packager")
            gui_log(f"root: {self.game_root}")
            gui_log(f"ffmpeg: {ffmpeg}")
            gui_log(f"ffprobe: {ffprobe}")
            gui_log(f"videos found: {len(sources)}")
            if args.delete_originals:
                gui_log("delete originals: yes, after each generated XMV validates")

            converted = 0
            failed = 0
            for source in sources:
                try:
                    if packager.convert_one(args, ffmpeg, ffprobe, source):
                        converted += 1
                except Exception as exc:
                    failed += 1
                    gui_log(f"  FAILED: {source.relative_to(self.game_root)}: {exc}")

            gui_log(f"done: converted={converted} failed={failed}")
            self.log_queue.put(("done", failed))
        except Exception as exc:
            self.log_queue.put(("log", f"FAILED: {exc}"))
            self.log_queue.put(("done", 1))
        finally:
            packager.log = old_log

    def drain_log_queue(self) -> None:
        try:
            while True:
                kind, value = self.log_queue.get_nowait()
                if kind == "log":
                    self.append_log(str(value))
                elif kind == "done":
                    self.convert_button.config(state="normal" if self.game_root else "disabled")
                    self.delete_check.config(state="normal")
                    self.force_check.config(state="normal")
                    self.refresh_state()
                    if int(value) == 0:
                        messagebox.showinfo("Conversion complete", "Cutscene conversion completed successfully.")
                    else:
                        messagebox.showerror("Conversion failed", "One or more videos failed. Check the log for details.")
        except queue.Empty:
            pass
        self.root.after(100, self.drain_log_queue)

    def run(self) -> None:
        self.root.mainloop()


def main() -> int:
    if "--self-test" in sys.argv:
        root = detect_game_root()
        print(root or "NOT_FOUND")
        return 0 if root else 2

    ConverterGui().run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
