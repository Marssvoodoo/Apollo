import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';

// Check the deployable output, including public files that Vite copies without
// resolving them. An unresolved CSS package URL can otherwise ship with exit 0.
const root = path.resolve(process.argv[2] ?? path.join(process.env.SUNSHINE_ASSETS_DIR || 'build', 'assets/web'));
const pages = ['apps', 'config', 'index', 'login', 'password', 'pin', 'troubleshooting', 'welcome'];
const seen = new Set();
let fontAwesome = false;

function check(reference, parent) {
  if (/^(?:[a-z][\w+.-]*:|\/\/|#)/i.test(reference)) return;
  const url = new URL(reference, `https://apollo.invalid/${parent}`);
  const relative = decodeURIComponent(url.pathname).replace(/^\//, '');
  const file = path.resolve(root, relative);
  assert(file.startsWith(root + path.sep), `Asset escapes output directory: ${relative}`);
  assert(fs.existsSync(file) && fs.statSync(file).isFile(), `Missing packaged asset: ${relative} (from ${parent})`);
  if (seen.has(relative)) return;
  seen.add(relative);
  if (relative.endsWith('.css')) {
    const css = fs.readFileSync(file, 'utf8');
    if (css.includes('Font Awesome') && css.includes('@font-face')) fontAwesome = true;
    for (const match of css.matchAll(/url\(\s*['"]?([^'"\s)]+)['"]?\s*\)/g)) check(match[1], relative);
  }
}

for (const page of pages) {
  const html = fs.readFileSync(path.join(root, `${page}.html`), 'utf8');
  for (const element of html.matchAll(/<(?:link|script)\b[^>]*>/gi)) {
    const reference = element[0].match(/\b(?:href|src)=["']([^"']+)["']/i);
    if (reference) check(reference[1], `${page}.html`);
  }
}
assert(fontAwesome, 'Font Awesome styles and font faces must be bundled');
console.log(JSON.stringify({ pages: pages.length, assets: seen.size, fontAwesome }, null, 2));
