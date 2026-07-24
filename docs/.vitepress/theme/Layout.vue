<script setup>
import { useData, withBase, useRoute } from 'vitepress'

const { site, theme } = useData()
const route = useRoute()

const isExternal = (link) => /^https?:\/\//.test(link)
const itemHref = (link) => isExternal(link) ? link : withBase(link)
const isActive = (link) => {
  if (isExternal(link)) return false
  const current = route.path.replace(/\.html$/, '').replace(/\/$/, '')
  const target = withBase(link).replace(/\.html$/, '').replace(/\/$/, '')
  return current === target
}
</script>

<template>
  <div class="site-shell">
    <header id="site-header">
      <a :href="withBase('/')" id="site-logo">
        <img :src="withBase('/favicon.png')" alt="">
        <span>
          <strong>{{ site.title }}</strong>
          <small>Native Plasma Wayland edition</small>
        </span>
      </a>
      <div class="social-links">
        <a v-for="item in theme.socialLinks"
          :key="item.text"
          :style="{ '--accent': item.color }"
          :href="item.link">{{ item.text }}</a>
      </div>
    </header>
    <div class="site-grid">
      <nav class="sidebar" aria-label="Documentation">
        <template v-for="group in theme.sidebar" :key="group.text">
          <section class="nav-group">
            <h2>{{ group.text }}</h2>
            <a v-for="item in group.items"
              :key="item.text"
              :class="{ active: isActive(item.link) }"
              :href="itemHref(item.link)"
              :target="isExternal(item.link) ? '_blank' : undefined"
              :rel="isExternal(item.link) ? 'noreferrer' : undefined">
              {{ item.text }}
              <span v-if="isExternal(item.link)" aria-hidden="true">↗</span>
            </a>
          </section>
        </template>
      </nav>
      <main class="content-card">
        <article class="markdown-body">
          <Content />
        </article>
        <footer>
          Built on
          <a href="https://github.com/SnosMe/awakened-poe-trade">Awakened PoE Trade</a>
          with gratitude to its maintainers and contributors.
        </footer>
      </main>
    </div>
  </div>
</template>

<style lang="postcss">
@import url('./style.css');
@tailwind base;
@tailwind utilities;

:global(*) { box-sizing: border-box; }

:global(body) {
  margin: 0;
  color: #20242b;
  background:
    radial-gradient(circle at 12% -10%, rgba(29, 153, 243, .2), transparent 32rem),
    #f4f7fa;
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont,
    "Segoe UI", sans-serif;
}

.site-shell {
  width: min(1120px, calc(100% - 32px));
  margin: 28px auto 48px;
}

#site-header {
  min-height: 116px;
  padding: 24px 28px;
  border: 1px solid rgba(255, 255, 255, .18);
  border-radius: 22px;
  background:
    linear-gradient(125deg, rgba(18, 49, 77, .97), rgba(18, 107, 166, .95)),
    #12314d;
  box-shadow: 0 18px 48px rgba(20, 56, 84, .18);
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 24px;
}

#site-logo {
  color: white;
  text-decoration: none;
  display: flex;
  align-items: center;
  gap: 18px;
}

#site-logo img {
  width: 68px;
  height: 68px;
  border-radius: 18px;
  box-shadow: 0 9px 24px rgba(0, 0, 0, .22);
}

#site-logo span { display: flex; flex-direction: column; gap: 3px; }
#site-logo strong { font-size: 1.3rem; letter-spacing: -.02em; }
#site-logo small { color: #b9e2ff; font-size: .82rem; }

.social-links { display: flex; flex-wrap: wrap; justify-content: flex-end; gap: 8px; }
.social-links a {
  padding: 8px 12px;
  color: white;
  background: color-mix(in srgb, var(--accent) 75%, transparent);
  border: 1px solid rgba(255, 255, 255, .24);
  border-radius: 999px;
  font-size: .78rem;
  font-weight: 650;
  text-decoration: none;
  transition: transform .15s ease, filter .15s ease;
}
.social-links a:hover { filter: brightness(1.15); transform: translateY(-1px); }

.site-grid {
  margin-top: 22px;
  display: grid;
  grid-template-columns: 214px minmax(0, 1fr);
  gap: 22px;
  align-items: start;
}

.sidebar {
  position: sticky;
  top: 20px;
  padding: 12px;
  border: 1px solid #dce5ec;
  border-radius: 18px;
  background: rgba(255, 255, 255, .9);
  box-shadow: 0 10px 28px rgba(31, 62, 84, .07);
}

.nav-group + .nav-group { margin-top: 12px; padding-top: 12px; border-top: 1px solid #e8eef2; }
.nav-group h2 {
  margin: 0 10px 5px;
  color: #74818d;
  font-size: .68rem;
  font-weight: 800;
  letter-spacing: .09em;
  text-transform: uppercase;
}
.nav-group a {
  padding: 8px 10px;
  border-radius: 10px;
  color: #354553;
  display: flex;
  justify-content: space-between;
  gap: 8px;
  font-size: .84rem;
  text-decoration: none;
}
.nav-group a:hover { color: #075f9e; background: #edf7ff; }
.nav-group a.active {
  color: white;
  background: linear-gradient(120deg, #147dbb, #1d99f3);
  box-shadow: 0 6px 14px rgba(29, 153, 243, .22);
}

.content-card {
  min-width: 0;
  overflow: hidden;
  border: 1px solid #dce5ec;
  border-radius: 20px;
  background: white;
  box-shadow: 0 14px 36px rgba(31, 62, 84, .08);
}
.markdown-body { padding: 14px 42px 42px; }
footer {
  padding: 17px 42px;
  border-top: 1px solid #e8eef2;
  color: #687782;
  background: #f9fbfc;
  font-size: .78rem;
}
footer a { color: #0877bb; }

@media (max-width: 780px) {
  .site-shell { width: min(100% - 20px, 680px); margin-top: 10px; }
  #site-header { padding: 20px; align-items: flex-start; flex-direction: column; }
  #site-logo img { width: 56px; height: 56px; border-radius: 14px; }
  .social-links { justify-content: flex-start; }
  .site-grid { grid-template-columns: 1fr; }
  .sidebar { position: static; display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }
  .nav-group + .nav-group { margin: 0; padding: 0; border: 0; }
  .markdown-body { padding: 10px 22px 30px; }
  footer { padding: 16px 22px; }
}

@media (max-width: 480px) {
  .sidebar { grid-template-columns: 1fr; }
}
</style>
