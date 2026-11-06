"""Add block/definition spacing after clang-format without touching string literals."""

import re

# Preserve raw strings, escaped literals and comments while locating source braces.
LITERALS = re.compile(
    r'R"([^()\\\s]{0,16})\([\s\S]*?\)\1"|"(?:\\[\s\S]|[^"\\])*"'
    r"|'(?:\\[^\n]|[^'\\\n])*'|//[^\n]*|/\*[\s\S]*?\*/"
)


def definition_spacing(source, header=False):
    masked = LITERALS.sub(lambda value: re.sub(r"[^\n]", " ", value[0]), source)
    lines = source.splitlines(keepends=True)
    code = masked.splitlines()
    output = []
    for index, line in enumerate(lines):
        output.append(line)
        if index + 1 >= len(lines) or not code[index + 1].strip():
            continue
        current, following = code[index].strip(), code[index + 1].strip()
        closed = current in {"}", "};"}
        declaration = header and bool(
            re.search(r"\)\s*(?:const\s*)?(?:override\s*)?;\s*$", current)
        )
        if (
            (closed or declaration)
            and following
            and not following.startswith(("}", ";", ",", ")", "else", "catch", "#"))
        ):
            output.append("\n")
    return "".join(output)
