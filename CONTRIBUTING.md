# Contributing

Thanks for looking. Bug reports, logs from machines that are not the one this
was developed on, and fixes are all welcome.

## Reporting a problem

Most tray problems depend on which applications are involved and on the exact
Windows build, so please include:

* your Windows version and build (`winver`) and your Windhawk version;
* how many displays you have and how they are arranged;
* the applications whose icons misbehave;
* the mod's log. Turn on logging for the mod in Windhawk, or capture it with
  `build\dbgcapture.exe --filter split-tray` (see
  [docs/development.md](docs/development.md)), then reproduce the problem.

## Changing the code

[docs/development.md](docs/development.md) covers building and testing, and
[docs/architecture.md](docs/architecture.md) covers how the pieces fit
together. A few conventions keep the project workable:

* **One file.** Windhawk compiles a mod from a single translation unit, so the
  mod stays in `src/split-tray.wh.cpp`, in its numbered sections. Sections 1
  to 4 have no side effects and are tested directly.
* **Tests with every change.** A bug fix starts with a test that fails on the
  old code, and a change that alters behaviour names the tests that cover it.
  `tools\build.ps1` must pass. `python tools/mutate.py` must kill every mutant.
  If you add behaviour the tests would not notice losing, add a mutant for it.
* **Evidence before assumptions.** Explorer's internals are undocumented. The
  wire format, the symbols and the XAML structure are all checked against what
  a real system does: the wire probe, `tools/check-symbols.py`, and the mod's
  own log. New assumptions about Explorer should be checked the same way.
* **Nothing slow on Explorer's thread.** Every application's tray messages are
  handled there.
* **Records.** Add an entry at the top of [CHANGES.md](CHANGES.md) for a
  functional change. It covers what was wrong, what changed, and how it was
  verified, including anything that was not. A new standing design decision
  goes in [DECISIONS.md](DECISIONS.md).

## License

By contributing you agree that your contribution is licensed under the
project's [MIT license](LICENSE).
