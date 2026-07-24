## Summary

Describe the focused change and why it belongs in the native port.

## Upstream relationship

Link the relevant upstream issue, pull request, or commit when applicable.

## Testing performed

- [ ] `python scripts/check-upstream-contract.py --require-complete`
- [ ] `npm run lint` in `renderer/`
- [ ] `npm run build` in `renderer/`
- [ ] Clean native Release build
- [ ] Plasma Wayland in-game test, if input/overlay/focus behavior changed

List exact test results and environment details:

## Safety and privacy

- [ ] No credentials, cookies, personal paths, game logs, item contents, or build artifacts are committed.
- [ ] AI-generated code, if any, was reviewed by the contributor.
- [ ] Existing PoE focus, taskbar, quick/locked check, and command behavior was rechecked where relevant.

## Screenshots or logs

Include only sanitized evidence when it materially helps review.
