/*!
 * \file test_mixed.cpp
 * \brief Checks the mixed DC bridge, a frozen AC gate, and a clocked LED.
 */

#include "core/netlist_builder.h"
#include "solver/simulator.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{

int gFailures = 0;

void Expect(bool condition, std::string_view message)
{
   if (condition == false)
   {
      std::cerr << "FAIL " << message << "\n";
      gFailures += 1;
   }
}

void ExpectNear(double actual, double expected, double tolerance, std::string_view message)
{
   if (std::isfinite(actual) == false || std::abs(actual - expected) > tolerance)
   {
      std::cerr << "FAIL " << message << " actual=" << actual << " expected=" << expected << "\n";
      gFailures += 1;
   }
}

uint64_t Place(Analyzer::Circuit* pCircuit, Analyzer::ComponentKind kind, int32_t positionX, int32_t positionY, int32_t rotation)
{
   Analyzer::Component component = Analyzer::CreateComponent(kind, Analyzer::GridPoint{positionX, positionY});
   component.rotationDegrees = rotation;
   uint64_t id = 0;
   Analyzer::Result added = pCircuit->AddComponent(component, &id);
   Expect(Analyzer::IsOk(added), "place component");
   return id;
}

void SetNumber(Analyzer::Circuit* pCircuit, uint64_t id, Analyzer::NumericProperty property, double value)
{
   Analyzer::Component* pComponent = pCircuit->FindComponent(id);
   Expect(pComponent != nullptr, "find placed component");
   if (pComponent == nullptr)
   {
      return;
   }
   Expect(Analyzer::IsOk(Analyzer::WriteNumeric(pComponent, property, value)), "write numeric property");
}

void SetCode(Analyzer::Circuit* pCircuit, uint64_t id, Analyzer::EnumProperty property, int32_t code)
{
   Analyzer::Component* pComponent = pCircuit->FindComponent(id);
   Expect(pComponent != nullptr, "find placed component");
   if (pComponent == nullptr)
   {
      return;
   }
   Expect(Analyzer::IsOk(Analyzer::WriteEnumCode(pComponent, property, code)), "write enum property");
}

double NetVoltageOfPin(const Analyzer::Circuit& circuit, const Analyzer::DcResult& result, uint64_t componentId, uint8_t pinIndex)
{
   Analyzer::Netlist netlist;
   if (Analyzer::IsErr(Analyzer::BuildNetlist(circuit, &netlist)))
   {
      return 0.0;
   }
   for (const Analyzer::NetlistDevice& device : netlist.devices)
   {
      if (device.componentId == componentId && pinIndex < device.pinCount && device.nets[pinIndex] < result.netVoltage.size())
      {
         return result.netVoltage[device.nets[pinIndex]];
      }
   }
   return 0.0;
}

double BranchCurrent(const Analyzer::DcResult& result, uint64_t componentId)
{
   for (const Analyzer::BranchValue& branch : result.branches)
   {
      if (branch.componentId == componentId)
      {
         return branch.currentAmpere;
      }
   }
   return 0.0;
}

void TestInverterLoad(void)
{
   constexpr double LoadOhm = 1000.0;
   constexpr double OutputOhm = 100.0;
   constexpr double SupplyVolt = 5.0;
   constexpr double ExpectedVolt = SupplyVolt * LoadOhm / (LoadOhm + OutputOhm);
   Analyzer::Circuit circuit;
   Place(&circuit, Analyzer::ComponentKind::Ground, 0, 0, 0);
   uint64_t inverter = Place(&circuit, Analyzer::ComponentKind::Inverter, 0, 0, 0);
   uint64_t load = Place(&circuit, Analyzer::ComponentKind::Resistor, 4, 0, 0);
   Place(&circuit, Analyzer::ComponentKind::Ground, 6, 0, 0);
   SetNumber(&circuit, inverter, Analyzer::NumericProperty::OutputResistanceOhm, OutputOhm);
   SetNumber(&circuit, load, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   ExpectNear(NetVoltageOfPin(circuit, result, inverter, 1), ExpectedVolt, 1.0e-4, "inverter loaded output");
   ExpectNear(BranchCurrent(result, load), ExpectedVolt / LoadOhm, 1.0e-6, "inverter load current");
}

void TestInverterAcHasNoGain(void)
{
   constexpr double LoadOhm = 1000.0;
   constexpr double OutputOhm = 100.0;
   constexpr double SupplyVolt = 5.0;
   constexpr double AcAmplitudeVolt = 1.0;
   constexpr double SweepHz = 1000.0;
   constexpr double ExpectedVolt = SupplyVolt * LoadOhm / (LoadOhm + OutputOhm);
   Analyzer::Circuit circuit;
   Place(&circuit, Analyzer::ComponentKind::Ground, 0, 0, 0);
   uint64_t source = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   uint64_t inverter = Place(&circuit, Analyzer::ComponentKind::Inverter, 0, 2, 0);
   uint64_t load = Place(&circuit, Analyzer::ComponentKind::Resistor, 4, 2, 0);
   Place(&circuit, Analyzer::ComponentKind::Ground, 6, 2, 0);
   SetNumber(&circuit, source, Analyzer::NumericProperty::DcVoltageVolt, 0.0);
   SetNumber(&circuit, source, Analyzer::NumericProperty::AcAmplitudeVolt, AcAmplitudeVolt);
   SetNumber(&circuit, inverter, Analyzer::NumericProperty::OutputResistanceOhm, OutputOhm);
   SetNumber(&circuit, load, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   circuit.Settings().sweepStartHz = SweepHz;
   circuit.Settings().sweepStopHz = SweepHz;
   Analyzer::AcResult result = Analyzer::SolveAc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   Expect(result.points.empty() == false, "ac point exists");
   if (result.points.empty())
   {
      return;
   }
   ExpectNear(NetVoltageOfPin(circuit, result.bias, inverter, 1), ExpectedVolt, 1.0e-4, "frozen inverter bias");
   Analyzer::Netlist netlist;
   Expect(Analyzer::IsOk(Analyzer::BuildNetlist(circuit, &netlist)), "ac netlist");
   uint32_t inputNet = 0;
   uint32_t outputNet = 0;
   for (const Analyzer::NetlistDevice& device : netlist.devices)
   {
      if (device.componentId == source && device.pinCount > 1)
      {
         inputNet = device.nets[1];
      }
      if (device.componentId == inverter && device.pinCount > 1)
      {
         outputNet = device.nets[1];
      }
   }
   const Analyzer::AcPoint& point = result.points.front();
   if (inputNet >= point.netVoltageReal.size() || outputNet >= point.netVoltageReal.size())
   {
      Expect(false, "ac nets present");
      return;
   }
   double inputMagnitude = std::hypot(point.netVoltageReal[inputNet], point.netVoltageImag[inputNet]);
   double outputMagnitude = std::hypot(point.netVoltageReal[outputNet], point.netVoltageImag[outputNet]);
   ExpectNear(inputMagnitude, AcAmplitudeVolt, 1.0e-4, "inverter input ac");
   ExpectNear(outputMagnitude, 0.0, 1.0e-4, "inverter output ac");
}

void TestLedConducts(void)
{
   constexpr double SupplyVolt = 5.0;
   constexpr double SeriesOhm = 220.0;
   constexpr double MinimumCurrentAmpere = 0.001;
   Analyzer::Circuit circuit;
   Place(&circuit, Analyzer::ComponentKind::Ground, 0, 0, 0);
   uint64_t source = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   uint64_t series = Place(&circuit, Analyzer::ComponentKind::Resistor, 0, 2, 0);
   uint64_t led = Place(&circuit, Analyzer::ComponentKind::Led, 2, 2, 0);
   Place(&circuit, Analyzer::ComponentKind::Ground, 4, 2, 0);
   SetNumber(&circuit, source, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   SetNumber(&circuit, series, Analyzer::NumericProperty::ResistanceOhm, SeriesOhm);
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   double current = BranchCurrent(result, series);
   double forward = NetVoltageOfPin(circuit, result, led, 0) - NetVoltageOfPin(circuit, result, led, 1);
   Expect(current > MinimumCurrentAmpere, "led conducts");
   Expect(forward > 1.5, "led is forward biased");
}

void TestSwitchDivider(void)
{
   constexpr double SupplyVolt = 10.0;
   constexpr double DividerOhm = 1000.0;
   constexpr double ClosedOhm = 100.0;
   constexpr int32_t ClosedCode = 1;
   constexpr int32_t ResistiveCode = 1;
   Analyzer::Circuit circuit;
   Place(&circuit, Analyzer::ComponentKind::Ground, 0, 0, 0);
   uint64_t source = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   uint64_t top = Place(&circuit, Analyzer::ComponentKind::Resistor, 0, 2, 0);
   uint64_t bottom = Place(&circuit, Analyzer::ComponentKind::Resistor, 2, 2, 270);
   uint64_t contact = Place(&circuit, Analyzer::ComponentKind::SpstSwitch, 2, 2, 270);
   SetNumber(&circuit, source, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   SetNumber(&circuit, top, Analyzer::NumericProperty::ResistanceOhm, DividerOhm);
   SetNumber(&circuit, bottom, Analyzer::NumericProperty::ResistanceOhm, DividerOhm);
   Analyzer::Wire wire = {};
   wire.points.push_back(Analyzer::GridPoint{2, 0});
   wire.points.push_back(Analyzer::GridPoint{0, 0});
   Expect(Analyzer::IsOk(circuit.AddWire(wire, nullptr)), "switch return wire");
   Analyzer::DcResult openResult = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(openResult.code), openResult.message);
   ExpectNear(NetVoltageOfPin(circuit, openResult, top, 1), SupplyVolt * 0.5, 1.0e-4, "open switch midpoint");
   SetCode(&circuit, contact, Analyzer::EnumProperty::RestState, ClosedCode);
   Analyzer::DcResult closedResult = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(closedResult.code), closedResult.message);
   ExpectNear(NetVoltageOfPin(circuit, closedResult, top, 1), 0.0, 1.0e-4, "ideal closed switch midpoint");
   ExpectNear(BranchCurrent(closedResult, top), SupplyVolt / DividerOhm, 1.0e-6, "ideal closed switch current");
   SetCode(&circuit, contact, Analyzer::EnumProperty::ClosedMode, ResistiveCode);
   SetNumber(&circuit, contact, Analyzer::NumericProperty::ClosedResistanceOhm, ClosedOhm);
   double parallelOhm = (ClosedOhm * DividerOhm) / (ClosedOhm + DividerOhm);
   double expectedMid = SupplyVolt * parallelOhm / (DividerOhm + parallelOhm);
   Analyzer::DcResult resistiveResult = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(resistiveResult.code), resistiveResult.message);
   ExpectNear(NetVoltageOfPin(circuit, resistiveResult, top, 1), expectedMid, 1.0e-4, "resistive closed switch midpoint");
}

void TestClockLed(void)
{
   constexpr double SeriesOhm = 220.0;
   constexpr double StopSec = 0.0015;
   constexpr double StepSec = 1.0e-5;
   constexpr double MinimumOnCurrent = 0.001;
   constexpr double MaximumOffCurrent = 0.0001;
   constexpr double LowWindowStartSec = 0.0005;
   constexpr double LowWindowStopSec = 0.0009;
   constexpr double HighWindowStartSec = 0.001;
   constexpr double HighWindowStopSec = 0.0014;
   Analyzer::Circuit circuit;
   circuit.Settings().stopTimeSec = StopSec;
   circuit.Settings().maximumStepSec = StepSec;
   uint64_t clock = Place(&circuit, Analyzer::ComponentKind::Clock, 0, 0, 0);
   uint64_t series = Place(&circuit, Analyzer::ComponentKind::Resistor, 4, 0, 0);
   Place(&circuit, Analyzer::ComponentKind::Led, 6, 0, 0);
   Place(&circuit, Analyzer::ComponentKind::Ground, 8, 0, 0);
   SetNumber(&circuit, series, Analyzer::NumericProperty::ResistanceOhm, SeriesOhm);
   SetNumber(&circuit, clock, Analyzer::NumericProperty::PropagationDelaySec, 0.0);
   Analyzer::TransientResult result = Analyzer::SolveTransient(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   Analyzer::Netlist netlist;
   Expect(Analyzer::IsOk(Analyzer::BuildNetlist(circuit, &netlist)), "clock led netlist");
   uint32_t leftNet = 0;
   uint32_t rightNet = 0;
   for (const Analyzer::NetlistDevice& device : netlist.devices)
   {
      if (device.componentId == series && device.pinCount >= 2)
      {
         leftNet = device.nets[0];
         rightNet = device.nets[1];
      }
   }
   bool sawOff = false;
   bool sawSecondOn = false;
   double firstCurrent = 0.0;
   if (result.samples.empty() == false && leftNet < result.samples.front().netVoltage.size() && rightNet < result.samples.front().netVoltage.size())
   {
      firstCurrent = std::abs(result.samples.front().netVoltage[leftNet] - result.samples.front().netVoltage[rightNet]) / SeriesOhm;
   }
   for (const Analyzer::TransientSample& sample : result.samples)
   {
      if (leftNet >= sample.netVoltage.size() || rightNet >= sample.netVoltage.size())
      {
         continue;
      }
      double current = std::abs(sample.netVoltage[leftNet] - sample.netVoltage[rightNet]) / SeriesOhm;
      if (sample.timeSec >= LowWindowStartSec && sample.timeSec < LowWindowStopSec)
      {
         sawOff = current < MaximumOffCurrent;
      }
      if (sample.timeSec >= HighWindowStartSec && sample.timeSec < HighWindowStopSec)
      {
         sawSecondOn = current > MinimumOnCurrent;
      }
   }
   Expect(firstCurrent > MinimumOnCurrent, "clock led conducts while high");
   Expect(sawOff, "clock led is off while low");
   Expect(sawSecondOn, "clock led conducts again");
}

} // namespace

int RunMixedTests(void)
{
   gFailures = 0;
   TestInverterLoad();
   TestInverterAcHasNoGain();
   TestLedConducts();
   TestSwitchDivider();
   TestClockLed();
   return gFailures;
}
