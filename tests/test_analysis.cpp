/*!
 * \file test_analysis.cpp
 * \brief Operating-point, sweep, and transient checks for linear and diode circuits.
 */

#include "core/netlist_builder.h"
#include "solver/simulator.h"

#include <cmath>
#include <iostream>
#include <numbers>
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

void TestVoltageDivider(void)
{
   Analyzer::Circuit circuit;
   Place(&circuit, Analyzer::ComponentKind::Ground, 0, 0, 0);
   uint64_t source = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   uint64_t top = Place(&circuit, Analyzer::ComponentKind::Resistor, 0, 2, 0);
   uint64_t bottom = Place(&circuit, Analyzer::ComponentKind::Resistor, 2, 2, 270);
   SetNumber(&circuit, source, Analyzer::NumericProperty::DcVoltageVolt, 10.0);
   SetNumber(&circuit, top, Analyzer::NumericProperty::ResistanceOhm, 1000.0);
   SetNumber(&circuit, bottom, Analyzer::NumericProperty::ResistanceOhm, 1000.0);
   Analyzer::Wire wire = {};
   wire.points.push_back(Analyzer::GridPoint{2, 0});
   wire.points.push_back(Analyzer::GridPoint{0, 0});
   Expect(Analyzer::IsOk(circuit.AddWire(wire, nullptr)), "add return wire");
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   ExpectNear(NetVoltageOfPin(circuit, result, top, 0), 10.0, 1.0e-6, "divider source node");
   ExpectNear(NetVoltageOfPin(circuit, result, top, 1), 5.0, 1.0e-6, "divider midpoint");
   ExpectNear(BranchCurrent(result, top), 0.005, 1.0e-8, "divider current");
}

void TestMissingGround(void)
{
   Analyzer::Circuit circuit;
   uint64_t source = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   uint64_t resistor = Place(&circuit, Analyzer::ComponentKind::Resistor, 0, 2, 0);
   SetNumber(&circuit, source, Analyzer::NumericProperty::DcVoltageVolt, 5.0);
   SetNumber(&circuit, resistor, Analyzer::NumericProperty::ResistanceOhm, 1000.0);
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(result.code == Analyzer::Result::MissingGround, "missing ground");
}

void TestDiodeKnee(void)
{
   Analyzer::Circuit circuit;
   Place(&circuit, Analyzer::ComponentKind::Ground, 0, 0, 0);
   uint64_t source = Place(&circuit, Analyzer::ComponentKind::CurrentSource, 0, 0, 90);
   uint64_t diode = Place(&circuit, Analyzer::ComponentKind::Diode, 0, 2, 270);
   SetNumber(&circuit, source, Analyzer::NumericProperty::DcCurrentAmpere, 0.001);
   SetNumber(&circuit, diode, Analyzer::NumericProperty::ForwardVoltageVolt, 0.7);
   SetNumber(&circuit, diode, Analyzer::NumericProperty::ForwardCurrentAmpere, 0.001);
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   ExpectNear(NetVoltageOfPin(circuit, result, diode, 0), 0.7, 1.0e-3, "diode knee voltage");
}

void TestRcCutoff(void)
{
   Analyzer::Circuit circuit;
   Place(&circuit, Analyzer::ComponentKind::Ground, 0, 0, 0);
   uint64_t source = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   uint64_t resistor = Place(&circuit, Analyzer::ComponentKind::Resistor, 0, 2, 0);
   uint64_t capacitor = Place(&circuit, Analyzer::ComponentKind::Capacitor, 2, 2, 270);
   SetNumber(&circuit, source, Analyzer::NumericProperty::DcVoltageVolt, 0.0);
   SetNumber(&circuit, source, Analyzer::NumericProperty::AcAmplitudeVolt, 1.0);
   SetNumber(&circuit, resistor, Analyzer::NumericProperty::ResistanceOhm, 1000.0);
   SetNumber(&circuit, capacitor, Analyzer::NumericProperty::CapacitanceFarad, 1.0e-6);
   Analyzer::Wire wire = {};
   wire.points.push_back(Analyzer::GridPoint{2, 0});
   wire.points.push_back(Analyzer::GridPoint{0, 0});
   Expect(Analyzer::IsOk(circuit.AddWire(wire, nullptr)), "rc return wire");
   double cutoff = 1.0 / (2.0 * std::numbers::pi * 1000.0 * 1.0e-6);
   circuit.Settings().sweepScale = Analyzer::SweepScale::Linear;
   circuit.Settings().sweepStartHz = cutoff;
   circuit.Settings().sweepStopHz = cutoff;
   circuit.Settings().linearPointCount = 2;
   Analyzer::AcResult result = Analyzer::SolveAc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   Expect(result.points.empty() == false, "ac produced a point");
   if (result.points.empty())
   {
      return;
   }
   Analyzer::Netlist netlist;
   Expect(Analyzer::IsOk(Analyzer::BuildNetlist(circuit, &netlist)), "rc netlist");
   uint32_t outputNet = 0;
   for (const Analyzer::NetlistDevice& device : netlist.devices)
   {
      if (device.componentId == capacitor)
      {
         outputNet = device.nets[0];
      }
   }
   double real = result.points.front().netVoltageReal[outputNet];
   double imag = result.points.front().netVoltageImag[outputNet];
   double magnitude = std::sqrt((real * real) + (imag * imag));
   ExpectNear(magnitude, 1.0 / std::sqrt(2.0), 1.0e-4, "rc cutoff magnitude");
}

void TestRcStep(void)
{
   Analyzer::Circuit circuit;
   Place(&circuit, Analyzer::ComponentKind::Ground, 0, 0, 0);
   uint64_t source = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   uint64_t resistor = Place(&circuit, Analyzer::ComponentKind::Resistor, 0, 2, 0);
   uint64_t capacitor = Place(&circuit, Analyzer::ComponentKind::Capacitor, 2, 2, 270);
   SetNumber(&circuit, source, Analyzer::NumericProperty::DcVoltageVolt, 0.0);
   SetNumber(&circuit, resistor, Analyzer::NumericProperty::ResistanceOhm, 1000.0);
   SetNumber(&circuit, capacitor, Analyzer::NumericProperty::CapacitanceFarad, 1.0e-6);
   Analyzer::Component* pSource = circuit.FindComponent(source);
   Expect(pSource != nullptr, "source exists");
   if (pSource != nullptr)
   {
      Expect(Analyzer::IsOk(Analyzer::WriteEnumCode(pSource, Analyzer::EnumProperty::TransientWaveform, 2)), "pulse waveform");
   }
   SetNumber(&circuit, source, Analyzer::NumericProperty::PulseInitial, 0.0);
   SetNumber(&circuit, source, Analyzer::NumericProperty::PulseLevel, 1.0);
   SetNumber(&circuit, source, Analyzer::NumericProperty::PulseDelaySec, 0.0);
   SetNumber(&circuit, source, Analyzer::NumericProperty::PulseRiseSec, 0.0);
   SetNumber(&circuit, source, Analyzer::NumericProperty::PulseFallSec, 0.0);
   SetNumber(&circuit, source, Analyzer::NumericProperty::PulseWidthSec, 0.01);
   SetNumber(&circuit, source, Analyzer::NumericProperty::PulsePeriodSec, 0.02);
   Analyzer::Wire wire = {};
   wire.points.push_back(Analyzer::GridPoint{2, 0});
   wire.points.push_back(Analyzer::GridPoint{0, 0});
   circuit.AddWire(wire, nullptr);
   circuit.Settings().integration = Analyzer::IntegrationMethod::Trapezoidal;
   circuit.Settings().stopTimeSec = 0.005;
   circuit.Settings().maximumStepSec = 1.0e-5;
   Analyzer::TransientResult result = Analyzer::SolveTransient(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   Analyzer::Netlist netlist;
   Analyzer::BuildNetlist(circuit, &netlist);
   uint32_t outputNet = 0;
   for (const Analyzer::NetlistDevice& device : netlist.devices)
   {
      if (device.componentId == capacitor)
      {
         outputNet = device.nets[0];
      }
   }
   double atTimeConstant = 0.0;
   double atEnd = 0.0;
   for (const Analyzer::TransientSample& sample : result.samples)
   {
      if (sample.timeSec <= 0.001)
      {
         atTimeConstant = sample.netVoltage[outputNet];
      }
      atEnd = sample.netVoltage[outputNet];
   }
   ExpectNear(atTimeConstant, 1.0 - std::exp(-1.0), 0.02, "rc step at one time constant");
   Expect(atEnd > 0.98, "rc step approaches the final value");
}

void TestNmosSaturation(void)
{
   Analyzer::Circuit circuit;
   Place(&circuit, Analyzer::ComponentKind::Ground, 0, 2, 0);
   uint64_t gateDrive = Place(&circuit, Analyzer::ComponentKind::VoltageSource, -2, 2, 270);
   uint64_t drainDrive = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 0, 270);
   uint64_t mosfet = Place(&circuit, Analyzer::ComponentKind::Nmos, 0, 0, 0);
   Analyzer::Wire gateReturn = {};
   gateReturn.points.push_back(Analyzer::GridPoint{-2, 2});
   gateReturn.points.push_back(Analyzer::GridPoint{0, 2});
   Analyzer::Wire drainReturn = {};
   drainReturn.points.push_back(Analyzer::GridPoint{0, 0});
   drainReturn.points.push_back(Analyzer::GridPoint{0, 2});
   Expect(Analyzer::IsOk(circuit.AddWire(gateReturn, nullptr)), "gate return");
   Expect(Analyzer::IsOk(circuit.AddWire(drainReturn, nullptr)), "drain return");
   SetNumber(&circuit, gateDrive, Analyzer::NumericProperty::DcVoltageVolt, 3.0);
   SetNumber(&circuit, drainDrive, Analyzer::NumericProperty::DcVoltageVolt, 5.0);
   SetNumber(&circuit, mosfet, Analyzer::NumericProperty::ThresholdVoltageVolt, 1.0);
   SetNumber(&circuit, mosfet, Analyzer::NumericProperty::Kp, 0.002);
   SetNumber(&circuit, mosfet, Analyzer::NumericProperty::ChannelWidthMeter, 1.0);
   SetNumber(&circuit, mosfet, Analyzer::NumericProperty::ChannelLengthMeter, 1.0);
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   ExpectNear(BranchCurrent(result, drainDrive), 0.004, 1.0e-6, "nmos saturation current");
}

} // namespace

int RunAnalysisTests(void)
{
   TestVoltageDivider();
   TestMissingGround();
   TestDiodeKnee();
   TestRcCutoff();
   TestRcStep();
   TestNmosSaturation();
   return gFailures;
}
