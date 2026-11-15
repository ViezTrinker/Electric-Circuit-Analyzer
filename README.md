# Electric Circuit Analyzer

Desktop circuit analyzer for linear and nonlinear DC, AC sweeps, transient runs, and event-driven logic. The window executable is `eca_analyzer`. It places, moves, and rotates parts, draws orthogonal wires, edits the main value of a selected part, and saves a `.eca.json` document. DC draws node voltages, AC draws a Bode diagram, and a transient draws time curves plus a spectrum when a node voltage changes. Example circuits live in `examples/`. The library behind that window is covered by tests.

The program, its messages, and this documentation are in English.

## What you can call today

`Analyzer::SolveDc`, `Analyzer::SolveAc`, and `Analyzer::SolveTransient` take a `Circuit` and return voltages, branch values, and logic levels.

| Analysis | What it solves |
| --- | --- |
| DC | Modified nodal analysis. Resistors, sources, ideal meters, switches, diodes, LEDs, BJTs, MOSFETs, and the 555 latch. Newton-Raphson for the nonlinear devices. |
| AC | Small-signal sweep of linear R, L, and C around the DC operating point. The 555 latch stays at the DC state. |
| Transient | Backward Euler or trapezoidal integration for linear R, L, and C, with DC, sine, and pulse sources. The 555 latch updates from the trigger and threshold. A mixed run stops at the next logic event and updates the drive. |
| Logic | Gates, latches, flip-flops, clocks, counters, shift registers, decoders, and BCD-to-7-segment. A circuit with no analog parts runs as a pure event queue. |

A mixed DC operating point stamps each logic output that shares a net with an analog part as a voltage behind its output resistance, and reads analog voltages back through `Vil` and `Vih`. An AC sweep freezes that drive, so a gate has no small-signal gain. A mixed transient keeps that drive only until the next logic event. See [docs/architecture.md](docs/architecture.md).

## Build

The library is C++20. The window needs Qt 6 Widgets. This tree is built with Visual Studio 2026 (MSVC) and the MSVC kit of Qt. On Windows, CMake uses the newest `msvc2022_64` kit under `C:\Qt` when `CMAKE_PREFIX_PATH` is not set. Pass that prefix when Qt is installed somewhere else. Do not link the MSVC kit with the MSYS2 g++.

Visual Studio solution:

```bat
cmake -S . -B out\vs -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
```

Open `out\vs\electric_circuit_analyzer.slnx`. Debug and Release are the configuration dropdown. Build the solution, then start `eca_analyzer`. The executable is `out\vs\Debug\eca_analyzer.exe` or `out\vs\Release\eca_analyzer.exe`. A post-build step copies the Qt runtime next to that executable.

Command-line build from the x64 developer environment, with Ninja on `PATH`:

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build
ctest --test-dir build --output-on-failure
```

`eca_core` is a static library with no Qt dependency. `eca_tests` links that library and runs the analog and digital checks. `eca_analyzer` links the library and Qt Widgets. `src/main.cpp` only starts the application and the main window.

## Layout

```text
src/main.cpp    Starts the window
src/ui          Schematic sheet, part palette, properties, and diagrams
examples        Voltage divider, RC, RLC, rectifier, transistors, logic, 555
src/core        Circuit document, parts, nets, SI numbers, result codes
src/solver      Dense MNA, device equations, DC / AC / transient
src/digital     Event-driven logic kernel
tests           Checks with no GUI
third_party     nlohmann/json, reserved for the file format
docs            Design notes for the library
```

Headers use include guards, Doxygen on the public declarations, and the `Analyzer` namespace.

## License

This project is licensed under the [MIT License](LICENSE). The vendored JSON header in `third_party/nlohmann` keeps its own license notice.

## Further reading

- [Architecture](docs/architecture.md) describes the document, the netlist, and how an analysis runs.
- [Circuit models](docs/circuit_models.md) describes pins, stamps, and logic levels.

## Not in this tree yet

- A full property panel
- Pull-up and pull-down parts on a mixed analog net are not yet resistors to a supply
- Small-signal parameters of diodes and transistors in an AC sweep
- A full 74xx catalog, RAM, ROM, and FPGA fabrics
- VHDL, Verilog, SPICE import, noise, op-amps, and transformers
