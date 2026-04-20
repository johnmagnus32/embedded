#!/usr/bin/env python3
"""
gen_devicetree.py — Parse a .dts file and generate devicetree.h

Simplified version of Zephyr's gen_defines.py + edtlib.py.
Zephyr's version handles the full DT spec (includes, overlays, bindings,
phandle resolution across files). Ours handles a single self-contained .dts.
"""

import re
import sys

def parse_dts(text):
    """Parse DTS text into a tree of nodes.

    Returns dict: { path: { 'props': {name: value}, 'label': str|None } }
    """
    # Strip C-style comments
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    text = re.sub(r'//.*', '', text)

    nodes = {}
    labels = {}  # label -> path

    def parse_node(src, path):
        """Parse the contents between { } for a node at the given path."""
        props = {}
        pos = 0

        while pos < len(src):
            # Skip whitespace
            m = re.match(r'\s+', src[pos:])
            if m:
                pos += m.end()
                continue

            if pos >= len(src):
                break

            # Child node: [label:] name[@addr] { ... };
            m = re.match(
                r'(?:(\w+)\s*:\s*)?'     # optional label
                r'([\w,.-]+)'            # node name
                r'(?:@([0-9a-fA-F]+))?'  # optional @address
                r'\s*\{',
                src[pos:]
            )
            if m:
                label = m.group(1)
                name = m.group(2)
                addr = m.group(3)
                node_name = f"{name}@{addr}" if addr else name
                child_path = f"{path}/{node_name}" if path != "/" else f"/{node_name}"

                # Find matching closing brace
                brace_start = pos + m.end()
                depth = 1
                i = brace_start
                while i < len(src) and depth > 0:
                    if src[i] == '{':
                        depth += 1
                    elif src[i] == '}':
                        depth -= 1
                    i += 1

                child_body = src[brace_start:i - 1]

                # Skip the }; after
                pos = i
                m2 = re.match(r'\s*;', src[pos:])
                if m2:
                    pos += m2.end()

                if label:
                    labels[label] = child_path

                parse_node(child_body, child_path)
                continue

            # Property: name = <value> | "string" | [...];
            m = re.match(r'([\w,#.-]+)\s*=\s*', src[pos:])
            if m:
                prop_name = m.group(1)
                pos += m.end()

                # Angle-bracket value: <0x1234> or <&label bus bit>
                m2 = re.match(r'<([^>]*)>\s*;', src[pos:])
                if m2:
                    raw = m2.group(1).strip()
                    props[prop_name] = parse_angle_value(raw)
                    pos += m2.end()
                    continue

                # String value: "..."
                m2 = re.match(r'"([^"]*)"\s*;', src[pos:])
                if m2:
                    props[prop_name] = m2.group(1)
                    pos += m2.end()
                    continue

            # Boolean property (no value): name;
            m = re.match(r'([\w,.-]+)\s*;', src[pos:])
            if m:
                props[m.group(1)] = True
                pos += m.end()
                continue

            # Skip anything else
            pos += 1

        nodes[path] = {'props': props, 'label': None}

        # Assign labels
        for lbl, lpath in labels.items():
            if lpath in nodes:
                nodes[lpath]['label'] = lbl

    def parse_angle_value(raw):
        """Parse contents of < >. Returns int, list, or phandle ref string."""
        tokens = raw.split()
        if len(tokens) == 1:
            return parse_token(tokens[0])
        return [parse_token(t) for t in tokens]

    def parse_token(t):
        if t.startswith('&'):
            return t  # phandle reference, resolve later
        if t.startswith('0x') or t.startswith('0X'):
            return int(t, 16)
        return int(t)

    # Find the root node: / { ... };
    m = re.search(r'/\s*\{', text)
    if not m:
        print("Error: no root node found", file=sys.stderr)
        sys.exit(1)

    brace_start = m.end()
    depth = 1
    i = brace_start
    while i < len(text) and depth > 0:
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
        i += 1

    root_body = text[brace_start:i - 1]
    parse_node(root_body, "/")

    return nodes, labels


def resolve_phandle(val, labels, nodes):
    """Resolve &label to the node's reg address."""
    if isinstance(val, str) and val.startswith('&'):
        label = val[1:]
        path = labels.get(label)
        if path and path in nodes:
            reg = nodes[path]['props'].get('reg')
            if isinstance(reg, int):
                return reg
    return val


def generate_header(nodes, labels):
    """Generate devicetree.h from parsed nodes."""
    lines = [
        "/* AUTO-GENERATED by gen_devicetree.py from board.dts — DO NOT EDIT */",
        "#ifndef DEVICETREE_H",
        "#define DEVICETREE_H",
        "",
    ]

    # Find the clock frequency
    for path, node in nodes.items():
        if node['props'].get('compatible') == 'fixed-clock':
            freq = node['props'].get('clock-frequency')
            if freq:
                lines.append(f"#define DT_SYSCLK_HZ {freq}")
                lines.append("")

    # Emit defines for each labeled node
    for label, path in sorted(labels.items()):
        node = nodes.get(path)
        if not node:
            continue

        prefix = f"DT_{label.upper()}"
        props = node['props']

        lines.append(f"/* {path} */")

        # reg property → BASE address
        if 'reg' in props:
            reg = props['reg']
            addr = reg if isinstance(reg, int) else reg
            lines.append(f"#define {prefix}_BASE 0x{addr:08X}")

        # clocks = <&rcc bus bit> → CLK_BUS and CLK_BIT
        if 'clocks' in props:
            clk = props['clocks']
            if isinstance(clk, list) and len(clk) >= 3:
                lines.append(f"#define {prefix}_CLK_BUS {clk[1]}")
                lines.append(f"#define {prefix}_CLK_BIT {clk[2]}")

        # Emit all simple integer and string properties
        skip = {'compatible', 'reg', 'clocks', 'status', 'tx-port'}
        for pname, pval in props.items():
            if pname in skip:
                continue
            cname = pname.replace('-', '_').replace(',', '_').upper()
            if isinstance(pval, int):
                lines.append(f"#define {prefix}_{cname} {pval}")
            elif isinstance(pval, str) and not pval.startswith('&'):
                lines.append(f"#define {prefix}_{cname} \"{pval}\"")

        # tx-port phandle → resolve to GPIO base and clock bit
        if 'tx-port' in props:
            ref = props['tx-port']
            if isinstance(ref, str) and ref.startswith('&'):
                gpio_label = ref[1:]
                gpio_path = labels.get(gpio_label)
                if gpio_path and gpio_path in nodes:
                    gpio = nodes[gpio_path]
                    gpio_reg = gpio['props'].get('reg', 0)
                    lines.append(f"#define {prefix}_TX_PORT_BASE 0x{gpio_reg:08X}")
                    gpio_clk = gpio['props'].get('clocks')
                    if isinstance(gpio_clk, list) and len(gpio_clk) >= 3:
                        lines.append(f"#define {prefix}_GPIO_CLK_BIT {gpio_clk[2]}")

        lines.append("")

    # Chosen aliases
    chosen_path = "/"
    chosen_node = None
    for path, node in nodes.items():
        if path.endswith("/chosen"):
            chosen_node = node
            break

    if chosen_node:
        lines.append("/* chosen aliases */")
        for pname, pval in chosen_node['props'].items():
            ref = pval if isinstance(pval, str) else (pval[0] if isinstance(pval, list) else None)
            if isinstance(ref, str) and ref.startswith('&'):
                alias_label = ref[1:]
                alias_path = labels.get(alias_label)
                if alias_path and alias_path in nodes:
                    alias_prefix = f"DT_{alias_label.upper()}"
                    cname = pname.replace('-', '_').upper()
                    lines.append(f"#define DT_{cname}_BASE {alias_prefix}_BASE")
                    lines.append(f"#define DT_{cname}_BAUDRATE {alias_prefix}_BAUDRATE")
        lines.append("")

    lines.append("#endif")
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.dts> <output.h>", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1]) as f:
        text = f.read()

    nodes, labels = parse_dts(text)
    header = generate_header(nodes, labels)

    with open(sys.argv[2], 'w') as f:
        f.write(header)

    print(f"Generated {sys.argv[2]} from {sys.argv[1]} "
          f"({len(nodes)} nodes, {len(labels)} labels)")


if __name__ == '__main__':
    main()
