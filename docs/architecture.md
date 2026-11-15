# Architecture

The analyzer is a library plus the window executable `eca_analyzer`. A schematic is a `Circuit`. Analysis compiles that document into a `Netlist`, then solves the analog matrix, the logic event queue, or both. The window in `src/ui` places parts, drags them to a new cell, turns them with R, and draws orthogonal wires. Wire vertices that sit on a moved or rotated pin follow that pin, and a diagonal segment gains a corner. The window edits one numeric value, supports undo and redo, and runs DC, AC, and transient on a worker thread. DC draws one bar per node voltage. AC draws magnitude and phase. A transient draws the changing node voltages or logic levels, and a spectrum when an analog voltage moves. The File menu opens the schematics in `examples/`. A full property panel stays outside that window for now.

```text
Circuit
  components, orthogonal wires, net labels, analysis settings
        |
        v
BuildNetlist
  one net per connected grid point, ground is net 0
        |
        +--> SolveDcAnalog / SolveAcAnalog / SolveTransientAnalog
        |
        +--> SolveDigitalDc / SolveDigitalTransient
```

`SolveDc` repeats the analog solve and the digital settle. A logic input on an analog net reads `Vil` and `Vih`. A logic output on an analog net is a voltage behind `outputResistanceOhm`. Pure digital nets stay out of the matrix. `SolveAc` freezes those drives: the sweep stamps the resistance and no small-signal path through the gate. `SolveTransient` uses the digital event queue alone when the netlist has no analog part. A mixed transient ends each analog step at the next logic event, then rebuilds the drives. With no capacitor, inductor, or time-varying source it jumps to that event and solves a new operating point.

## Document

`src/core/circuit.h` holds the editable schematic.

- A component has a kind, a name, a grid anchor, a rotation of 0, 90, 180, or 270 degrees, and a property variant.
- Wires are lists of grid points. A segment must be orthogonal. The builder rasterizes each segment, so a pin that lands on a segment joins that net.
- Net labels name the net under a grid point. The first name wins. A second different name is a warning.
- `UndoHistory` stores whole-circuit snapshots. The depth is 100. A new edit clears the redo stack.
- `ParseSiNumber` accepts forms such as `4.7k`, `10m`, `10u`, `1Meg`, and `1e-6`. `Meg` is mega. A bare `M` is also mega. `m` is milli.

Positive Y points down, matching a screen. `RotateOffset` turns a pin offset clockwise: 90 degrees maps `(x, y)` to `(-y, x)`.

Two-terminal parts span two grid cells. The anchor is pin 0. Pin 1 sits at local `(2, 0)` before rotation. Logic boxes are four cells wide. Input pins step down the left side by two cells. The output pin of a gate sits on the right at the first row.

Analog parts require a ground symbol. Every ground symbol is the same net, index 0. A pure logic circuit does not need ground. `KindIsAnalog` is false for gates, latches, flip-flops, clocks, decoders, counters, shift registers, pull-ups, pull-downs, and the logic probe.

Ideal shorts are not merged into one net. A closed ideal switch, an ideal ammeter, and a DC inductor with no series resistance become 0 V branches, so the branch current can be recovered. A loop of those branches returns `VoltageSourceLoop`.

## Result codes

`Result` is an `enum class` of `int8_t`. Negative values are errors, `Ok` is 0, and `Warning` is positive. Use `IsErr`, `IsOk`, and `IsMsg`. Descriptions are in `Describe`.

| Code | When it is returned |
| --- | --- |
| `MissingGround` | An analog device is present and no ground symbol exists. |
| `VoltageSourceLoop` | Voltage sources or ideal shorts close a loop. |
| `SingularMatrix` | Gaussian elimination finds no usable pivot. |
| `NoConvergence` | Newton or the transient step control gives up. |
| `Oscillation` | Logic is still changing after 1000 delta cycles. |
| `InvalidSweep` | The AC start or stop is not usable, or the start is above the stop. |
| `InvalidTimeStep` | The transient stop or maximum step is not positive. |
| `Warning` | The run finished, and a setup or hold check failed. |

## Analog solver

`src/solver/dense_system.h` is a row-major matrix templated on `double` and `std::complex<double>`. Factorization is Gaussian elimination with partial pivoting. There is no Eigen dependency.

Unknowns are the voltage of every net except ground, then one current per voltage-source branch. Net `k` (`k > 0`) is unknown `k - 1`. A global `gmin` of `1e-12 S` ties every analog node to ground so a floating capacitor still has a DC path.

The voltage-source current is the current leaving the positive terminal into the rest of the circuit. The stamp puts `-1` on the plus-node row, `+1` on the minus-node row, and the constraint `Vplus - Vminus = V`.

DC repeats that stamp inside Newton-Raphson, at most 80 times. A forward diode, LED, or base-junction step is limited to about two emission coefficients times the thermal voltage before it is accepted. A reverse step is not limited. Convergence requires a full step and a node change within `1e-6 + 1e-5`. A linear circuit finishes on the second iteration.

AC freezes the operating point and stamps capacitors as `jωC` and inductors as `1 / (R + jωL)`. A start frequency equal to the stop frequency is one point. A diode or LED adds its bias conductance, junction capacitance, and transit capacitance. A BJT adds the Ebers-Moll transconductances plus `Cje`, `Cjc`, and the transit capacitances. A MOSFET adds `gm`, `gds`, and the overlap capacitances `cgso*W` and `cgdo*W`. Junction capacitance below half the built-in potential is `Cj0 / (1 - V/Vj)^M`, and above that point it follows the tangent.

Transient uses a fixed maximum step. Backward Euler is the default. Trapezoidal integration is optional. The step shrinks when the matrix is singular and grows by 1.2 after an accepted step. Capacitor and inductor history uses the companion model. Initial values come from the operating point, or from the component fields when `InitialConditionMode::ComponentValues` is selected.

`CancelToken` is an atomic flag the solvers poll between iterations and steps.

## Logic kernel

`src/digital/logic_kernel.cpp` never puts a pure logic net into the analog matrix.

Each output contributes a strong high, a strong low, or nothing. A pull-up contributes a weak high, and a pull-down a weak low. Strong wins over weak. Two opposite strong drives, or two opposite pulls with no strong drive, resolve to Conflict. No drive at all is High-Z. Open-drain high is High-Z. Open-drain low is a strong low.

A DC settle updates one device at a time until the nets stop changing. Sequential parts start from `initialState` or `initialValue`. The clock output at time zero is the waveform value at `t = 0`, which is already high for a 50 percent clock with zero delay. That is not a rising edge. The first rising edge is one period later.

In transient, an output changes at `t + propagationDelay`. A delay of zero waits one delta cycle, so a flip-flop output cannot feed back in the same cycle. More than 1000 delta cycles at one timestamp returns `Oscillation`. Following gates read the new logic level when the output event commits, not at the end of the analog rise or fall.

Setup and hold, when non-zero, set Q to Conflict until the next valid edge and add a warning. They do not change an analog voltage.

## File format

`WriteCircuitText` and `ReadCircuitText` store one circuit as UTF-8 `.eca.json`. The format name is `electric-circuit-analyzer` and the version is 1. The file holds the analysis settings, parts, wires, and net labels. Enumerated fields are stored as their integer codes. A property the part editor would reject is left out, so every stored field can be applied on load. Switch event times and DIP channel states are extra keys beside those properties. An unknown version, an unknown component kind, or an unknown key rejects the whole text and leaves the destination circuit unchanged. The window opens and saves that text from the File menu.
