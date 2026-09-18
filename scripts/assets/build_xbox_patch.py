"""Package tracked Xbox gameplay overrides without modifying stock archives."""
import argparse
from pathlib import Path
from mots_compat_pack import read_gob_entries, write_gob


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        default=root / 'build/xbox/release/mods/xbox_patch.gob')
    args = parser.parse_args()
    source = root / 'assets/xbox-patch'
    records = [(str(p.relative_to(source)).replace('/', '\\'), p.read_bytes())
               for p in sorted((source / 'cog').rglob('*.cog'))]
    if not records:
        raise RuntimeError('Xbox patch has no COG sources')
    write_gob(args.output, records)
    if read_gob_entries(args.output) != records:
        raise RuntimeError('Xbox patch round-trip verification failed')
    print(f'Built and verified {args.output}: {len(records)} entries')


if __name__ == '__main__':
    main()
