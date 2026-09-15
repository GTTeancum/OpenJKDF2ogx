"""Host calibration bounds and invalid/reset samples, without an emulator."""
from pathlib import Path
import importlib.util
root=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location("ram",root/"scripts/xbox/poll_xemu_ram_log.py")
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
a={"host_before":10.,"host_after":11.,"words":[2,100,1000,500,50,2]}
b={"host_before":40.,"host_after":41.,"words":[4,15100,30001000,15500,1850,4]}
r=m.compare_clock_probes(a,b)
assert r["valid"] and r["host_seconds_min"]==29 and r["host_seconds_max"]==31
assert r["tick_ms"]==15000 and r["counter_us"]==30000000 and r["game_ms"]==15000
assert r["host_fps_min"]==1800/31 and r["host_fps_max"]==1800/29
assert not m.compare_clock_probes(None,b)["valid"]
assert not m.compare_clock_probes(a,a)["valid"]
b["words"][3]=0
assert not m.compare_clock_probes(a,b)["valid"]
a["words"][1]=0xfffffff0;b["words"][1]=16;b["words"][3]=15500
assert m.compare_clock_probes(a,b)["tick_ms"]==32
# Simulate torn publication; read_clock_probe must reject it.
m.read_words=lambda *args:[3,0,0,0,1,2]
assert m.read_clock_probe(None,123,None) is None
m.read_words=lambda *args:[4,100,1000,500,1,4]
assert m.read_clock_probe(None,123,None)["words"][3]==500
print("PASS: host-time bounds, clock wrap, reset and torn-sample rejection")
