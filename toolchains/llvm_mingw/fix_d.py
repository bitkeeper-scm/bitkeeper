import sys
import os
import re

if len(sys.argv) < 3:
    sys.exit(0)

mf_file = sys.argv[1]
tool_dir = os.path.realpath(sys.argv[2])

if not os.path.isfile(mf_file):
    sys.exit(0)

with open(mf_file, "r") as f:
    content = f.read()

content = content.replace("\\\n", " ").replace("\\\r\n", " ")
tokens = [t for t in re.split(r'\s+', content) if t and t != "\\"]

new_tokens = []
for tok in tokens:
    if tok.endswith(":"):
        new_tokens.append(tok)
    else:
        tok = tok.replace("\\", "/")
        real_tok = os.path.realpath(tok)
        if real_tok.startswith(tool_dir) or tok.startswith(tool_dir):
            # Ignore toolchain internal headers
            continue
        else:
            new_tokens.append(tok)

with open(mf_file, "w") as f:
    if new_tokens:
        f.write(new_tokens[0] + " \\\n  " + " \\\n  ".join(new_tokens[1:]) + "\n")
    else:
        f.write("")
