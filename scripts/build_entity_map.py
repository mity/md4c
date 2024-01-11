#!/usr/bin/env python3

import os
import sys
import json
import urllib.request


url_str = "https://html.spec.whatwg.org/entities.json"

with urllib.request.urlopen(url_str) as url:
    entities = json.load(url)

records = []

for name in entities:
    if name[-1] != ';':
        continue

    codepoints = entities[name]["codepoints"]

    if len(codepoints) > 2:
        print('Entity {} needs {} codepints; may need to update the .c code '
                ' accordingly.'.format(name, len(codepoints)), file=sys.stderr)
        sys.exit(1)

    while len(codepoints) < 2:
        codepoints.append(0)

    codepoints_str = map(str, codepoints)
    records.append("    { \"" + name + "\", { " + ", ".join(codepoints_str) + " } }")

records.sort()

sys.stdout.write("static const ENTITY ENTITY_MAP[] = {\n")
sys.stdout.write(",\n".join(records))
sys.stdout.write("\n};\n\n")
