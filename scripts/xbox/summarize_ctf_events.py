"""Summarize deduplicated stock CTF callback events from XEMU RAM polls."""
import argparse, collections, json, re
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('run_dir',type=Path);args=parser.parse_args()
pattern=re.compile(r'BotCTFEvent: event=(\d+) playerThing=(-?\d+) redScore=(\d+) goldScore=(\d+) timeMs=(\d+)')
seen=set();events=[]
for path in sorted(args.run_dir.glob('ram_poll*.txt')):
 for match in pattern.finditer(path.read_text(errors='replace')):
  row=tuple(map(int,match.groups()))
  if row not in seen: events.append(row);seen.add(row)
events.sort(key=lambda row:row[4])
labels={10:'red_taken',11:'red_returned',12:'red_capture',13:'red_dropped',14:'red_auto_return',15:'red_retaken',20:'gold_taken',21:'gold_returned',22:'gold_capture',23:'gold_dropped',24:'gold_auto_return',25:'gold_retaken'}
counts=collections.Counter(labels.get(row[0],str(row[0])) for row in events)
prior=(0,0);increases=[];warnings=[]
for event,player,red,gold,when in events:
 if event in (12,22):
  team=0 if event==12 else 1;scores=(red,gold)
  increases.append({'team':'red' if team==0 else 'gold','reportedScore':scores[team],'previousLoggedScore':prior[team],'timeMs':when,'playerThing':player})
 prior=(red,gold)
print(json.dumps({'events':len(events),'counts':dict(counts),'lastScores':prior,'captureResults':increases,'warnings':warnings,'hasFlagReturn':any(row[0] in (11,14,21,24) for row in events)},indent=2))
