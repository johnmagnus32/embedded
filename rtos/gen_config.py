#!/usr/bin/env python3
"""
gen_config.py — Parse .config and generate config.h

Like Zephyr's Kconfig → autoconf.h pipeline.
Reads CONFIG_X=y|n lines, emits #define CONFIG_X 1 for y options.
Also emits a config.mk for the Makefile to conditionally include objects.
"""

import sys

def main():
    if len(sys.argv) != 4:
        sys.exit(f"Usage: {sys.argv[0]} <.config> <config.h> <config.mk>")

    config = {}
    with open(sys.argv[1]) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if '=' in line:
                key, val = line.split('=', 1)
                config[key.strip()] = val.strip()

    # Generate config.h
    lines = [
        "/* AUTO-GENERATED from .config — DO NOT EDIT */",
        "#ifndef CONFIG_H",
        "#define CONFIG_H",
        "",
    ]
    for key, val in sorted(config.items()):
        if val == 'y':
            lines.append(f"#define {key} 1")
        # 'n' options are simply not defined
    lines.append("")
    lines.append("#endif")

    with open(sys.argv[2], 'w') as f:
        f.write('\n'.join(lines) + '\n')

    # Generate config.mk (Makefile conditionals)
    with open(sys.argv[3], 'w') as f:
        f.write("# AUTO-GENERATED from .config — DO NOT EDIT\n")
        for key, val in sorted(config.items()):
            f.write(f"{key} := {val}\n")

    enabled = sum(1 for v in config.values() if v == 'y')
    print(f"Generated {sys.argv[2]} and {sys.argv[3]} "
          f"({enabled} options enabled)")

if __name__ == '__main__':
    main()
