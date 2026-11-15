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

The diode knee is not a hard clamp. `forwardVoltageVolt` is the junction voltage at `forwardCurrentAmpere`, default 0.7 V at 1 mA. Saturation current follows from that point and the emission coefficient. Series resistance, when greater than zero, sits outside the junction on an internal node. Positive current is anode to cathode. The LED uses the same junction and adds a color for the schematic. Transient repeats that conductance at every accepted step. The AC sweep stamps the same conductance plus junction and transit capacitance. Those capacitances are not in the transient stamp.

The BJT is an Ebers-Moll transport model. Pins are collector, base, emitter. Currents are into the device. An Early voltage of 0 means infinite Early voltage. Junction capacitances and transit times default to 0. The AC sweep stamps them on top of the transconductances, using a built-in potential of 0.7 V and a grading coefficient of 0.5. They are not part of the transient stamp. Newton limits each base junction step to two thermal voltages.

The MOSFET is Shichman-Hodges level 1 with the bulk tied to the source. Transient uses the same equations. The AC sweep stamps `gm` and `gds`, and overlap capacitors `cgso * W` and `cgdo * W`. Pins are drain, gate, source. `Id` is into the drain. PMOS uses the same equations on terminal voltages and threshold with the sign flipped, then flips `Id` back into the drain. The default test point `Vgs = 3 V`, `Vds = 5 V`, `Vto = 1 V`, `Kp = 2 mA/V²`, `W = L` gives `Id = 4 mA` when lambda is 0. The region boundary is smoothed over 1 mV. The exact saturation current away from that boundary is unchanged.

## Meters and switches

| Kind | Ideal | Non-ideal |
| --- | --- | --- |
| Ammeter, pin 0 = A, pin 1 = B | 0 V branch. Positive current is A to B. | Burden resistance. Burden inductance is stored. |
| Voltmeter, pin 0 = plus, pin 1 = minus | No stamp. | Input resistance. Input capacitance is stored. |
| SPST and pushbutton | Closed is a 0 V branch when the pins differ. Open stamps nothing. | Closed and open resistances. An open resistance of 0 is rejected. |

`MeterMode` and `ResistanceMode` are enums, not booleans. The pushbutton contact is normally open or normally closed. The rest state is the DC state. The event list is applied only in transient. At DC the 7-segment display is one diode per segment, the comparator pulls its output to VCC or GND, the relay coil is a resistor and the contact follows pull-in and drop-out, the DAC output is `code / 15 * Vref`, and the ADC code is `floor(Vin / Vref * 16)` clamped to 0..15.

An SPDT switch joins COM to throw A or throw B. The other throw uses the open resistance when that mode is resistive. A DIP switch is four independent SPST channels. A closed ideal contact is a 0 V branch.

The 555 pins are VCC, GND, TRIG, THRES, DISCH, OUT, and RESET. OUT is a resistor of `outputResistanceOhm` to VCC when the latch is high and to GND when it is low. DISCH uses the same resistance to GND while the output is low, and is open while the output is high. RESET below 0.4 of the supply forces the output low. Otherwise a trigger below one third of the supply sets the latch, and a threshold above two thirds clears it. If both are quiet the latch holds. DC repeats that decision until the voltages and the latch agree. If they never agree, DC keeps the initial output-low state and solves that circuit. Small-signal AC freezes the DC latch. Transient applies the same decision after `propagationDelaySec`. The astable high time is about `0.693 (R1 + R2) C`. The low time uses the Thevenin resistance of R2, R1, and the discharge resistance.

## Logic levels

A logic input stores nothing while the net is purely digital. High-Z on an input is Conflict. A low input still forces AND low and NAND high, and a high input still forces OR high and NOR low. Any other non-binary input makes that gate output Conflict. A disabled tri-state output is High-Z, and an open-drain high is also High-Z.

CMOS thresholds scale as 0.7 and 0.3 of the supply. TTL stays at Vol 0.2 V, Voh 3.4 V, Vil 0.8 V, and Vih 2.0 V until the family is Custom. The default family is CMOS 5 V. The default output resistance is 100 ohm, the delay is 10 ns, and the rise and fall times are 5 ns. Gate supply current is not modeled.

On a mixed net the DC solver stamps a high output as `vohVolt` and a low output as `volVolt`, each behind `outputResistanceOhm`. High-Z and Conflict add no drive. An output resistance of 0 is stamped as 1 mΩ. The AC sweep keeps that resistance to ground and contributes no controlled source, so the gate does not amplify. Pull-up and pull-down parts on a mixed net are not yet resistors to the supply.

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

These are the checks in `tests/test_analysis.cpp`, `tests/test_digital.cpp`, and `tests/test_mixed.cpp`.

- A 10 V source and two 1 kΩ resistors divide to 5 V and 5 mA.
- A 1 mA source into the default diode develops 0.7 V.
- A 1 kΩ and 1 µF low-pass has gain `1/sqrt(2)` at `1/(2πRC)`.
- The same RC, stepped to 1 V with trapezoidal steps of 10 µs, is near `1 - exp(-1)` at 1 ms.
- The NMOS point above draws 4 mA from the drain supply. With the gate at 0 V that current is below 1 nA.
- A 1 mA diode bias has the small-signal resistance `n Vt / I`. A 1 µF junction capacitance attenuates that voltage at 100 kHz.
- An NPN biased at the 0.7 V knee into 1 kΩ has collector gain `gm Rc`. The same NMOS, with 1 kΩ in the drain and a 10 V supply, has gain `gm Rd`.
- NAND and XOR match the binary truth table. A floating NAND input is Conflict. One input held low forces a NAND output high.
- An open-drain high plus a pull-up is high. Two opposite push-pull outputs are Conflict.
- An inverter with 10 ns delay follows a clock edge 10 ns later.
- A 1 kHz D flip-flop with D held high goes high one period plus 10 ns after `t = 0`.
- The counter and the shift register advance once per rising edge.
- BCD codes 0, 1, and 8 produce the usual glyphs. Code 10 is blank.
- An inverter input tied to ground drives 5 V through 100 Ω into 1 kΩ, so the output node is 4.545 V. The same node has no AC gain.
- A 1 kHz clock, 220 Ω, and an LED conduct only while the clock output is high. The transient step ends at each clock edge.
- A 5 V source, 220 Ω, and the default LED conduct. An ideal switch across the lower resistor of a divider shorts that node. A 100 Ω closed switch does not. A closed switch that opens at 1 ms releases that node in the transient.
- A common-cathode segment tied to 5 V draws current through its 330 Ω resistor.
- A comparator with IN+ above IN- pulls the output toward VCC. A relay coil above pull-in closes the normally open contact.
- DAC code 1 with a 15 V reference produces 1 V behind the output resistance. ADC code for 5 V on an 8 V reference is 10.
- An SPDT throw and one closed DIP channel short the bottom of a divider. The other SPDT throw leaves that node at the source voltage.
- A 555 with trigger and threshold at ground drives its output high. With both pins at the supply, the output stays low.
- A 555 astable with 10 kΩ, 10 kΩ, and 100 nF has the documented charge and discharge period.
