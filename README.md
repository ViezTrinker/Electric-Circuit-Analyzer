# Electric Circuit Analyzer

Desktop circuit analyzer for linear and nonlinear DC, AC sweeps, transient runs, and event-driven logic. The core library is in place and covered by tests. The schematic window is not built yet.

The program, its messages, and this documentation are in English.

## What you can call today

`Analyzer::SolveDc`, `Analyzer::SolveAc`, and `Analyzer::SolveTransient` take a `Circuit` and return voltages, branch values, and logic levels.

| Analysis | What it solves |
| --- | --- |
| DC | Modified nodal analysis. Resistors, sources, ideal meters, switches, diodes, LEDs, BJTs, and MOSFETs. Newton-Raphson for the nonlinear devices. |
| AC | Small-signal sweep of linear R, L, and C around the DC operating point. |
| Transient | Backward Euler or trapezoidal integration for linear R, L, and C, with DC, sine, and pulse sources. |
| Logic | Gates, latches, flip-flops, clocks, counters, shift registers, decoders, and BCD-to-7-segment. A circuit with no analog parts runs as a pure event queue. |

A circuit that mixes analog parts and logic still solves the analog network. Logic outputs are not yet stamped back into that network, and analog voltages are not yet read as logic inputs. See [docs/architecture.md](docs/architecture.md).

## Build

The library is C++20. The tested toolchain is Visual Studio 2026 (MSVC) with Ninja. Configure from the x64 developer environment so `cl` is on `PATH`.

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

`eca_core` is a static library with no Qt dependency. `eca_tests` links that library and runs the analog and digital checks. Ninja must be on `PATH` (for example the Ninja shipped with MSYS2). Do not link this tree with the MSYS2 g++ if you later add the MSVC build of Qt.

## Layout

```text
src/core        Circuit document, parts, nets, SI numbers, result codes
src/solver      Dense MNA, device equations, DC / AC / transient
src/digital     Event-driven logic kernel
tests           Checks with no GUI
third_party     nlohmann/json, reserved for the file format
docs            Design notes for the library
```

Headers use include guards, Doxygen on the public declarations, and the `Analyzer` namespace. `main` does not exist yet. When the window is added it should only start the application and the main window.

## License

This project is licensed under the [MIT License](LICENSE). The vendored JSON header in `third_party/nlohmann` keeps its own license notice.

## Further reading

- [Architecture](docs/architecture.md) describes the document, the netlist, and how an analysis runs.
- [Circuit models](docs/circuit_models.md) describes pins, stamps, and logic levels.

## Not in this tree yet

- Schematic editor, property panel, and plots
- Saving and loading `.eca.json` (the JSON header is vendored, the reader and writer are not)
- Mixed-signal bridge: logic driving a resistor or LED, and analog voltages read by a gate, ADC, or 555
- Small-signal parameters of diodes and transistors in an AC sweep
- A full 74xx catalog, RAM, ROM, and FPGA fabrics
- VHDL, Verilog, SPICE import, noise, op-amps, and transformers
