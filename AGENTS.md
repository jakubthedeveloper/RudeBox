# Architecture Guide

- Structure the code for top-down reading. High-level entry points should describe the application or processing flow using concise, domain-level operations.
- Increase the level of detail gradually across layers. A reader should be able to understand the overall flow first and inspect lower-level functions only when more detail is needed.
- Keep orchestration separate from implementation details. Do not mix the main control flow with calculations, hardware access, value conversion, logging, or other low-level operations.
- Give every module or class a clear responsibility. Place reusable or domain-independent behavior in the module that owns that concept, rather than in whichever caller currently uses it.
- Extract focused private methods or dedicated helpers when they make calling code shorter and reveal its intent. Prefer meaningful domain names over comments that explain dense procedural code.
- Encapsulate user-interface and hardware interaction behind dedicated abstractions. Application-level code should coordinate these components without knowing how controls are read, outputs are driven, or diagnostics are produced.
- Preserve these boundaries when adding or changing features. Extend the appropriate layer instead of introducing shortcuts that leak lower-level details into higher-level code.
- After every change, review `README.md` and update it whenever the behavior, configuration, architecture, setup, or usage has changed.
- Keep Markdown paragraphs and list items on a single source line. Do not insert hard line breaks in the middle of sentences or wrap prose to a fixed column width.
- Write all source code, identifiers, comments, and documentation in English.

## Filesystem boundaries

Do not inspect, search, read, modify, or enumerate files outside this repository
unless I explicitly ask you to do so.

In particular, do not access:
- ~/Desktop
- ~/Documents
- ~/Downloads
- other repositories
- arbitrary files in my home directory

If a file outside the repository appears relevant, ask me for permission first.