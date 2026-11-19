# AGENTS.md

This repository has strict formatting and style rules for AI-assisted edits.

## Required behavior

1. Do not add braces around single-statement bodies for control flow.
   - This rule applies to control-flow statements such as `if`, `else`, `for`, `foreach`, `while`, `do`, and `loop` when the body contains exactly one instruction.
   - Keep the body in the compact single-statement form instead of forcing a block with `{}`.
   - Do not rewrite valid single-line statements into multi-line brace blocks.

2. Do not add trailing comments at the end of files.
   - Do not append empty lines at the end of a file.
   - Do not add explanatory comments just to "caption" the file.
   - Preserve the final code state without any trailing comment noise.

3. Do not add unnecessary trailing blank lines at end of files.
   - Keep the file ending clean and minimal.
   - Only allow a final newline at the end of normal source files when needed by the language/tooling (not program file, buildsyseem).

4. Kconfig exception.
   - Kconfig files are allowed to end with a final newline and standard formatting line breaks where required by the Kconfig syntax.
   - In Kconfig files, do not aggressively collapse or rewrite required line breaks just to match normal C-style source formatting.
   - For Kconfig, preserve valid layout and final newline behavior as required by the build configuration tool.

## Prohibited AI edits

- Do not auto-insert `{}` around single statements in `if`, `else`, `for`, `foreach`, `while`, `do`, and `loop` constructs.
- Do not add line break to the end of source files.
- Do not add extra blank lines at the end of non-Kconfig files.
- Do not create or modify `.ini` or `.dnf` files as part of bootloader build automation.
- Do not invent config-generation logic that writes bootloader `.ini` or `.dnf` files.

## Preferred style

- Keep the existing formatting style unless a change is strictly required.
- Prefer minimal edits.
- Do not reformat unrelated code.
- If a control-flow statement has a single instruction, keep it single-line unless the surrounding style already requires otherwise.

## Scope

These rules apply to all repository files unless a file is explicitly a Kconfig file, where Kconfig newline and layout rules take precedence.