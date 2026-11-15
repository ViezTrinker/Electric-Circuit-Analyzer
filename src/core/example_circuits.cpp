/*!
 * \file example_circuits.cpp
 * \brief Schematics used by the example menu and the example files.
 */

#include "example_circuits.h"

namespace Analyzer
{

namespace
{

Result AddPart(Circuit* pCircuit, ComponentKind kind, int32_t positionX, int32_t positionY, int32_t rotationDegrees, uint64_t* pId)
{
   if (pCircuit == nullptr)
   {
      return Result::InvalidParameter;
   }
   Component component = CreateComponent(kind, GridPoint{positionX, positionY});
   component.rotationDegrees = rotationDegrees;
   return pCircuit->AddComponent(component, pId);
}

Result AddGround(Circuit* pCircuit, int32_t positionX, int32_t positionY)
{
   return AddPart(pCircuit, ComponentKind::Ground, positionX, positionY, 0, nullptr);
}

Result AddSegment(Circuit* pCircuit, int32_t startX, int32_t startY, int32_t endX, int32_t endY)
{
   if (pCircuit == nullptr)
   {
      return Result::InvalidParameter;
   }
   Wire wire = {};
   wire.points.push_back(GridPoint{startX, startY});
   wire.points.push_back(GridPoint{endX, endY});
   return pCircuit->AddWire(wire, nullptr);
}

Result SetNumber(Circuit* pCircuit, uint64_t componentId, NumericProperty property, double value)
{
   if (pCircuit == nullptr)
   {
      return Result::InvalidParameter;
   }
   Component* pComponent = pCircuit->FindComponent(componentId);
   if (pComponent == nullptr)
   {
      return Result::Error;
   }
   return WriteNumeric(pComponent, property, value);
}

Result SetCode(Circuit* pCircuit, uint64_t componentId, EnumProperty property, int32_t code)
{
   if (pCircuit == nullptr)
   {
      return Result::InvalidParameter;
   }
   Component* pComponent = pCircuit->FindComponent(componentId);
   if (pComponent == nullptr)
   {
      return Result::Error;
   }
   return WriteEnumCode(pComponent, property, code);
}

Result BuildVoltageDivider(Circuit* pCircuit)
{
   pCircuit->SetName("Voltage divider");
   uint64_t sourceId = 0;
   uint64_t topId = 0;
   uint64_t bottomId = 0;
   if (IsErr(AddGround(pCircuit, 0, 0)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::VoltageSource, 0, 0, 90, &sourceId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Resistor, 0, TwoTerminalSpan, 0, &topId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Resistor, TwoTerminalSpan, TwoTerminalSpan, 270, &bottomId)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::DcVoltageVolt, 10.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, topId, NumericProperty::ResistanceOhm, 1000.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, bottomId, NumericProperty::ResistanceOhm, 1000.0)))
   {
      return Result::Error;
   }
   return AddSegment(pCircuit, TwoTerminalSpan, 0, 0, 0);
}

Result BuildRcLowpass(Circuit* pCircuit)
{
   pCircuit->SetName("RC low-pass");
   pCircuit->Settings().kind = AnalysisKind::AcSweep;
   pCircuit->Settings().sweepScale = SweepScale::Logarithmic;
   pCircuit->Settings().sweepStartHz = 10.0;
   pCircuit->Settings().sweepStopHz = 100000.0;
   pCircuit->Settings().pointsPerDecade = 10;
   uint64_t sourceId = 0;
   uint64_t resistorId = 0;
   uint64_t capacitorId = 0;
   if (IsErr(AddGround(pCircuit, 0, 0)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::VoltageSource, 0, 0, 90, &sourceId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Resistor, 0, TwoTerminalSpan, 0, &resistorId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Capacitor, TwoTerminalSpan, TwoTerminalSpan, 270, &capacitorId)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::DcVoltageVolt, 0.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::AcAmplitudeVolt, 1.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, resistorId, NumericProperty::ResistanceOhm, 1000.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, capacitorId, NumericProperty::CapacitanceFarad, 1.0e-6)))
   {
      return Result::Error;
   }
   return AddSegment(pCircuit, TwoTerminalSpan, 0, 0, 0);
}

Result BuildRlcStep(Circuit* pCircuit)
{
   constexpr double PulseWidthSec = 0.02;
   constexpr double PulsePeriodSec = 0.04;
   pCircuit->SetName("RLC step");
   pCircuit->Settings().kind = AnalysisKind::Transient;
   pCircuit->Settings().stopTimeSec = 0.005;
   pCircuit->Settings().maximumStepSec = 2.0e-5;
   uint64_t sourceId = 0;
   uint64_t resistorId = 0;
   uint64_t inductorId = 0;
   uint64_t capacitorId = 0;
   if (IsErr(AddGround(pCircuit, 0, 0)) || IsErr(AddGround(pCircuit, 4, 0)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::VoltageSource, 0, 0, 90, &sourceId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Resistor, 0, TwoTerminalSpan, 0, &resistorId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Inductor, TwoTerminalSpan, TwoTerminalSpan, 0, &inductorId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Capacitor, 4, TwoTerminalSpan, 270, &capacitorId)))
   {
      return Result::Error;
   }
   if (IsErr(SetCode(pCircuit, sourceId, EnumProperty::TransientWaveform, static_cast<int32_t>(SourceWaveform::Pulse))))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::DcVoltageVolt, 0.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::PulseInitial, 0.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::PulseLevel, 1.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::PulseDelaySec, 0.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::PulseRiseSec, 0.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::PulseFallSec, 0.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::PulseWidthSec, PulseWidthSec)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::PulsePeriodSec, PulsePeriodSec)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, resistorId, NumericProperty::ResistanceOhm, 10.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, inductorId, NumericProperty::InductanceHenry, 0.01)))
   {
      return Result::Error;
   }
   return SetNumber(pCircuit, capacitorId, NumericProperty::CapacitanceFarad, 1.0e-6);
}

Result BuildRectifier(Circuit* pCircuit)
{
   pCircuit->SetName("Diode rectifier");
   pCircuit->Settings().kind = AnalysisKind::Transient;
   pCircuit->Settings().stopTimeSec = 0.04;
   pCircuit->Settings().maximumStepSec = 2.0e-4;
   uint64_t sourceId = 0;
   uint64_t resistorId = 0;
   if (IsErr(AddGround(pCircuit, 0, 0)) || IsErr(AddGround(pCircuit, TwoTerminalSpan, 0)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::VoltageSource, 0, 0, 90, &sourceId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Diode, 0, TwoTerminalSpan, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Resistor, TwoTerminalSpan, TwoTerminalSpan, 270, &resistorId)))
   {
      return Result::Error;
   }
   if (IsErr(SetCode(pCircuit, sourceId, EnumProperty::TransientWaveform, static_cast<int32_t>(SourceWaveform::Sine))))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::DcVoltageVolt, 0.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::SineOffset, 0.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::SineAmplitude, 5.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::SineFrequencyHz, 50.0)))
   {
      return Result::Error;
   }
   return SetNumber(pCircuit, resistorId, NumericProperty::ResistanceOhm, 1000.0);
}

Result BuildNpnBias(Circuit* pCircuit)
{
   pCircuit->SetName("NPN bias");
   uint64_t sourceId = 0;
   uint64_t collectorId = 0;
   uint64_t baseId = 0;
   if (IsErr(AddGround(pCircuit, 0, -2)) || IsErr(AddGround(pCircuit, 4, 4)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::VoltageSource, 0, -2, 90, &sourceId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Resistor, 0, 0, 0, &collectorId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Resistor, 0, 2, 0, &baseId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Npn, 4, 2, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(AddSegment(pCircuit, 2, 0, 4, 0)) || IsErr(AddSegment(pCircuit, 0, 0, 0, 2)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::DcVoltageVolt, 5.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, collectorId, NumericProperty::ResistanceOhm, 1000.0)))
   {
      return Result::Error;
   }
   return SetNumber(pCircuit, baseId, NumericProperty::ResistanceOhm, 470000.0);
}

Result BuildNmosSwitch(Circuit* pCircuit)
{
   pCircuit->SetName("NMOS switch");
   uint64_t gateId = 0;
   uint64_t drainId = 0;
   uint64_t mosfetId = 0;
   if (IsErr(AddGround(pCircuit, 0, 2)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::VoltageSource, -2, 2, 270, &gateId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::VoltageSource, 0, 0, 270, &drainId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Nmos, 0, 0, 0, &mosfetId)))
   {
      return Result::Error;
   }
   if (IsErr(AddSegment(pCircuit, -2, 2, 0, 2)) || IsErr(AddSegment(pCircuit, 0, 0, 0, 2)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, gateId, NumericProperty::DcVoltageVolt, 3.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, drainId, NumericProperty::DcVoltageVolt, 5.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, mosfetId, NumericProperty::ThresholdVoltageVolt, 1.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, mosfetId, NumericProperty::Kp, 0.002)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, mosfetId, NumericProperty::ChannelWidthMeter, 1.0)))
   {
      return Result::Error;
   }
   return SetNumber(pCircuit, mosfetId, NumericProperty::ChannelLengthMeter, 1.0);
}

Result BuildNandSrLatch(Circuit* pCircuit)
{
   pCircuit->SetName("NAND SR latch");
   if (IsErr(AddPart(pCircuit, ComponentKind::NandGate, 0, 0, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::NandGate, 8, 4, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::PullDown, 0, 0, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::PullUp, 8, 4, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(AddSegment(pCircuit, 4, 0, 4, -2)) || IsErr(AddSegment(pCircuit, 4, -2, 16, -2)))
   {
      return Result::Error;
   }
   if (IsErr(AddSegment(pCircuit, 16, -2, 16, 6)) || IsErr(AddSegment(pCircuit, 16, 6, 8, 6)))
   {
      return Result::Error;
   }
   if (IsErr(AddSegment(pCircuit, 12, 4, 14, 4)) || IsErr(AddSegment(pCircuit, 14, 4, 14, 2)))
   {
      return Result::Error;
   }
   if (IsErr(AddSegment(pCircuit, 14, 2, 0, 2)))
   {
      return Result::Error;
   }
   return Result::Ok;
}

Result BuildBinaryCounter(Circuit* pCircuit)
{
   uint64_t clockId = 0;
   uint64_t counterId = 0;
   pCircuit->SetName("Binary counter");
   pCircuit->Settings().kind = AnalysisKind::Transient;
   pCircuit->Settings().stopTimeSec = 0.0035;
   if (IsErr(AddPart(pCircuit, ComponentKind::Counter, 0, 0, 0, &counterId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Clock, -4, 0, 0, &clockId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::PullDown, 0, 2, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::PullUp, 0, 4, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, clockId, NumericProperty::PropagationDelaySec, 0.0)))
   {
      return Result::Error;
   }
   return SetNumber(pCircuit, counterId, NumericProperty::PropagationDelaySec, 0.0);
}

Result BuildSevenSegmentCount(Circuit* pCircuit)
{
   uint64_t clockId = 0;
   uint64_t counterId = 0;
   pCircuit->SetName("Seven-segment count");
   pCircuit->Settings().kind = AnalysisKind::Transient;
   pCircuit->Settings().stopTimeSec = 0.0025;
   if (IsErr(AddGround(pCircuit, 20, 0)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Counter, 0, 0, 0, &counterId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Clock, -4, 0, 0, &clockId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::PullDown, 0, 2, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::PullUp, 0, 4, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::BcdToSevenSegment, 8, 0, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::SevenSegment, 16, 0, 0, nullptr)))
   {
      return Result::Error;
   }
   for (int32_t row = 0; row < 4; row += 1)
   {
      int32_t positionY = row * PinSpacing;
      if (IsErr(AddSegment(pCircuit, BoxWidth, positionY, 8, positionY)))
      {
         return Result::Error;
      }
   }
   for (int32_t row = 0; row < 7; row += 1)
   {
      int32_t positionY = row * PinSpacing;
      if (IsErr(AddSegment(pCircuit, 12, positionY, 16, positionY)))
      {
         return Result::Error;
      }
   }
   if (IsErr(SetNumber(pCircuit, clockId, NumericProperty::PropagationDelaySec, 0.0)))
   {
      return Result::Error;
   }
   return SetNumber(pCircuit, counterId, NumericProperty::PropagationDelaySec, 0.0);
}

Result BuildLedAndSwitch(Circuit* pCircuit)
{
   uint64_t sourceId = 0;
   uint64_t resistorId = 0;
   uint64_t switchId = 0;
   pCircuit->SetName("LED and switch");
   if (IsErr(AddGround(pCircuit, 0, 0)) || IsErr(AddGround(pCircuit, 4, 0)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::VoltageSource, 0, 0, 90, &sourceId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Resistor, 0, TwoTerminalSpan, 0, &resistorId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Led, TwoTerminalSpan, TwoTerminalSpan, 0, nullptr)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::SpstSwitch, 4, TwoTerminalSpan, 270, &switchId)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, sourceId, NumericProperty::DcVoltageVolt, 5.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, resistorId, NumericProperty::ResistanceOhm, 220.0)))
   {
      return Result::Error;
   }
   return SetCode(pCircuit, switchId, EnumProperty::RestState, static_cast<int32_t>(SwitchState::Closed));
}

Result BuildTimer555(Circuit* pCircuit)
{
   uint64_t supplyId = 0;
   uint64_t timerId = 0;
   uint64_t chargeId = 0;
   uint64_t dischargeId = 0;
   uint64_t capacitorId = 0;
   uint64_t loadId = 0;
   pCircuit->SetName("555 astable");
   pCircuit->Settings().kind = AnalysisKind::Transient;
   pCircuit->Settings().stopTimeSec = 0.008;
   pCircuit->Settings().maximumStepSec = 5.0e-6;
   if (IsErr(AddGround(pCircuit, 0, -2)) || IsErr(AddGround(pCircuit, 0, 2)) || IsErr(AddGround(pCircuit, 6, 2)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::VoltageSource, 0, -2, 90, &supplyId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Timer555, 0, 0, 0, &timerId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Resistor, 2, 0, 0, &chargeId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Resistor, 8, 6, 90, &dischargeId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Capacitor, 0, 2, 90, &capacitorId)))
   {
      return Result::Error;
   }
   if (IsErr(AddPart(pCircuit, ComponentKind::Resistor, 4, 2, 0, &loadId)))
   {
      return Result::Error;
   }
   if (IsErr(AddSegment(pCircuit, 0, 0, 2, 0)))
   {
      return Result::Error;
   }
   if (IsErr(AddSegment(pCircuit, 4, 0, 8, 0)) || IsErr(AddSegment(pCircuit, 8, 0, 8, 6)))
   {
      return Result::Error;
   }
   if (IsErr(AddSegment(pCircuit, 8, 8, 0, 8)) || IsErr(AddSegment(pCircuit, 0, 8, 0, 6)))
   {
      return Result::Error;
   }
   if (IsErr(AddSegment(pCircuit, 0, 4, 0, 6)))
   {
      return Result::Error;
   }
   if (IsErr(AddSegment(pCircuit, 4, 4, 2, 4)) || IsErr(AddSegment(pCircuit, 2, 4, 2, 0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, supplyId, NumericProperty::DcVoltageVolt, 5.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, chargeId, NumericProperty::ResistanceOhm, 10000.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, dischargeId, NumericProperty::ResistanceOhm, 10000.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, capacitorId, NumericProperty::CapacitanceFarad, 100.0e-9)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, loadId, NumericProperty::ResistanceOhm, 10000.0)))
   {
      return Result::Error;
   }
   if (IsErr(SetNumber(pCircuit, timerId, NumericProperty::OutputResistanceOhm, 1.0)))
   {
      return Result::Error;
   }
   return SetNumber(pCircuit, timerId, NumericProperty::PropagationDelaySec, 0.0);
}

} // namespace

std::string_view ExampleFileName(ExampleCircuit example)
{
   switch (example)
   {
      case ExampleCircuit::VoltageDivider: return "voltage_divider.eca.json";
      case ExampleCircuit::RcLowpass: return "rc_lowpass.eca.json";
      case ExampleCircuit::RlcStep: return "rlc_step.eca.json";
      case ExampleCircuit::Rectifier: return "rectifier.eca.json";
      case ExampleCircuit::NpnBias: return "npn_bias.eca.json";
      case ExampleCircuit::NmosSwitch: return "nmos_switch.eca.json";
      case ExampleCircuit::NandSrLatch: return "nand_sr_latch.eca.json";
      case ExampleCircuit::BinaryCounter: return "binary_counter.eca.json";
      case ExampleCircuit::SevenSegmentCount: return "seven_segment_count.eca.json";
      case ExampleCircuit::LedAndSwitch: return "led_and_switch.eca.json";
      case ExampleCircuit::Timer555Astable: return "timer_555_astable.eca.json";
      default: break;
   }
   return {};
}

std::string_view ExampleTitle(ExampleCircuit example)
{
   switch (example)
   {
      case ExampleCircuit::VoltageDivider: return "Voltage Divider";
      case ExampleCircuit::RcLowpass: return "RC Low-Pass";
      case ExampleCircuit::RlcStep: return "RLC Step";
      case ExampleCircuit::Rectifier: return "Diode Rectifier";
      case ExampleCircuit::NpnBias: return "NPN Bias";
      case ExampleCircuit::NmosSwitch: return "NMOS Switch";
      case ExampleCircuit::NandSrLatch: return "NAND SR Latch";
      case ExampleCircuit::BinaryCounter: return "Binary Counter";
      case ExampleCircuit::SevenSegmentCount: return "Seven-Segment Count";
      case ExampleCircuit::LedAndSwitch: return "LED and Switch";
      case ExampleCircuit::Timer555Astable: return "555 Astable";
      default: break;
   }
   return {};
}

Result BuildExample(ExampleCircuit example, Circuit* pCircuit)
{
   if (pCircuit == nullptr)
   {
      return Result::InvalidParameter;
   }
   Circuit built;
   Result code = Result::InvalidParameter;
   switch (example)
   {
      case ExampleCircuit::VoltageDivider: code = BuildVoltageDivider(&built); break;
      case ExampleCircuit::RcLowpass: code = BuildRcLowpass(&built); break;
      case ExampleCircuit::RlcStep: code = BuildRlcStep(&built); break;
      case ExampleCircuit::Rectifier: code = BuildRectifier(&built); break;
      case ExampleCircuit::NpnBias: code = BuildNpnBias(&built); break;
      case ExampleCircuit::NmosSwitch: code = BuildNmosSwitch(&built); break;
      case ExampleCircuit::NandSrLatch: code = BuildNandSrLatch(&built); break;
      case ExampleCircuit::BinaryCounter: code = BuildBinaryCounter(&built); break;
      case ExampleCircuit::SevenSegmentCount: code = BuildSevenSegmentCount(&built); break;
      case ExampleCircuit::LedAndSwitch: code = BuildLedAndSwitch(&built); break;
      case ExampleCircuit::Timer555Astable: code = BuildTimer555(&built); break;
      default: break;
   }
   if (IsErr(code))
   {
      return code;
   }
   *pCircuit = std::move(built);
   return Result::Ok;
}

} // namespace Analyzer
