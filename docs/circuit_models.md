# Circuit models

Parts live in `src/core/component.h`. The analog stamps are in `src/solver/analog_solver.cpp` and `src/solver/device_model.cpp`. Logic behavior is in `src/digital/logic_kernel.cpp`.

Currents into a device are positive unless a section below says otherwise. Branch current for a resistor is `(Vpin0 - Vpin1) / R`.

## Sources and passives

| Kind | Pins | DC behavior |
| --- | --- | --- |
| Voltage source | 0 minus, 1 plus | DC value, or the sine or pulse waveform in transient. AC amplitude is the small-signal source. An AC amplitude of 0 is a short. |
| Current source | 0 tail, 1 head | Current flows from tail to head. An AC amplitude of 0 is an open. |
| Resistor | 0, 1 | Conductance `1/R`. |
| Potentiometer | A, wiper, B | Two resistors split by the wiper fraction. Each section stays at least `1e-6` of the total. |
| Capacitor | 0, 1 | Open at DC. Transient companion uses `C/h` or `2C/h`. Optional ESR is stored and not yet in the transient stamp. |
| Inductor | 0, 1 | Short at DC when series R is 0, otherwise a resistor. Backward Euler uses `h/L`. Trapezoidal currently stamps `2h/L`. |
| Ground | 1 | Net 0. |

The diode knee is not a hard clamp. `forwardVoltageVolt` is the junction voltage at `forwardCurrentAmpere`, default 0.7 V at 1 mA. Saturation current follows from that point and the emission coefficient. Series resistance, when greater than zero, sits outside the junction on an internal node. Positive current is anode to cathode. The LED uses the same junction and adds a color for the schematic.

The BJT is an Ebers-Moll transport model. Pins are collector, base, emitter. Currents are into the device. An Early voltage of 0 means infinite Early voltage. Junction capacitances and transit times default to 0.

The MOSFET is Shichman-Hodges level 1 with the bulk tied to the source. Pins are drain, gate, source. `Id` is into the drain. PMOS uses the same equations on terminal voltages and threshold with the sign flipped, then flips `Id` back into the drain. The default test point `Vgs = 3 V`, `Vds = 5 V`, `Vto = 1 V`, `Kp = 2 mA/V²`, `W = L` gives `Id = 4 mA` when lambda is 0. The region boundary is smoothed over 1 mV. The exact saturation current away from that boundary is unchanged.

## Meters and switches

| Kind | Ideal | Non-ideal |
| --- | --- | --- |
| Ammeter, pin 0 = A, pin 1 = B | 0 V branch. Positive current is A to B. | Burden resistance. Burden inductance is stored. |
| Voltmeter, pin 0 = plus, pin 1 = minus | No stamp. | Input resistance. Input capacitance is stored. |
| SPST and pushbutton | Closed is a 0 V branch when the pins differ. Open stamps nothing. | Closed and open resistances. An open resistance of 0 is rejected. |

`MeterMode` and `ResistanceMode` are enums, not booleans. The pushbutton contact is normally open or normally closed. The rest state is the DC state. The event list is applied only in transient. SPDT, DIP switches, the relay, the 555, the comparator, the 7-segment display, the ADC, and the DAC are in the part catalog. Their analog stamps are not implemented yet.

## Logic levels

A logic input stores nothing while the net is purely digital. High-Z on an input is Conflict, and the gate output becomes Conflict. The exception is a disabled tri-state output, which is High-Z, and an open-drain high, which is also High-Z.

CMOS thresholds scale as 0.7 and 0.3 of the supply. TTL stays at Vol 0.2 V, Voh 3.4 V, Vil 0.8 V, and Vih 2.0 V until the family is Custom. The default family is CMOS 5 V. The default output resistance is 100 ohm, the delay is 10 ns, and the rise and fall times are 5 ns. Gate supply current is not modeled.

| Kind | Pins, rotation 0 |
| --- | --- |
| Buffer, inverter, Schmitt inverter | Input, output |
| AND, OR, NAND, NOR | Inputs `0 .. n-2`, output last. `n` is 2 to 8. |
| XOR, XNOR | A, B, output |
| Tri-state buffer | Data, enable, output |
| SR latch | S, R, Q, Qbar |
| D latch | D, enable, Q, Qbar |
| D and T flip-flop | D or T, clock, Q, Qbar, preset, clear |
| JK flip-flop | J, K, clock, Q, Qbar, preset, clear |
| Clock | Y. For `t < delay`, the level is `startLevel`. After that the output is high while the position in the period is below `duty * period`. |
| Multiplexer | Data inputs, then selects, then Y. Width is 2:1 or 4:1. |
| Decoder | A, B, enable, Y0 to Y3 |
| BCD to 7-segment | D0 to D3, then segments a to g. Codes 10 to 15 are blank. |
| Full adder | A, B, Cin, Sum, Cout |
| 4-bit counter | Clock, clear, enable, Q0 to Q3, carry. Carry is high when the count is 15. |
| 4-bit shift register | Clock, clear, serial in, Q0 to Q3. The shift moves toward Q3. Serial data enters Q0. |
| Pull-up, pull-down | One pin. Weak high or weak low on a pure logic net. |

Preset, clear, set, reset, and enable each have an active-high or active-low polarity. Both preset and clear active, or both set and reset active, produce Conflict. J = K = high toggles. T high toggles and T low holds. Clock-to-Q is at least one delta even when the delay is 0.

## Defaults used by the tests

These are the checks in `tests/test_analysis.cpp` and `tests/test_digital.cpp`.

- A 10 V source and two 1 kΩ resistors divide to 5 V and 5 mA.
- A 1 mA source into the default diode develops 0.7 V.
- A 1 kΩ and 1 µF low-pass has gain `1/sqrt(2)` at `1/(2πRC)`.
- The same RC, stepped to 1 V with trapezoidal steps of 10 µs, is near `1 - exp(-1)` at 1 ms.
- The NMOS point above draws 4 mA from the drain supply.
- NAND and XOR match the binary truth table. A floating NAND input is Conflict.
- An open-drain high plus a pull-up is high. Two opposite push-pull outputs are Conflict.
- An inverter with 10 ns delay follows a clock edge 10 ns later.
- A 1 kHz D flip-flop with D held high goes high one period plus 10 ns after `t = 0`.
- The counter and the shift register advance once per rising edge.
- BCD codes 0, 1, and 8 produce the usual glyphs. Code 10 is blank.
