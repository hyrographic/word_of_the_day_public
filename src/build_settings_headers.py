import os
import re

# Build settings_page.h from settings.html
with open('html/settings.html', 'r') as f:
    settings_html = f.read()

with open('include/settings_page.h', 'r') as f:
    template = f.read()

if 'HTML_PLACEHOLDER' in template:
    print('replacing placeholder')
    output = template.replace('HTML_PLACEHOLDER', settings_html).replace('HTML_PLACEHOLDER', settings_html)
else:
    print('replacing existing html')
    # replace existing HTML
    s = template.index('<!DOCTYPE html>')
    e = template.index('</html>')
    removed_html = [c for i, c in enumerate(template) if (i < s) or (i > e + len('</html>') - 1)]
    output = ''.join(removed_html[:s] + [settings_html] + removed_html[s:])

with open('include/settings_page.h', 'w') as f:
    f.write(output)

print("Generated settings_page.h")

# Build settings_assets.h from SVG files
svg_dir = 'html/svg'
svg_files = [f for f in os.listdir(svg_dir) if f.endswith('.svg')]

def to_variable_name(filename):
    """Convert filename to valid C variable name"""
    name = os.path.splitext(filename)[0]
    # Replace non-alphanumeric chars with underscore
    name = re.sub(r'[^a-zA-Z0-9]', '_', name)
    # Ensure it doesn't start with a number
    if name[0].isdigit():
        name = '_' + name
    return name.lower() + '_svg'

header_content = '''#ifndef SETTINGS_ASSETS_H
#define SETTINGS_ASSETS_H

#include <Arduino.h>
#include <WebServer.h>

'''

# Add each SVG as a PROGMEM string
for svg_file in sorted(svg_files):
    var_name = to_variable_name(svg_file)
    svg_path = os.path.join(svg_dir, svg_file)

    with open(svg_path, 'r', encoding='utf-8') as f:
        svg_content = f.read().strip()

    header_content += f'const char {var_name}[] PROGMEM = R"rawliteral({svg_content})rawliteral";\n\n'

# Add helper function to register routes
header_content += '''// Helper function to register all SVG routes
inline void registerSvgRoutes(WebServer& server) {
'''

for svg_file in sorted(svg_files):
    var_name = to_variable_name(svg_file)
    header_content += f'''    server.on("/svg/{svg_file}", HTTP_GET, [&server]() {{
        server.send(200, "image/svg+xml", {var_name});
    }});
'''

header_content += '''}

#endif
'''

with open('include/settings_assets.h', 'w', encoding='utf-8') as f:
    f.write(header_content)

print(f"Generated settings_assets.h with {len(svg_files)} SVG files")
