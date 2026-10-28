"""Stage the Xbox v1.0 download from an explicit allowlist (no test install)."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.local/release-v1.0/package'
NAME = 'OpenJKDF2x-Xbox-v1.0'
STAGE = OUT / NAME


def main():
    if STAGE.exists():
        raise SystemExit(f'Refusing to overwrite existing stage: {STAGE}')
    sources = {}

    def add(src, dst):
        src = ROOT / src
        if not src.is_file():
            raise FileNotFoundError(src)
        sources[dst] = src

    for name in ('default.xbe', 'TitleImage.xbx', 'SaveImage.xbx', 'TitleMeta.xbx'):
        add(f'build/xbox/release/{name}', name)
    for name in ('box art.png', 'README.md', 'LICENSE.md'):
        add(name, name)
    add('docs/RELEASE_NOTES_v1.0.md', 'UPDATE_NOTES.md')
    for name in ('OpenJKDF2xCutsceneConverter.exe', 'CUTSCENE_CONVERSION_README.txt'):
        add(f'tools/xbox/cutscene_converter/dist/{name}', name)
    for src in sorted((ROOT / 'resource/ui/bm').glob('*.bm')):
        if src.name.startswith('xbtn_') or (
                src.name.startswith('xw_') and not src.name.startswith('xw_mots_')):
            add(src, f'Resource/ui/bm/{src.name}')
    # Maintained runtime components, not arbitrary user-installed mods.
    add('build/xbox/release/mods/xbox_patch.gob', 'mods/xbox_patch.gob')
    add('assets/mp-hud/jkhud.txt', 'jkhud.txt')
    for src in sorted((ROOT / 'assets/mp-hud/Resource').rglob('*')):
        if src.is_file():
            add(src, src.relative_to(ROOT / 'assets/mp-hud').as_posix())

    manifest = {}
    for dst, src in sources.items():
        target = STAGE / dst
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, target)
        manifest[dst] = hashlib.sha256(target.read_bytes()).hexdigest()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    archive = OUT / (NAME + '.zip')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for dst in sorted(sources):
            z.write(STAGE / dst, NAME + '/' + dst)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert set(z.namelist()) == {NAME + '/' + dst for dst in sources}
        for dst, digest in manifest.items():
            assert hashlib.sha256(z.read(NAME + '/' + dst)).hexdigest() == digest
        forbidden = {'.goo', '.jkl', '.mat', '.snd', '.smk', '.san', '.xmv',
                     '.jkp', '.jks', '.bnav', '.log', '.map', '.obj', '.psd'}
        assert all(Path(dst).suffix.lower() not in forbidden for dst in sources)
        assert [dst for dst in sources if dst.lower().endswith('.gob')] == ['mods/xbox_patch.gob']
        assert not any('xbox_smoke' in dst or dst.startswith(('player/', 'Episode/', 'MUSIC/')) for dst in sources)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (OUT / 'SHA256SUMS.txt').write_text(f'{digest}  {archive.name}\n')
    print(f'Verified {len(sources)} files: {archive}\nSHA256 {digest}')


if __name__ == '__main__':
    main()
