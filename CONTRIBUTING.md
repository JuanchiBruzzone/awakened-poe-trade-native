# Contributing

Thank you for improving Awakened PoE Trade Native.

This project is maintained with AI assistance because the repository owner
does not have time to develop and maintain it alone. AI-generated patches are
welcome, but the contributor remains responsible for reviewing the code,
protecting user data, and reporting the tests actually performed.

## Before opening an issue

- Search existing issues and upstream Awakened PoE Trade issues.
- Reproduce with the latest native release.
- Separate native Linux integration problems from upstream renderer, parser,
  translation, and trade-logic problems.
- Remove account names, cookies, tokens, whispers, item text, and personal
  paths from logs.

## Pull requests

1. Fork the repository and create a focused branch.
2. Keep native backend work under `native/` when the shared protocol does not
   require a renderer or IPC change.
3. Preserve upstream application behavior unless the change is explicitly a
   reviewed native improvement.
4. Do not commit build directories, generated AppImages, credentials, local
   configuration, game logs, or OCR captures.
5. Update documentation and the compatibility table when behavior changes.
6. Add a clear test report to the pull request.

Required local checks:

```bash
python scripts/check-upstream-contract.py --require-complete
cd renderer
npm ci
npm run lint
npm run build
cd ..
./build-native.sh
ctest --test-dir native/build --output-on-failure
appstreamcli validate --no-net native/resources/awakened-poe-trade-native.appdata.xml
```

Changes involving overlays, shortcuts, clipboard behavior, input injection, or
focus must also be tested in Path of Exile on Plasma Wayland. Confirm:

- PoE keeps keyboard focus.
- Plasma panels do not appear over the game.
- Quick and locked price checks retain their distinct behavior.
- Chat commands never paste stale item text.
- Shift+Space and configured shortcuts still work.

## Upstream synchronization

Use `./sync-upstream.sh --check` to inspect new upstream commits. Review changes
under `main/src` and `ipc` carefully. After porting relevant behavior, update
the reviewed digest in `native/compat/upstream-contract.json` and explain the
review in the pull request.

## Releases

`native/VERSION` is the release source of truth. Use:

```text
UPSTREAM_VERSION-native.NATIVE_REVISION
```

Example: `3.28.104-native.2`.

Only bump it after the release commit is ready. Merging that change to
`master` runs the release workflow, creates `v<VERSION>`, verifies the
AppImage, and publishes the AppImage and checksum. Existing tags are immutable.
The release also publishes an SPDX SBOM and repository-bound GitHub
attestations.
