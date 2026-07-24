---
title: OCR Guide
---

# OCR guide

This guide explains how to set up and use native Heist OCR.

### OCR Setup ###

The native host uses OpenCV and Tesseract with the proven upstream OCR assets.
The repository installer downloads the pinned upstream archive, verifies its
SHA-256 checksum, and installs only the required templates and language data:

```bash
git clone --depth 1 https://github.com/JuanchiBruzzone/awakened-poe-trade-native.git
cd awakened-poe-trade-native
./install-ocr-data.sh
```

The resulting configuration structure is:

```text
apt-data/
├── config.json
└── cv-ocr/
   ├── eng.traineddata
   ├── rus.traineddata
   └── ... lock templates
```

Restart the application after installation.

### Widget configuration ###

1. Open the widget by clicking near the Settings button.
   ![](https://i.imgur.com/Y0RJune.png)

   I prefer to place it at the bottom.
   ![](https://i.imgur.com/bkNDKYg.png)

2. Edit the widget and assign a hotkey.
   ![](https://i.imgur.com/GeOMcal.png)

### Rules to follow before pressing the hotkey ###

1. Both icons should be fully visible.
   ![](https://i.imgur.com/Mu6B6it.png)

2. The text should not be occluded by health bar or other elements.
   ![](https://i.imgur.com/cM2i3Rk.png)

Happy hunting!
