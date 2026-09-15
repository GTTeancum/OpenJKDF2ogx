"""Guard against linking the Xbox no-op JK save/packet implementation."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[3]
link_map = (root / "build/xbox/release/openjkdf2_xbox.exe.map").read_text()
for symbol in (
    "jkDSS_Startup", "jkDSS_SendSetSaberInfo2",
    "jkDSS_playerconfig_idksync", "jkDSS_player_thingsidkfunc",
    "jkDSS_Write", "jkDSS_Load", "jkDSS_Processx32",
):
    matches = [line for line in link_map.splitlines()
               if re.search(r"\?" + symbol + r"@@", line)]
    assert len(matches) == 1, (symbol, matches)
    assert matches[0].rstrip().endswith("jkDSS.obj"), (symbol, matches[0])
print("PASS: Xbox links the real JK save callbacks and POV packet handler")
