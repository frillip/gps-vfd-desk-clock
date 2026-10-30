"""House-style passes that clang-format cannot do, for the V2+ dsPIC sources.

  - A /* */ comment of up to three lines becomes // lines, unless code follows
    it on its last line (an inline comment, or a macro continuation).
  - Exactly two blank lines after each function definition in a .c file; at
    most one blank line anywhere else, and in all headers.

Usage: python3 tools/style.py <files...>   (tools/format.sh runs it for you)
Line endings (CRLF or LF) are preserved per file.
"""
import sys


def convert(text):
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c in '"\'':                       # string or character literal
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1]); i = j + 1; continue
        if text.startswith('//', i):
            j = text.find('\n', i); j = n if j < 0 else j
            out.append(text[i:j]); i = j; continue
        if not text.startswith('/*', i):
            out.append(c); i += 1; continue
        end = text.find('*/', i + 2)
        body = text[i + 2:end]
        line_start = text.rfind('\n', 0, i) + 1
        prefix = text[line_start:i]
        line_end = text.find('\n', end + 2); line_end = n if line_end < 0 else line_end
        suffix = text[end + 2:line_end]
        lines = body.count('\n') + 1
        if lines > 3 or suffix.strip():
            out.append(text[i:end + 2]); i = end + 2; continue
        content = []
        for raw in body.split('\n'):
            s = raw.strip()
            if s.startswith('*'):
                s = s[1:].strip() if not s.startswith('*/') else ''
            content.append(s)
        while content and not content[0]: content.pop(0)
        while content and not content[-1]: content.pop()
        if not content:
            content = ['']
        column = len(prefix.expandtabs())
        indent = ' ' * column
        rendered = [('// ' + s).rstrip() for s in content]
        out.append(('\n' + indent).join(rendered))
        # drop trailing spaces left between */ and end of line
        i = end + 2
        while i < n and text[i] in ' \t':
            i += 1
    return ''.join(out)



def brace_depths(lines):
    """Brace depth at the start of each line, ignoring comments and literals."""
    depths, depth, in_block = [], 0, False
    for line in lines:
        depths.append(depth)
        i, n = 0, len(line)
        while i < n:
            if in_block:
                j = line.find('*/', i)
                if j < 0:
                    break
                in_block, i = False, j + 2
                continue
            c = line[i]
            if line.startswith('//', i):
                break
            if line.startswith('/*', i):
                in_block, i = True, i + 2
                continue
            if c in '"\'':
                j = i + 1
                while j < n and line[j] != c:
                    j += 2 if line[j] == '\\' else 1
                i = j + 1
                continue
            depth += (c == '{') - (c == '}')
            i += 1
    return depths

def normalise(text, is_c):
    lines = text.split('\n')
    depths = brace_depths(lines)
    out = []
    pending = 0          # blank lines seen since the last content line
    after_function = False
    for line, depth in zip(lines, depths):
        if line.strip() == '':
            pending += 1
            continue
        if out:
            if depth > 0:
                keep = min(pending, 1)
            elif is_c and after_function:
                keep = 2
            else:
                keep = min(pending, 1)
            out.extend([''] * keep)
        pending = 0
        out.append(line)
        after_function = depth == 1 and line == '}'
    return '\n'.join(out).rstrip('\n') + '\n'



def main(paths):
    for path in paths:
        raw = open(path, newline='').read()
        crlf = '\r\n' in raw
        text = raw.replace('\r\n', '\n')
        text = normalise(convert(text), path.endswith('.c'))
        open(path, 'w', newline='').write(text.replace('\n', '\r\n') if crlf else text)


if __name__ == '__main__':
    main(sys.argv[1:])
