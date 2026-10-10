#!/usr/bin/env python3
"""Builds the shared category icons and the reboot/suspend icons.

Writes SVG sources next to this script and 256x256 PNGs:
  icons/categories/<folder>.png
  icons/payloads/reboot.png, icons/payloads/suspend.png (sources in icons/payloads/src)
Needs: pip install cairosvg (uses the Inter font if installed).
"""
import os
import cairosvg

HERE = os.path.dirname(os.path.abspath(__file__))
ICONS = os.path.abspath(os.path.join(HERE, '..', '..'))

# Folder chip colors from the pearl theme in index.html.
CATS = {
    'loader':  '#4f46e5',
    'hen':     '#db2777',
    'storage': '#7c3aed',
    'mods':    '#047857',
    'files':   '#b45309',
    'net':     '#c2410c',
    'media':   '#0369a1',
    'system':  '#475569',
    'tools':   '#0f766e',
}

# White line glyphs on a 256 grid, centered around (128,128).
GLYPHS = {
    # rocket
    'loader': '<path d="M128 58c26 18 38 46 38 78v26h-76v-26c0-32 12-60 38-78z"/>'
              '<circle cx="128" cy="116" r="13"/>'
              '<path d="M90 140l-20 18v26l20-10M166 140l20 18v26l-20-10M112 170v24M128 170v32M144 170v24"/>',
    # open padlock
    'hen': '<rect x="78" y="118" width="100" height="78" rx="16"/>'
           '<path d="M100 118V96a28 28 0 0 1 54-10"/>'
           '<path d="M128 148v18"/>',
    # gamepad
    'storage': '<path d="M84 98h88c22 0 34 16 38 40l6 34c3 18-15 28-28 15l-22-21H90l-22 21c-13 13-31 3-28-15l6-34c4-24 16-40 38-40z"/>'
               '<path d="M92 126v28M78 140h28"/><circle cx="164" cy="132" r="5"/><circle cx="178" cy="148" r="5"/>',
    # wrench
    'mods': '<path d="M168 70a34 34 0 0 0-40 44l-56 56a14 14 0 0 0 20 20l56-56a34 34 0 0 0 44-40l-20 20-18-6-6-18z"/>',
    # folder
    'files': '<path d="M62 92a12 12 0 0 1 12-12h36l14 16h58a12 12 0 0 1 12 12v72a12 12 0 0 1-12 12H74a12 12 0 0 1-12-12z"/>'
             '<path d="M62 118h132"/>',
    # globe
    'net': '<circle cx="128" cy="128" r="62"/><path d="M66 128h124M128 66c-34 36-34 88 0 124M128 66c34 36 34 88 0 124"/>'
           '<path d="M78 96h100M78 160h100"/>',
    # play in a frame
    'media': '<rect x="62" y="78" width="132" height="100" rx="18"/><path d="M114 106l36 22-36 22z" fill="#fff"/>',
    # power
    'system': '<path d="M128 64v58"/><path d="M90 90a54 54 0 1 0 76 0"/>',
    # code brackets
    'tools': '<path d="M100 88l-40 40 40 40M156 88l40 40-40 40M140 74l-24 108"/>',
}


def mix(hex_color, other, t):
    a = [int(hex_color[i:i + 2], 16) for i in (1, 3, 5)]
    b = [int(other[i:i + 2], 16) for i in (1, 3, 5)]
    return '#%02x%02x%02x' % tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def category_svg(cid, color):
    top = mix(color, '#ffffff', 0.28)
    bottom = mix(color, '#000000', 0.18)
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" width="256" height="256">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="{top}"/>
      <stop offset="1" stop-color="{bottom}"/>
    </linearGradient>
    <linearGradient id="shine" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ffffff" stop-opacity=".28"/>
      <stop offset=".5" stop-color="#ffffff" stop-opacity="0"/>
    </linearGradient>
  </defs>
  <rect x="8" y="8" width="240" height="240" rx="54" fill="url(#bg)"/>
  <rect x="8" y="8" width="240" height="240" rx="54" fill="url(#shine)"/>
  <rect x="9.5" y="9.5" width="237" height="237" rx="52.5" fill="none" stroke="#ffffff" stroke-opacity=".35" stroke-width="3"/>
  <g fill="none" stroke="#ffffff" stroke-width="14" stroke-linecap="round" stroke-linejoin="round">{GLYPHS[cid]}</g>
</svg>
'''


# reboot/suspend: same look as the ELF Launcher icon (icons/icon.svg).
OHAD_GLYPHS = {
    'reboot': ('REBOOT', '<path d="M290 186a52 52 0 1 1-34-22" fill="none"/>'
                         '<path d="M246 146l22 18-22 18" fill="none"/>'),
    'suspend': ('SUSPEND', '<path d="M251 130A56 56 0 1 0 305.1 203.8A46 46 0 0 1 251 130Z" fill="url(#g)" stroke="none"/>'),
}


def ohad_svg(word, glyph):
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">
  <defs>
    <radialGradient id="bg" cx="30%" cy="22%" r="95%">
      <stop offset="0" stop-color="#262c52"/>
      <stop offset=".55" stop-color="#121624"/>
      <stop offset="1" stop-color="#0b0d14"/>
    </radialGradient>
    <linearGradient id="g" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#8fa0ff"/>
      <stop offset=".55" stop-color="#a78bfa"/>
      <stop offset="1" stop-color="#f472b6"/>
    </linearGradient>
    <linearGradient id="chip" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#1f2540"/>
      <stop offset="1" stop-color="#151a2e"/>
    </linearGradient>
    <filter id="glow" x="-30%" y="-30%" width="160%" height="160%">
      <feGaussianBlur stdDeviation="18"/>
    </filter>
  </defs>
  <rect width="512" height="512" fill="url(#bg)"/>
  <circle cx="256" cy="226" r="130" fill="#7c8cff" opacity=".22" filter="url(#glow)"/>
  <g stroke="url(#g)" stroke-width="14" stroke-linecap="round">
    <path d="M196 92v34M256 92v34M316 92v34M196 326v34M256 326v34M316 326v34M106 166h34M106 226h34M106 286h34M372 166h34M372 226h34M372 286h34"/>
  </g>
  <rect x="140" y="126" width="232" height="200" rx="38" fill="url(#chip)" stroke="url(#g)" stroke-width="12"/>
  <g transform="translate(0,40)" stroke="url(#g)" stroke-width="16" stroke-linecap="round" stroke-linejoin="round">{glyph}</g>
  <text x="256" y="440" text-anchor="middle" font-family="Inter, 'Segoe UI', 'DejaVu Sans', Arial, sans-serif" font-weight="800" font-size="{64 if len(word) < 7 else 56}" letter-spacing="5" fill="#eef0ff">{word}</text>
  <text x="256" y="478" text-anchor="middle" font-family="Inter, 'Segoe UI', 'DejaVu Sans', Arial, sans-serif" font-weight="700" font-size="22" letter-spacing="5" fill="url(#g)">X-F1REBALL-X</text>
</svg>
'''


def render(svg, svg_path, png_path):
    with open(svg_path, 'w') as f:
        f.write(svg)
    cairosvg.svg2png(bytestring=svg.encode(), write_to=png_path, output_width=256, output_height=256)
    print(png_path, os.path.getsize(png_path))


if __name__ == '__main__':
    for cid, color in CATS.items():
        render(category_svg(cid, color), os.path.join(HERE, cid + '.svg'), os.path.join(ICONS, 'categories', cid + '.png'))
    psrc = os.path.join(ICONS, 'payloads', 'src')
    os.makedirs(psrc, exist_ok=True)
    for name, (word, glyph) in OHAD_GLYPHS.items():
        render(ohad_svg(word, glyph), os.path.join(psrc, name + '.svg'), os.path.join(ICONS, 'payloads', name + '.png'))
