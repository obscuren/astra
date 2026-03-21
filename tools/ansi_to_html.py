#!/usr/bin/env python3
"""Convert ANSI-colored terminal output to an HTML file."""
import re
import subprocess
import sys

# xterm-256 color table (first 16 + 216 cube + 24 grayscale)
def xterm256_to_rgb(n):
    if n < 16:
        # Standard colors
        base = [
            (0,0,0), (128,0,0), (0,128,0), (128,128,0),
            (0,0,128), (128,0,128), (0,128,128), (192,192,192),
            (128,128,128), (255,0,0), (0,255,0), (255,255,0),
            (0,0,255), (255,0,255), (0,255,255), (255,255,255),
        ]
        return base[n]
    elif n < 232:
        n -= 16
        r = (n // 36) * 51
        g = ((n % 36) // 6) * 51
        b = (n % 6) * 51
        return (r, g, b)
    else:
        v = 8 + (n - 232) * 10
        return (v, v, v)

def rgb_hex(n):
    r, g, b = xterm256_to_rgb(n)
    return f"#{r:02x}{g:02x}{b:02x}"

def ansi_to_html(text):
    lines = []
    lines.append("""<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Overworld Mockup v2</title>
<style>
body { background: #0a0a0a; margin: 20px; }
pre {
    font-family: 'Menlo', 'Monaco', 'Courier New', monospace;
    font-size: 14px;
    line-height: 1.15;
    letter-spacing: 0px;
}
</style>
</head>
<body>
<pre>""")

    # Parse ANSI escape sequences
    fg = None
    bg = None
    bold = False
    i = 0
    buf = []

    def style():
        parts = []
        if fg is not None:
            parts.append(f"color:{rgb_hex(fg)}")
        if bg is not None:
            parts.append(f"background:{rgb_hex(bg)}")
        if bold:
            parts.append("font-weight:bold")
        return ";".join(parts)

    def flush():
        nonlocal buf
        if not buf:
            return ""
        content = "".join(buf)
        buf = []
        s = style()
        if s:
            return f'<span style="{s}">{content}</span>'
        return content

    result = []
    pos = 0
    esc_re = re.compile(r'\033\[([0-9;]*)m')

    for m in esc_re.finditer(text):
        # Text before this escape
        before = text[pos:m.start()]
        for ch in before:
            if ch == '<':
                buf.append('&lt;')
            elif ch == '>':
                buf.append('&gt;')
            elif ch == '&':
                buf.append('&amp;')
            else:
                buf.append(ch)

        result.append(flush())

        codes = m.group(1).split(';') if m.group(1) else ['0']
        ci = 0
        while ci < len(codes):
            c = int(codes[ci]) if codes[ci] else 0
            if c == 0:
                fg = None; bg = None; bold = False
            elif c == 1:
                bold = True
            elif c == 38 and ci + 2 < len(codes) and int(codes[ci+1]) == 5:
                fg = int(codes[ci+2])
                ci += 2
            elif c == 48 and ci + 2 < len(codes) and int(codes[ci+1]) == 5:
                bg = int(codes[ci+2])
                ci += 2
            ci += 1

        pos = m.end()

    # Remaining text
    before = text[pos:]
    for ch in before:
        if ch == '<':
            buf.append('&lt;')
        elif ch == '>':
            buf.append('&gt;')
        elif ch == '&':
            buf.append('&amp;')
        else:
            buf.append(ch)
    result.append(flush())

    lines.append("".join(result))
    lines.append("</pre></body></html>")
    return "\n".join(lines)

if __name__ == '__main__':
    script = sys.argv[1] if len(sys.argv) > 1 else 'tools/overworld_mockup.py'
    proc = subprocess.run(['python3', script],
                          capture_output=True, text=True)
    html = ansi_to_html(proc.stdout)
    out = '/Users/jeffrey/dev/crawler/tools/overworld_mockup.html'
    with open(out, 'w') as f:
        f.write(html)
    print(f"Wrote {out}")
