#!/usr/bin/env python3
"""Reject ambiguous PowerShell $name: interpolation in repository scripts.

In a double-quoted PowerShell string, `$name:` is parsed as a scoped/drive
variable reference. Ordinary variables followed by a literal colon must be
written as `${name}:`. Scope prefixes such as `$env:` are valid.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
VALID_SCOPES = {"env", "global", "script", "local", "private", "using"}
PATTERN = re.compile(r"\$([A-Za-z_][A-Za-z0-9_]*):")

failures: list[str] = []
for path in sorted(SCRIPTS.rglob("*.ps1")):
    text = path.read_text(encoding="utf-8-sig")
    for line_no, line in enumerate(text.splitlines(), 1):
        for match in PATTERN.finditer(line):
            if match.group(1).lower() not in VALID_SCOPES:
                rel = path.relative_to(ROOT)
                failures.append(
                    f"{rel}:{line_no}: ambiguous {match.group(0)!r}; "
                    f"use '${{{match.group(1)}}}:' when the colon is literal"
                )

if failures:
    print("POWERSHELL_INTERPOLATION_CHECK_FAIL")
    print("\n".join(failures))
    sys.exit(1)

print("POWERSHELL_INTERPOLATION_CHECK_PASS")
