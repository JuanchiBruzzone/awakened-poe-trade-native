import { defineConfig } from 'vitepress'

const BASE = '/awakened-poe-trade-native/'

export default defineConfig({
  title: 'Awakened PoE Trade Native',
  description: 'Native KDE Plasma Wayland host for Awakened PoE Trade',
  base: BASE,
  mpa: true,
  head: [
    ['link', { rel: 'shortcut icon', type: 'image/png', href: `${BASE}favicon.png` }],
    ['meta', { name: 'theme-color', content: '#1d99f3' }]
  ],
  markdown: {
    theme: 'light-plus',
    attrs: {
      leftDelimiter: '{:',
      rightDelimiter: '}'
    }
  },
  themeConfig: {
    appVersion: '3.28.104-native.2',
    github: {
      releasesUrl: 'https://github.com/JuanchiBruzzone/awakened-poe-trade-native/releases'
    },
    socialLinks: [
      {
        text: 'Native GitHub',
        color: '#1d99f3',
        link: 'https://github.com/JuanchiBruzzone/awakened-poe-trade-native'
      },
      {
        text: 'Upstream',
        color: '#6f42c1',
        link: 'https://github.com/SnosMe/awakened-poe-trade'
      }
    ],
    sidebar: [
      {
        text: 'Get started',
        items: [{
          text: 'Download',
          link: '/download'
        }, {
          text: 'Quick start',
          link: '/quick-start'
        }, {
          text: 'Linux setup',
          link: '/linux-dependencies'
        }]
      },
      {
        text: 'Use and repair',
        items: [{
          text: 'Troubleshooting',
          link: '/troubleshooting'
        }, {
          text: 'OCR guide',
          link: '/ocr-guide'
        }, {
          text: 'Chat commands',
          link: '/chat-commands'
        }]
      },
      {
        text: 'Project',
        items: [{
          text: 'Native architecture',
          link: '/native-architecture'
        }, {
          text: 'Upstream sync',
          link: '/upstream-sync'
        }, {
          text: 'Contributing',
          link: 'https://github.com/JuanchiBruzzone/awakened-poe-trade-native/blob/master/CONTRIBUTING.md'
        }]
      }
    ]
  }
})
