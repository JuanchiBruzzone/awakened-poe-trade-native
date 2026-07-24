# Upstream synchronization

The renderer, parser, trade logic, data, translations, and shared IPC types stay
owned by `SnosMe/awakened-poe-trade`. Native Linux changes stay under `native/`
plus native build, packaging, and validation scripts.

Run `./sync-upstream.sh --check` to fetch upstream and preview new commits. It
also lists changes under `main/src` and `ipc`, which are the Electron behavior
and shared protocol surfaces the C++ host must continue to match.

The scheduled **Upstream monitor** GitHub workflow performs a read-only check
every week. When upstream has commits not present in the native branch, it
opens or updates one labeled issue containing the commit list and affected
renderer/backend files. It never merges or executes upstream code. A maintainer
reviews that issue and performs the integration with this documented process.

From a clean integration branch, run `./sync-upstream.sh --merge`. Renderer-only
features merge normally. If upstream backend or IPC files changed, the native
contract check fails until those changes are reviewed, implemented in C++, and
the reviewed tree digests in `native/compat/upstream-contract.json` are updated.

CI should run:

```bash
python scripts/check-upstream-contract.py
```

Release candidates additionally run:

```bash
python scripts/check-upstream-contract.py --require-complete
```

The stricter command intentionally remains red while the capability table names
unfinished parity work. This prevents a successful compile from being mistaken
for complete backend compatibility.

Native release tags use the upstream version followed by a native revision
(for example, `v3.28.104-native.1`). The workflow compiles the tag into the
binary, attaches the versioned native AppImage, and GitHub records the digest
consumed by the in-app updater.
