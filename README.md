# Street Rod 2: The Next Generation — SDL3 port

A faithful native port (C, SDL3) of California Dreams' *Street Rod 2: The Next Generation* (1991),
in the making. Not an emulator. It builds on the engine of the
[Street Rod port](https://github.com/kylofon/street-rod-sdl3), since both games share most of their
code. The original game files are not included: the port reads them from a `Game` folder at run
time.

Status and plan: [PLAN.md](PLAN.md) (as a page: `python tools/plan_site.py`, then open
`site/index.html`). Agent rules: [CLAUDE.md](CLAUDE.md).

## Layout

```text
Game\        the installed DOS game (not in git)
_tools\      junction to Ghidra + JDK (shared with the other ports, not in git)
tools\       reverse-engineering scripts (Python, Ghidra)
port\        RE guide, function indexes, SR1 matches, symbols, specs (port\spec), formats (port\formats)
docs\        manual PDFs/text (not in git)
work\        generated output and DOSBox reference captures (not in git)
site\        local HTML view of PLAN.md with token usage (not in git)
sr2port\     the port (from phase 4)
```

## License

MIT for the code in this repository (see [LICENSE](LICENSE)). Street Rod 2 is © California Dreams /
Logical Design Works.
