---
title: Download
---

<script setup>
import { useData } from 'vitepress'

const { theme } = useData()
</script>

# Download

Download native releases only from this repository's GitHub Releases page.
Third-party mirrors are not maintained or verified by this project.

| Download | Platform | Updates |
|---|---|---|
| <a :href="`${theme.github.releasesUrl}/download/v${theme.appVersion}/Awakened-PoE-Trade-Native-${theme.appVersion}-x86_64.AppImage`">Native x86-64 AppImage</a> | KDE Plasma 6 Wayland | Automatic when the AppImage is writable |
| <a :href="`${theme.github.releasesUrl}/download/v${theme.appVersion}/Awakened-PoE-Trade-Native-${theme.appVersion}-x86_64.AppImage.sha256`">SHA-256 checksum</a> | Release verification | Manual |
| <a :href="`${theme.github.releasesUrl}/download/v${theme.appVersion}/Awakened-PoE-Trade-Native-${theme.appVersion}-x86_64.spdx.json`">SPDX SBOM</a> | Release transparency | Manual |

Latest version is <span class="bg-gray-100 border rounded px-1">{{ theme.appVersion }}</span>

```bash
chmod +x Awakened-PoE-Trade-Native-*.AppImage
sha256sum -c Awakened-PoE-Trade-Native-*.AppImage.sha256
./Awakened-PoE-Trade-Native-*.AppImage
```

---

### Requirements

- x86-64 Linux
- KDE Plasma 6 running under Wayland
- Path of Exile 1 running as a native Wayland client
- `ydotool` with its daemon running
- `wl-clipboard`

The AppImage checks and logs these host integrations at launch. Read
[Linux dependencies and setup](./linux-dependencies) before running it.

Windows, macOS, X11, XWayland PoE windows, and cloud-gaming sessions are not
supported by this native distribution. Use
[upstream Awakened PoE Trade](https://github.com/SnosMe/awakened-poe-trade)
for its supported platforms.
