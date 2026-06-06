import sys
with open('scripts/generate_report.py', 'r', encoding='utf-8') as f:
    src = f.read()

# Replace every non-latin1 character with a safe ASCII equivalent
replacements = [
    ('\u2022', '-'),
    ('\u2019', "'"),
    ('\u2018', "'"),
    ('\u201c', '"'),
    ('\u201d', '"'),
    ('\u207b\u00b9\u00b2', '^-12'),
    ('\u207b\u00b9\u00b3', '^-13'),
    ('\u207b\u00b9', '^-1'),
    ('\u207b', '^-'),
    ('\u00d7', 'x'),
    ('\u00b9', '1'),
    ('\u00b2', '2'),
    ('\u00b3', '3'),
    ('\u2014', '--'),
    ('\u2013', '-'),
    ('\u03b5', 'epsilon'),
    ('\u03b1', 'alpha'),
    ('\u2264', '<='),
    ('\u2265', '>='),
    ('\u2260', '!='),
    ('\u03a3', 'SUM'),
    ('\u2192', '->'),
    ('\u2248', '~'),
    ('\u00b0', ' deg'),
    ('\u00b5', 'mu'),
    ('\u00e9', 'e'),
    ('\u00e0', 'a'),
    ('\u221a', 'sqrt'),
    ('\u03c0', 'pi'),
    ('\u03a9', 'Ohm'),
    ('\u2026', '...'),
    ('\u2032', "'"),
    ('\u221e', 'inf'),
    ('\u03b5', 'eps'),
    ('\u00b7', '*'),
    ('\u22c5', '*'),
    ('\u2211', 'SUM'),
]
for old, new in replacements:
    src = src.replace(old, new)

# Brute-force: replace ANY remaining char > 127 with '?'
result = []
for ch in src:
    if ord(ch) > 127:
        result.append('?')
    else:
        result.append(ch)
src = ''.join(result)

with open('scripts/generate_report.py', 'w', encoding='utf-8') as f:
    f.write(src)
print('OK - all chars sanitised, file written.')

