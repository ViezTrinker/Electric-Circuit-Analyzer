/*!
 * \file component_catalog.cpp
 * \brief Kind names, pin geometry, and default component parameters.
 */

#include "component.h"

#include <cmath>
#include <utility>

namespace Analyzer
{

namespace
{

GridPoint At(int32_t positionX, int32_t positionY)
{
   return GridPoint{positionX, positionY};
}

LogicProperties MakeLogic(uint8_t inputCount)
{
   LogicProperties logic = {};
   logic.family = LogicFamily::Cmos5V;
   logic.supplyVoltageVolt = DefaultSupplyVoltageVolt;
   logic.propagationDelaySec = DefaultPropagationDelaySec;
   logic.riseTimeSec = DefaultEdgeTimeSec;
   logic.fallTimeSec = DefaultEdgeTimeSec;
   logic.outputResistanceOhm = DefaultOutputResistanceOhm;
   logic.outputStage = OutputStage::PushPull;
   logic.inputCount = inputCount;
   logic.enablePolarity = Polarity::ActiveHigh;
   logic.lowThresholdVolt = CmosLowFraction * DefaultSupplyVoltageVolt;
   logic.highThresholdVolt = CmosHighFraction * DefaultSupplyVoltageVolt;
   ApplyLogicFamily(&logic, FamilySupplyReset::FamilyDefault);
   return logic;
}

DiodeProperties MakeDiode(void)
{
   DiodeProperties diode = {};
   diode.forwardVoltageVolt = DefaultForwardVoltageVolt;
   diode.forwardCurrentAmpere = DefaultForwardCurrentAmpere;
   diode.emissionCoefficient = DefaultEmissionCoefficient;
   diode.seriesResistanceOhm = 0.0;
   diode.temperatureCelsius = DefaultTemperatureCelsius;
   diode.junctionCapacitanceFarad = 0.0;
   diode.junctionPotentialVolt = DefaultJunctionPotentialVolt;
   diode.gradingCoefficient = DefaultGradingCoefficient;
   diode.transitTimeSec = 0.0;
   return diode;
}

VoltageSourceProperties MakeVoltageSource(void)
{
   VoltageSourceProperties source = {};
   source.dcVoltageVolt = DefaultSourceVoltageVolt;
   source.acAmplitudeVolt = 0.0;
   source.acPhaseDeg = 0.0;
   source.transientWaveform = SourceWaveform::Dc;
   source.sineOffsetVolt = 0.0;
   source.sineAmplitudeVolt = DefaultSourceVoltageVolt;
   source.sineFrequencyHz = 1000.0;
   source.sinePhaseDeg = 0.0;
   source.sineDelaySec = 0.0;
   source.pulseInitialVolt = 0.0;
   source.pulseLevelVolt = DefaultSourceVoltageVolt;
   source.pulseDelaySec = 0.0;
   source.pulseRiseSec = 1.0e-9;
   source.pulseFallSec = 1.0e-9;
   source.pulseWidthSec = 0.0005;
   source.pulsePeriodSec = 0.001;
   return source;
}

CurrentSourceProperties MakeCurrentSource(void)
{
   CurrentSourceProperties source = {};
   source.dcCurrentAmpere = DefaultSourceCurrentAmpere;
   source.acAmplitudeAmpere = 0.0;
   source.acPhaseDeg = 0.0;
   source.transientWaveform = SourceWaveform::Dc;
   source.sineOffsetAmpere = 0.0;
   source.sineAmplitudeAmpere = DefaultSourceCurrentAmpere;
   source.sineFrequencyHz = 1000.0;
   source.sinePhaseDeg = 0.0;
   source.sineDelaySec = 0.0;
   source.pulseInitialAmpere = 0.0;
   source.pulseLevelAmpere = DefaultSourceCurrentAmpere;
   source.pulseDelaySec = 0.0;
   source.pulseRiseSec = 1.0e-9;
   source.pulseFallSec = 1.0e-9;
   source.pulseWidthSec = 0.0005;
   source.pulsePeriodSec = 0.001;
   return source;
}

SequentialProperties MakeSequential(void)
{
   SequentialProperties sequential = {};
   sequential.logic = MakeLogic(1);
   sequential.initialState = LogicLevel::Low;
   sequential.clockEdge = ClockEdge::Rising;
   sequential.presetPolarity = Polarity::ActiveHigh;
   sequential.clearPolarity = Polarity::ActiveHigh;
   sequential.srPolarity = Polarity::ActiveHigh;
   sequential.enablePolarity = Polarity::ActiveHigh;
   sequential.setupTimeSec = 0.0;
   sequential.holdTimeSec = 0.0;
   return sequential;
}

bool IsTwoTerminal(ComponentKind kind)
{
   switch (kind)
   {
      case ComponentKind::Resistor:
      case ComponentKind::Capacitor:
      case ComponentKind::Inductor:
      case ComponentKind::VoltageSource:
      case ComponentKind::CurrentSource:
      case ComponentKind::Diode:
      case ComponentKind::Led:
      case ComponentKind::Ammeter:
      case ComponentKind::Voltmeter:
      case ComponentKind::SpstSwitch:
      case ComponentKind::Pushbutton:
      {
         return true;
      }
      default:
      {
         break;
      }
   }
   return false;
}

GridPoint LeftPin(uint8_t row)
{
   return At(0, static_cast<int32_t>(row) * PinSpacing);
}

GridPoint RightPin(uint8_t row)
{
   return At(BoxWidth, static_cast<int32_t>(row) * PinSpacing);
}

} // namespace

void ApplyLogicFamily(LogicProperties* pLogic, FamilySupplyReset resetSupply)
{
   if (pLogic == nullptr)
   {
      return;
   }
   if (pLogic->family == LogicFamily::Custom)
   {
      return;
   }
   if (resetSupply == FamilySupplyReset::FamilyDefault)
   {
      if (pLogic->family == LogicFamily::Cmos3V3)
      {
         pLogic->supplyVoltageVolt = 3.3;
      }
      else
      {
         pLogic->supplyVoltageVolt = DefaultSupplyVoltageVolt;
      }
   }
   if (pLogic->family == LogicFamily::Ttl)
   {
      pLogic->volVolt = TtlVolVolt;
      pLogic->vohVolt = TtlVohVolt;
      pLogic->vilVolt = TtlVilVolt;
      pLogic->vihVolt = TtlVihVolt;
      return;
   }
   pLogic->volVolt = 0.0;
   pLogic->vohVolt = pLogic->supplyVoltageVolt;
   pLogic->vilVolt = CmosLowFraction * pLogic->supplyVoltageVolt;
   pLogic->vihVolt = CmosHighFraction * pLogic->supplyVoltageVolt;
}

double ThermalVoltage(double temperatureCelsius)
{
   double kelvin = temperatureCelsius + ZeroCelsiusInKelvin;
   return BoltzmannJoulePerKelvin * kelvin / ElectronChargeCoulomb;
}

double SaturationCurrent(double forwardVoltageVolt, double forwardCurrentAmpere, double emissionCoefficient, double temperatureCelsius)
{
   double thermal = emissionCoefficient * ThermalVoltage(temperatureCelsius);
   if (thermal <= 0.0 || forwardCurrentAmpere <= 0.0)
   {
      return 0.0;
   }
   double exponent = forwardVoltageVolt / thermal;
   if (exponent > 80.0)
   {
      exponent = 80.0;
   }
   return forwardCurrentAmpere / (std::exp(exponent) - 1.0);
}

std::string KindKey(ComponentKind kind)
{
   switch (kind)
   {
      case ComponentKind::Resistor: return "resistor";
      case ComponentKind::Capacitor: return "capacitor";
      case ComponentKind::Inductor: return "inductor";
      case ComponentKind::Potentiometer: return "potentiometer";
      case ComponentKind::VoltageSource: return "voltageSource";
      case ComponentKind::CurrentSource: return "currentSource";
      case ComponentKind::Diode: return "diode";
      case ComponentKind::Led: return "led";
      case ComponentKind::Npn: return "npn";
      case ComponentKind::Pnp: return "pnp";
      case ComponentKind::Nmos: return "nmos";
      case ComponentKind::Pmos: return "pmos";
      case ComponentKind::Relay: return "relay";
      case ComponentKind::Timer555: return "timer555";
      case ComponentKind::Comparator: return "comparator";
      case ComponentKind::Ground: return "ground";
      case ComponentKind::SpstSwitch: return "spstSwitch";
      case ComponentKind::SpdtSwitch: return "spdtSwitch";
      case ComponentKind::Pushbutton: return "pushbutton";
      case ComponentKind::DipSwitch: return "dipSwitch";
      case ComponentKind::SevenSegment: return "sevenSegment";
      case ComponentKind::Ammeter: return "ammeter";
      case ComponentKind::Voltmeter: return "voltmeter";
      case ComponentKind::LogicProbe: return "logicProbe";
      case ComponentKind::Buffer: return "buffer";
      case ComponentKind::Inverter: return "inverter";
      case ComponentKind::SchmittInverter: return "schmittInverter";
      case ComponentKind::AndGate: return "andGate";
      case ComponentKind::OrGate: return "orGate";
      case ComponentKind::NandGate: return "nandGate";
      case ComponentKind::NorGate: return "norGate";
      case ComponentKind::XorGate: return "xorGate";
      case ComponentKind::XnorGate: return "xnorGate";
      case ComponentKind::TriStateBuffer: return "triStateBuffer";
      case ComponentKind::SrLatch: return "srLatch";
      case ComponentKind::DLatch: return "dLatch";
      case ComponentKind::DFlipFlop: return "dFlipFlop";
      case ComponentKind::JkFlipFlop: return "jkFlipFlop";
      case ComponentKind::TFlipFlop: return "tFlipFlop";
      case ComponentKind::Clock: return "clock";
      case ComponentKind::Multiplexer: return "multiplexer";
      case ComponentKind::Decoder: return "decoder";
      case ComponentKind::BcdToSevenSegment: return "bcdToSevenSegment";
      case ComponentKind::FullAdder: return "fullAdder";
      case ComponentKind::Counter: return "counter";
      case ComponentKind::ShiftRegister: return "shiftRegister";
      case ComponentKind::PullUp: return "pullUp";
      case ComponentKind::PullDown: return "pullDown";
      case ComponentKind::Adc: return "adc";
      case ComponentKind::Dac: return "dac";
      default: break;
   }
   return "unknown";
}

std::string KindLabel(ComponentKind kind)
{
   switch (kind)
   {
      case ComponentKind::Resistor: return "Resistor";
      case ComponentKind::Capacitor: return "Capacitor";
      case ComponentKind::Inductor: return "Inductor";
      case ComponentKind::Potentiometer: return "Potentiometer";
      case ComponentKind::VoltageSource: return "Voltage Source";
      case ComponentKind::CurrentSource: return "Current Source";
      case ComponentKind::Diode: return "Diode";
      case ComponentKind::Led: return "LED";
      case ComponentKind::Npn: return "NPN";
      case ComponentKind::Pnp: return "PNP";
      case ComponentKind::Nmos: return "NMOS";
      case ComponentKind::Pmos: return "PMOS";
      case ComponentKind::Relay: return "Relay";
      case ComponentKind::Timer555: return "Timer 555";
      case ComponentKind::Comparator: return "Comparator";
      case ComponentKind::Ground: return "Ground";
      case ComponentKind::SpstSwitch: return "SPST Switch";
      case ComponentKind::SpdtSwitch: return "SPDT Switch";
      case ComponentKind::Pushbutton: return "Pushbutton";
      case ComponentKind::DipSwitch: return "DIP Switch";
      case ComponentKind::SevenSegment: return "7-Segment";
      case ComponentKind::Ammeter: return "Ammeter";
      case ComponentKind::Voltmeter: return "Voltmeter";
      case ComponentKind::LogicProbe: return "Logic Probe";
      case ComponentKind::Buffer: return "Buffer";
      case ComponentKind::Inverter: return "Inverter";
      case ComponentKind::SchmittInverter: return "Schmitt Inverter";
      case ComponentKind::AndGate: return "AND";
      case ComponentKind::OrGate: return "OR";
      case ComponentKind::NandGate: return "NAND";
      case ComponentKind::NorGate: return "NOR";
      case ComponentKind::XorGate: return "XOR";
      case ComponentKind::XnorGate: return "XNOR";
      case ComponentKind::TriStateBuffer: return "Tri-state Buffer";
      case ComponentKind::SrLatch: return "SR Latch";
      case ComponentKind::DLatch: return "D Latch";
      case ComponentKind::DFlipFlop: return "D Flip-Flop";
      case ComponentKind::JkFlipFlop: return "JK Flip-Flop";
      case ComponentKind::TFlipFlop: return "T Flip-Flop";
      case ComponentKind::Clock: return "Clock";
      case ComponentKind::Multiplexer: return "Multiplexer";
      case ComponentKind::Decoder: return "2-to-4 Decoder";
      case ComponentKind::BcdToSevenSegment: return "BCD to 7-Segment";
      case ComponentKind::FullAdder: return "Full Adder";
      case ComponentKind::Counter: return "4-bit Counter";
      case ComponentKind::ShiftRegister: return "4-bit Shift Register";
      case ComponentKind::PullUp: return "Pull-up";
      case ComponentKind::PullDown: return "Pull-down";
      case ComponentKind::Adc: return "ADC";
      case ComponentKind::Dac: return "DAC";
      default: break;
   }
   return "Unknown";
}

Result KindFromKey(std::string_view key, ComponentKind* pKind)
{
   if (pKind == nullptr)
   {
      return Result::InvalidParameter;
   }
   for (int32_t code = 0; code <= static_cast<int32_t>(ComponentKind::Dac); code += 1)
   {
      auto kind = static_cast<ComponentKind>(code);
      if (KindKey(kind) == key)
      {
         *pKind = kind;
         return Result::Ok;
      }
   }
   return Result::UnknownKind;
}

std::string KindPrefix(ComponentKind kind)
{
   switch (kind)
   {
      case ComponentKind::Resistor: return "R";
      case ComponentKind::Capacitor: return "C";
      case ComponentKind::Inductor: return "L";
      case ComponentKind::Potentiometer: return "RV";
      case ComponentKind::VoltageSource: return "V";
      case ComponentKind::CurrentSource: return "I";
      case ComponentKind::Diode: return "D";
      case ComponentKind::Led: return "LED";
      case ComponentKind::Npn:
      case ComponentKind::Pnp: return "Q";
      case ComponentKind::Nmos:
      case ComponentKind::Pmos: return "M";
      case ComponentKind::Relay: return "K";
      case ComponentKind::Timer555:
      case ComponentKind::AndGate:
      case ComponentKind::OrGate:
      case ComponentKind::NandGate:
      case ComponentKind::NorGate:
      case ComponentKind::XorGate:
      case ComponentKind::XnorGate:
      case ComponentKind::Buffer:
      case ComponentKind::Inverter:
      case ComponentKind::SchmittInverter:
      case ComponentKind::TriStateBuffer:
      case ComponentKind::SrLatch:
      case ComponentKind::DLatch:
      case ComponentKind::DFlipFlop:
      case ComponentKind::JkFlipFlop:
      case ComponentKind::TFlipFlop:
      case ComponentKind::Multiplexer:
      case ComponentKind::Decoder:
      case ComponentKind::BcdToSevenSegment:
      case ComponentKind::FullAdder:
      case ComponentKind::Counter:
      case ComponentKind::ShiftRegister:
      case ComponentKind::Adc:
      case ComponentKind::Dac: return "U";
      case ComponentKind::Comparator: return "CMP";
      case ComponentKind::Ground: return "GND";
      case ComponentKind::SpstSwitch:
      case ComponentKind::SpdtSwitch:
      case ComponentKind::Pushbutton:
      case ComponentKind::DipSwitch: return "SW";
      case ComponentKind::SevenSegment: return "DISP";
      case ComponentKind::Ammeter: return "AM";
      case ComponentKind::Voltmeter: return "VM";
      case ComponentKind::LogicProbe: return "PR";
      case ComponentKind::Clock: return "CLK";
      case ComponentKind::PullUp: return "PU";
      case ComponentKind::PullDown: return "PD";
      default: break;
   }
   return "X";
}

bool KindIsAnalog(ComponentKind kind)
{
   switch (kind)
   {
      case ComponentKind::Buffer:
      case ComponentKind::Inverter:
      case ComponentKind::SchmittInverter:
      case ComponentKind::AndGate:
      case ComponentKind::OrGate:
      case ComponentKind::NandGate:
      case ComponentKind::NorGate:
      case ComponentKind::XorGate:
      case ComponentKind::XnorGate:
      case ComponentKind::TriStateBuffer:
      case ComponentKind::SrLatch:
      case ComponentKind::DLatch:
      case ComponentKind::DFlipFlop:
      case ComponentKind::JkFlipFlop:
      case ComponentKind::TFlipFlop:
      case ComponentKind::Clock:
      case ComponentKind::Multiplexer:
      case ComponentKind::Decoder:
      case ComponentKind::BcdToSevenSegment:
      case ComponentKind::FullAdder:
      case ComponentKind::Counter:
      case ComponentKind::ShiftRegister:
      case ComponentKind::PullUp:
      case ComponentKind::PullDown:
      case ComponentKind::LogicProbe:
      {
         return false;
      }
      default:
      {
         break;
      }
   }
   return true;
}

uint8_t PinCount(const Component& component)
{
   switch (component.kind)
   {
      case ComponentKind::Ground:
      case ComponentKind::LogicProbe:
      case ComponentKind::PullUp:
      case ComponentKind::PullDown:
      case ComponentKind::Clock:
      {
         return 1;
      }
      case ComponentKind::Potentiometer:
      case ComponentKind::Npn:
      case ComponentKind::Pnp:
      case ComponentKind::Nmos:
      case ComponentKind::Pmos:
      case ComponentKind::SpdtSwitch:
      case ComponentKind::TriStateBuffer:
      {
         return 3;
      }
      case ComponentKind::SrLatch:
      case ComponentKind::DLatch:
      {
         return 4;
      }
      case ComponentKind::Relay:
      case ComponentKind::Comparator:
      case ComponentKind::FullAdder:
      {
         return 5;
      }
      case ComponentKind::DFlipFlop:
      case ComponentKind::TFlipFlop:
      case ComponentKind::Dac:
      {
         return 6;
      }
      case ComponentKind::Timer555:
      case ComponentKind::JkFlipFlop:
      case ComponentKind::Decoder:
      case ComponentKind::ShiftRegister:
      case ComponentKind::Adc:
      {
         return 7;
      }
      case ComponentKind::Counter:
      case ComponentKind::DipSwitch:
      {
         return 8;
      }
      case ComponentKind::SevenSegment:
      {
         return 9;
      }
      case ComponentKind::BcdToSevenSegment:
      {
         return 11;
      }
      case ComponentKind::Multiplexer:
      {
         if (std::holds_alternative<MuxProperties>(component.properties)
            && std::get<MuxProperties>(component.properties).width == MuxWidth::Four)
         {
            return 7;
         }
         return 4;
      }
      case ComponentKind::XorGate:
      case ComponentKind::XnorGate:
      {
         return 3;
      }
      case ComponentKind::AndGate:
      case ComponentKind::OrGate:
      case ComponentKind::NandGate:
      case ComponentKind::NorGate:
      {
         if (std::holds_alternative<LogicProperties>(component.properties))
         {
            uint8_t inputs = std::get<LogicProperties>(component.properties).inputCount;
            if (inputs < 2)
            {
               inputs = 2;
            }
            return static_cast<uint8_t>(inputs + 1);
         }
         return 3;
      }
      default:
      {
         break;
      }
   }
   return 2;
}

std::string PinName(const Component& component, uint8_t pinIndex)
{
   switch (component.kind)
   {
      case ComponentKind::VoltageSource:
      {
         return pinIndex == 0 ? "minus" : "plus";
      }
      case ComponentKind::CurrentSource:
      case ComponentKind::Ammeter:
      {
         return pinIndex == 0 ? "A" : "B";
      }
      case ComponentKind::Voltmeter:
      {
         return pinIndex == 0 ? "plus" : "minus";
      }
      case ComponentKind::Diode:
      case ComponentKind::Led:
      {
         return pinIndex == 0 ? "anode" : "cathode";
      }
      case ComponentKind::Npn:
      case ComponentKind::Pnp:
      {
         if (pinIndex == 0) return "C";
         if (pinIndex == 1) return "B";
         return "E";
      }
      case ComponentKind::Nmos:
      case ComponentKind::Pmos:
      {
         if (pinIndex == 0) return "D";
         if (pinIndex == 1) return "G";
         return "S";
      }
      case ComponentKind::Potentiometer:
      {
         if (pinIndex == 0) return "A";
         if (pinIndex == 1) return "W";
         return "B";
      }
      case ComponentKind::SpdtSwitch:
      {
         if (pinIndex == 0) return "COM";
         if (pinIndex == 1) return "A";
         return "B";
      }
      case ComponentKind::Relay:
      {
         if (pinIndex == 0) return "COIL1";
         if (pinIndex == 1) return "COIL2";
         if (pinIndex == 2) return "COM";
         if (pinIndex == 3) return "NO";
         return "NC";
      }
      case ComponentKind::Timer555:
      {
         if (pinIndex == 0) return "VCC";
         if (pinIndex == 1) return "GND";
         if (pinIndex == 2) return "TRIG";
         if (pinIndex == 3) return "THRES";
         if (pinIndex == 4) return "DISCH";
         if (pinIndex == 5) return "OUT";
         return "RESET";
      }
      case ComponentKind::Comparator:
      {
         if (pinIndex == 0) return "IN+";
         if (pinIndex == 1) return "IN-";
         if (pinIndex == 2) return "OUT";
         if (pinIndex == 3) return "VCC";
         return "GND";
      }
      case ComponentKind::Adc:
      {
         if (pinIndex == 0) return "VIN";
         if (pinIndex == 1) return "VREF";
         if (pinIndex == 2) return "CLK";
         return std::string("D") + std::to_string(pinIndex - 3);
      }
      case ComponentKind::Dac:
      {
         if (pinIndex < 4) return std::string("D") + std::to_string(pinIndex);
         if (pinIndex == 4) return "VREF";
         return "VOUT";
      }
      case ComponentKind::SevenSegment:
      {
         if (pinIndex < 7)
         {
            return std::string(1, static_cast<char>('a' + pinIndex));
         }
         if (pinIndex == 7) return "dp";
         return "COM";
      }
      case ComponentKind::Clock:
      {
         return "Y";
      }
      case ComponentKind::DFlipFlop:
      {
         if (pinIndex == 0) return "D";
         if (pinIndex == 1) return "CLK";
         if (pinIndex == 2) return "Q";
         if (pinIndex == 3) return "Qbar";
         if (pinIndex == 4) return "PRE";
         return "CLR";
      }
      case ComponentKind::Counter:
      {
         if (pinIndex == 0) return "CLK";
         if (pinIndex == 1) return "CLR";
         if (pinIndex == 2) return "EN";
         if (pinIndex == 7) return "COUT";
         return std::string("Q") + std::to_string(pinIndex - 3);
      }
      default:
      {
         break;
      }
   }
   return std::string("P") + std::to_string(pinIndex);
}

GridPoint PinPosition(const Component& component, uint8_t pinIndex)
{
   GridPoint offset = At(0, 0);
   if (IsTwoTerminal(component.kind))
   {
      if (pinIndex == 1)
      {
         offset = At(TwoTerminalSpan, 0);
      }
   }
   else if (component.kind == ComponentKind::Npn || component.kind == ComponentKind::Pnp
      || component.kind == ComponentKind::Nmos || component.kind == ComponentKind::Pmos)
   {
      if (pinIndex == 0)
      {
         offset = At(0, -TwoTerminalSpan);
      }
      else if (pinIndex == 1)
      {
         offset = At(-TwoTerminalSpan, 0);
      }
      else
      {
         offset = At(0, TwoTerminalSpan);
      }
   }
   else if (component.kind == ComponentKind::Potentiometer)
   {
      if (pinIndex == 1)
      {
         offset = At(TwoTerminalSpan, -TwoTerminalSpan);
      }
      else if (pinIndex == 2)
      {
         offset = At(BoxWidth, 0);
      }
   }
   else if (component.kind == ComponentKind::SpdtSwitch)
   {
      if (pinIndex == 1)
      {
         offset = At(TwoTerminalSpan, TwoTerminalSpan);
      }
      else if (pinIndex == 2)
      {
         offset = At(TwoTerminalSpan, -TwoTerminalSpan);
      }
   }
   else if (component.kind == ComponentKind::DipSwitch)
   {
      uint8_t channel = static_cast<uint8_t>(pinIndex / 2);
      offset = (pinIndex % 2) == 0 ? LeftPin(channel) : RightPin(channel);
   }
   else if (component.kind == ComponentKind::Clock)
   {
      offset = RightPin(0);
   }
   else if (component.kind == ComponentKind::AndGate || component.kind == ComponentKind::OrGate
      || component.kind == ComponentKind::NandGate || component.kind == ComponentKind::NorGate
      || component.kind == ComponentKind::Buffer || component.kind == ComponentKind::Inverter
      || component.kind == ComponentKind::SchmittInverter || component.kind == ComponentKind::XorGate
      || component.kind == ComponentKind::XnorGate)
   {
      uint8_t count = PinCount(component);
      if (pinIndex + 1 == count)
      {
         offset = RightPin(0);
      }
      else
      {
         offset = LeftPin(pinIndex);
      }
   }
   else if (component.kind == ComponentKind::TriStateBuffer)
   {
      if (pinIndex == 0) offset = LeftPin(0);
      else if (pinIndex == 1) offset = LeftPin(1);
      else offset = RightPin(0);
   }
   else if (component.kind == ComponentKind::DFlipFlop || component.kind == ComponentKind::TFlipFlop)
   {
      if (pinIndex == 0) offset = LeftPin(0);
      else if (pinIndex == 1) offset = LeftPin(1);
      else if (pinIndex == 2) offset = RightPin(0);
      else if (pinIndex == 3) offset = RightPin(1);
      else if (pinIndex == 4) offset = LeftPin(2);
      else offset = LeftPin(3);
   }
   else if (component.kind == ComponentKind::SrLatch || component.kind == ComponentKind::DLatch)
   {
      if (pinIndex == 0) offset = LeftPin(0);
      else if (pinIndex == 1) offset = LeftPin(1);
      else if (pinIndex == 2) offset = RightPin(0);
      else offset = RightPin(1);
   }
   else if (component.kind == ComponentKind::JkFlipFlop)
   {
      if (pinIndex == 0) offset = LeftPin(0);
      else if (pinIndex == 1) offset = LeftPin(1);
      else if (pinIndex == 2) offset = LeftPin(2);
      else if (pinIndex == 3) offset = RightPin(0);
      else if (pinIndex == 4) offset = RightPin(1);
      else if (pinIndex == 5) offset = LeftPin(3);
      else offset = RightPin(3);
   }
   else if (component.kind == ComponentKind::Counter)
   {
      if (pinIndex < 3) offset = LeftPin(pinIndex);
      else offset = RightPin(static_cast<uint8_t>(pinIndex - 3));
   }
   else if (component.kind == ComponentKind::ShiftRegister || component.kind == ComponentKind::Adc)
   {
      if (pinIndex < 3) offset = LeftPin(pinIndex);
      else offset = RightPin(static_cast<uint8_t>(pinIndex - 3));
   }
   else if (component.kind == ComponentKind::Decoder)
   {
      if (pinIndex < 3) offset = LeftPin(pinIndex);
      else offset = RightPin(static_cast<uint8_t>(pinIndex - 3));
   }
   else if (component.kind == ComponentKind::BcdToSevenSegment)
   {
      if (pinIndex < 4) offset = LeftPin(pinIndex);
      else offset = RightPin(static_cast<uint8_t>(pinIndex - 4));
   }
   else if (component.kind == ComponentKind::FullAdder || component.kind == ComponentKind::Comparator)
   {
      if (pinIndex < 3) offset = LeftPin(pinIndex);
      else offset = RightPin(static_cast<uint8_t>(pinIndex - 3));
   }
   else if (component.kind == ComponentKind::Relay)
   {
      if (pinIndex == 0) offset = LeftPin(0);
      else if (pinIndex == 1) offset = LeftPin(1);
      else offset = RightPin(static_cast<uint8_t>(pinIndex - 2));
   }
   else if (component.kind == ComponentKind::Timer555)
   {
      if (pinIndex < 4) offset = LeftPin(pinIndex);
      else offset = RightPin(static_cast<uint8_t>(pinIndex - 4));
   }
   else if (component.kind == ComponentKind::SevenSegment)
   {
      if (pinIndex < 8) offset = LeftPin(pinIndex);
      else offset = RightPin(0);
   }
   else if (component.kind == ComponentKind::Dac)
   {
      if (pinIndex < 5) offset = LeftPin(pinIndex);
      else offset = RightPin(0);
   }
   else if (component.kind == ComponentKind::Multiplexer)
   {
      uint8_t count = PinCount(component);
      if (pinIndex + 1 == count)
      {
         offset = RightPin(0);
      }
      else
      {
         offset = LeftPin(pinIndex);
      }
   }
   GridPoint rotated = RotateOffset(offset, component.rotationDegrees);
   return At(component.position.positionX + rotated.positionX, component.position.positionY + rotated.positionY);
}

Component CreateComponent(ComponentKind kind, GridPoint position)
{
   Component component = {};
   component.id = 0;
   component.kind = kind;
   component.name = KindPrefix(kind);
   component.position = position;
   component.rotationDegrees = 0;
   switch (kind)
   {
      case ComponentKind::Resistor:
      {
         component.properties = ResistorProperties{DefaultResistanceOhm};
         break;
      }
      case ComponentKind::Capacitor:
      {
         component.properties = CapacitorProperties{DefaultCapacitanceFarad, 0.0, 0.0};
         break;
      }
      case ComponentKind::Inductor:
      {
         component.properties = InductorProperties{DefaultInductanceHenry, 0.0, 0.0};
         break;
      }
      case ComponentKind::Potentiometer:
      {
         component.properties = PotentiometerProperties{DefaultResistanceOhm, 0.5};
         break;
      }
      case ComponentKind::VoltageSource:
      {
         component.properties = MakeVoltageSource();
         break;
      }
      case ComponentKind::CurrentSource:
      {
         component.properties = MakeCurrentSource();
         break;
      }
      case ComponentKind::Diode:
      {
         component.properties = MakeDiode();
         break;
      }
      case ComponentKind::Led:
      {
         LedProperties led = {};
         led.diode = MakeDiode();
         led.diode.forwardVoltageVolt = 2.0;
         led.diode.forwardCurrentAmpere = 0.01;
         led.color = LedColor::Red;
         component.properties = led;
         break;
      }
      case ComponentKind::Npn:
      case ComponentKind::Pnp:
      {
         BjtProperties bjt = {};
         bjt.forwardVoltageVolt = DefaultForwardVoltageVolt;
         bjt.forwardCurrentAmpere = DefaultForwardCurrentAmpere;
         bjt.emissionCoefficient = DefaultEmissionCoefficient;
         bjt.temperatureCelsius = DefaultTemperatureCelsius;
         bjt.forwardBeta = DefaultForwardBeta;
         bjt.reverseBeta = DefaultReverseBeta;
         bjt.earlyVoltageVolt = 0.0;
         component.properties = bjt;
         break;
      }
      case ComponentKind::Nmos:
      case ComponentKind::Pmos:
      {
         MosfetProperties mosfet = {};
         mosfet.thresholdVoltageVolt = kind == ComponentKind::Nmos ? DefaultMosThresholdVolt : -DefaultMosThresholdVolt;
         mosfet.kpAmperePerVoltSquared = DefaultMosKp;
         mosfet.channelWidthMeter = DefaultChannelWidthMeter;
         mosfet.channelLengthMeter = DefaultChannelLengthMeter;
         mosfet.lambdaPerVolt = 0.0;
         mosfet.cgsoFaradPerMeter = 0.0;
         mosfet.cgdoFaradPerMeter = 0.0;
         component.properties = mosfet;
         break;
      }
      case ComponentKind::Relay:
      {
         component.properties = RelayProperties{100.0, 0.0, 0.02, 0.01, 0.005};
         break;
      }
      case ComponentKind::Timer555:
      {
         component.properties = Timer555Properties{DefaultOutputResistanceOhm, DefaultPropagationDelaySec};
         break;
      }
      case ComponentKind::Comparator:
      {
         component.properties = ComparatorProperties{0.0, DefaultOutputResistanceOhm, DefaultPropagationDelaySec};
         break;
      }
      case ComponentKind::Ground:
      {
         component.properties = EmptyProperties{0};
         break;
      }
      case ComponentKind::SpstSwitch:
      {
         SpstProperties properties = {};
         properties.restState = SwitchState::Open;
         properties.closedMode = ResistanceMode::Ideal;
         properties.closedResistanceOhm = DefaultClosedResistanceOhm;
         properties.openMode = ResistanceMode::Ideal;
         properties.openResistanceOhm = DefaultOpenResistanceOhm;
         component.properties = properties;
         break;
      }
      case ComponentKind::SpdtSwitch:
      {
         SpdtProperties properties = {};
         properties.restPosition = ThrowPosition::ThrowA;
         properties.closedMode = ResistanceMode::Ideal;
         properties.closedResistanceOhm = DefaultClosedResistanceOhm;
         properties.openMode = ResistanceMode::Ideal;
         properties.openResistanceOhm = DefaultOpenResistanceOhm;
         component.properties = properties;
         break;
      }
      case ComponentKind::Pushbutton:
      {
         PushbuttonProperties properties = {};
         properties.contact = ContactForm::NormallyOpen;
         properties.mechanicalState = SwitchState::Open;
         properties.closedMode = ResistanceMode::Ideal;
         properties.closedResistanceOhm = DefaultClosedResistanceOhm;
         properties.openMode = ResistanceMode::Ideal;
         properties.openResistanceOhm = DefaultOpenResistanceOhm;
         component.properties = properties;
         break;
      }
      case ComponentKind::DipSwitch:
      {
         DipSwitchProperties properties = {};
         properties.channelState.fill(SwitchState::Open);
         properties.closedMode = ResistanceMode::Ideal;
         properties.closedResistanceOhm = DefaultClosedResistanceOhm;
         properties.openMode = ResistanceMode::Ideal;
         properties.openResistanceOhm = DefaultOpenResistanceOhm;
         component.properties = properties;
         break;
      }
      case ComponentKind::SevenSegment:
      {
         SevenSegmentProperties properties = {};
         properties.common = SegmentCommon::CommonCathode;
         properties.color = LedColor::Red;
         properties.forwardVoltageVolt = 2.0;
         properties.forwardCurrentAmpere = 0.01;
         properties.seriesResistanceOhm = DefaultSegmentResistanceOhm;
         component.properties = properties;
         break;
      }
      case ComponentKind::Ammeter:
      case ComponentKind::Voltmeter:
      {
         MeterProperties properties = {};
         properties.mode = MeterMode::Ideal;
         properties.burdenResistanceOhm = 1.0;
         properties.burdenInductanceHenry = 0.0;
         properties.inputResistanceOhm = 1000000.0;
         properties.inputCapacitanceFarad = 0.0;
         component.properties = properties;
         break;
      }
      case ComponentKind::LogicProbe:
      {
         ProbeProperties probe = {};
         probe.family = LogicFamily::Cmos5V;
         probe.supplyVoltageVolt = DefaultSupplyVoltageVolt;
         probe.vilVolt = CmosLowFraction * DefaultSupplyVoltageVolt;
         probe.vihVolt = CmosHighFraction * DefaultSupplyVoltageVolt;
         component.properties = probe;
         break;
      }
      case ComponentKind::Clock:
      {
         ClockProperties clock = {};
         clock.logic = MakeLogic(0);
         clock.frequencyHz = 1000.0;
         clock.dutyCycle = 0.5;
         clock.delaySec = 0.0;
         clock.startLevel = LogicLevel::Low;
         component.properties = clock;
         break;
      }
      case ComponentKind::Multiplexer:
      {
         MuxProperties mux = {};
         mux.logic = MakeLogic(2);
         mux.width = MuxWidth::Two;
         component.properties = mux;
         break;
      }
      case ComponentKind::Decoder:
      case ComponentKind::BcdToSevenSegment:
      {
         DecoderProperties decoder = {};
         decoder.logic = MakeLogic(2);
         decoder.outputPolarity = Polarity::ActiveHigh;
         decoder.enablePolarity = Polarity::ActiveHigh;
         component.properties = decoder;
         break;
      }
      case ComponentKind::Counter:
      {
         CounterProperties counter = {};
         counter.logic = MakeLogic(1);
         counter.clearPolarity = Polarity::ActiveHigh;
         counter.clockEdge = ClockEdge::Rising;
         counter.initialValue = 0;
         component.properties = counter;
         break;
      }
      case ComponentKind::ShiftRegister:
      {
         ShiftRegisterProperties shift = {};
         shift.logic = MakeLogic(1);
         shift.clearPolarity = Polarity::ActiveHigh;
         shift.clockEdge = ClockEdge::Rising;
         shift.initialValue = 0;
         component.properties = shift;
         break;
      }
      case ComponentKind::PullUp:
      case ComponentKind::PullDown:
      {
         PullProperties pull = {};
         pull.logic = MakeLogic(0);
         pull.resistanceOhm = DefaultResistanceOhm;
         component.properties = pull;
         break;
      }
      case ComponentKind::Adc:
      {
         AdcProperties adc = {};
         adc.logic = MakeLogic(1);
         adc.conversion = AdcConversion::Clocked;
         adc.clockEdge = ClockEdge::Rising;
         component.properties = adc;
         break;
      }
      case ComponentKind::Dac:
      {
         DacProperties dac = {};
         dac.logic = MakeLogic(4);
         component.properties = dac;
         break;
      }
      case ComponentKind::SrLatch:
      case ComponentKind::DLatch:
      case ComponentKind::DFlipFlop:
      case ComponentKind::JkFlipFlop:
      case ComponentKind::TFlipFlop:
      {
         component.properties = MakeSequential();
         break;
      }
      default:
      {
         LogicProperties logic = MakeLogic(2);
         if (kind == ComponentKind::Buffer || kind == ComponentKind::Inverter || kind == ComponentKind::SchmittInverter
            || kind == ComponentKind::XorGate || kind == ComponentKind::XnorGate)
         {
            logic.inputCount = kind == ComponentKind::XorGate || kind == ComponentKind::XnorGate ? 2 : 1;
         }
         if (kind == ComponentKind::Buffer || kind == ComponentKind::Inverter || kind == ComponentKind::SchmittInverter)
         {
            logic.inputCount = 1;
         }
         component.properties = logic;
         break;
      }
   }
   return component;
}

} // namespace Analyzer
