/*!
 * \file test_devices.cpp
 * \brief DC checks for the display, comparator, relay, converters, switches, and 555.
 */

#include "core/netlist_builder.h"
#include "solver/simulator.h"

#include <array>
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

void AddGround(Analyzer::Circuit* pCircuit, int32_t positionX, int32_t positionY)
{
   Place(pCircuit, Analyzer::ComponentKind::Ground, positionX, positionY, 0);
}

void AddSegment(Analyzer::Circuit* pCircuit, int32_t startX, int32_t startY, int32_t endX, int32_t endY)
{
   Analyzer::Wire wire = {};
   wire.points.push_back(Analyzer::GridPoint{startX, startY});
   wire.points.push_back(Analyzer::GridPoint{endX, endY});
   Expect(Analyzer::IsOk(pCircuit->AddWire(wire, nullptr)), "wire");
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

void TestSegmentCurrent(void)
{
   constexpr double SupplyVolt = 5.0;
   constexpr double MinimumCurrentAmpere = 0.005;
   Analyzer::Circuit circuit;
   AddGround(&circuit, 0, 0);
   AddGround(&circuit, 4, 2);
   uint64_t source = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   Place(&circuit, Analyzer::ComponentKind::SevenSegment, 0, 2, 0);
   SetNumber(&circuit, source, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   double current = BranchCurrent(result, source);
   Expect(current > MinimumCurrentAmpere, "segment current");
   Expect(current < 0.02, "segment current stays limited");
}

void TestComparatorHigh(void)
{
   constexpr double SupplyVolt = 5.0;
   constexpr double PlusVolt = 3.0;
   constexpr double MinusVolt = 1.0;
   constexpr double LoadOhm = 1000.0;
   constexpr double OutputOhm = 100.0;
   constexpr double ExpectedVolt = SupplyVolt * LoadOhm / (LoadOhm + OutputOhm);
   Analyzer::Circuit circuit;
   AddGround(&circuit, 0, 2);
   AddGround(&circuit, -2, 6);
   AddGround(&circuit, 4, 2);
   AddGround(&circuit, 4, 6);
   AddGround(&circuit, 0, 10);
   uint64_t plus = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 2, 90);
   uint64_t minus = Place(&circuit, Analyzer::ComponentKind::VoltageSource, -2, 6, 0);
   uint64_t supply = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 4, 2, 90);
   Place(&circuit, Analyzer::ComponentKind::Comparator, 0, 4, 0);
   uint64_t load = Place(&circuit, Analyzer::ComponentKind::Resistor, 0, 8, 90);
   SetNumber(&circuit, plus, Analyzer::NumericProperty::DcVoltageVolt, PlusVolt);
   SetNumber(&circuit, minus, Analyzer::NumericProperty::DcVoltageVolt, MinusVolt);
   SetNumber(&circuit, supply, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   SetNumber(&circuit, load, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   ExpectNear(NetVoltageOfPin(circuit, result, load, 0), ExpectedVolt, 1.0e-3, "comparator high output");
}

void TestRelayContact(void)
{
   constexpr double CoilVolt = 5.0;
   constexpr double ContactVolt = 10.0;
   constexpr double LoadOhm = 1000.0;
   Analyzer::Circuit circuit;
   AddGround(&circuit, 0, 0);
   AddGround(&circuit, 4, -2);
   AddGround(&circuit, 8, 2);
   uint64_t coil = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   uint64_t contact = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 4, -2, 90);
   Place(&circuit, Analyzer::ComponentKind::Relay, 0, 0, 0);
   uint64_t load = Place(&circuit, Analyzer::ComponentKind::Resistor, 6, 2, 0);
   Analyzer::Wire wire = {};
   wire.points.push_back(Analyzer::GridPoint{4, 2});
   wire.points.push_back(Analyzer::GridPoint{6, 2});
   Expect(Analyzer::IsOk(circuit.AddWire(wire, nullptr)), "relay contact wire");
   SetNumber(&circuit, coil, Analyzer::NumericProperty::DcVoltageVolt, CoilVolt);
   SetNumber(&circuit, contact, Analyzer::NumericProperty::DcVoltageVolt, ContactVolt);
   SetNumber(&circuit, load, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   ExpectNear(BranchCurrent(result, coil), CoilVolt / 100.0, 1.0e-4, "relay coil current");
   ExpectNear(NetVoltageOfPin(circuit, result, load, 0), ContactVolt, 1.0e-2, "relay normally open voltage");
}

void TestDacCode(void)
{
   constexpr double BitVolt = 5.0;
   constexpr double ReferenceVolt = 15.0;
   constexpr double LoadOhm = 1000.0;
   constexpr double OutputOhm = 100.0;
   constexpr double TheveninVolt = ReferenceVolt / 15.0;
   constexpr double ExpectedVolt = TheveninVolt * LoadOhm / (LoadOhm + OutputOhm);
   Analyzer::Circuit circuit;
   AddGround(&circuit, 0, -2);
   AddGround(&circuit, 0, 2);
   AddGround(&circuit, 0, 4);
   AddGround(&circuit, 0, 6);
   AddGround(&circuit, -2, 8);
   AddGround(&circuit, 6, 0);
   uint64_t bit = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, -2, 90);
   uint64_t reference = Place(&circuit, Analyzer::ComponentKind::VoltageSource, -2, 8, 0);
   Place(&circuit, Analyzer::ComponentKind::Dac, 0, 0, 0);
   uint64_t load = Place(&circuit, Analyzer::ComponentKind::Resistor, 4, 0, 0);
   SetNumber(&circuit, bit, Analyzer::NumericProperty::DcVoltageVolt, BitVolt);
   SetNumber(&circuit, reference, Analyzer::NumericProperty::DcVoltageVolt, ReferenceVolt);
   SetNumber(&circuit, load, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   ExpectNear(NetVoltageOfPin(circuit, result, load, 0), ExpectedVolt, 1.0e-3, "dac code 1");
}

void TestAdcCode(void)
{
   constexpr double InputVolt = 5.0;
   constexpr double ReferenceVolt = 8.0;
   constexpr double LoadOhm = 1000.0;
   constexpr double OutputOhm = 100.0;
   constexpr double HighVolt = 5.0 * LoadOhm / (LoadOhm + OutputOhm);
   Analyzer::Circuit circuit;
   AddGround(&circuit, 0, -2);
   AddGround(&circuit, -2, 2);
   AddGround(&circuit, 6, 0);
   AddGround(&circuit, 6, 2);
   AddGround(&circuit, 6, 4);
   AddGround(&circuit, 6, 6);
   uint64_t input = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, -2, 90);
   uint64_t reference = Place(&circuit, Analyzer::ComponentKind::VoltageSource, -2, 2, 0);
   uint64_t converter = Place(&circuit, Analyzer::ComponentKind::Adc, 0, 0, 0);
   uint64_t load0 = Place(&circuit, Analyzer::ComponentKind::Resistor, 4, 0, 0);
   uint64_t load1 = Place(&circuit, Analyzer::ComponentKind::Resistor, 4, 2, 0);
   uint64_t load2 = Place(&circuit, Analyzer::ComponentKind::Resistor, 4, 4, 0);
   uint64_t load3 = Place(&circuit, Analyzer::ComponentKind::Resistor, 4, 6, 0);
   SetNumber(&circuit, input, Analyzer::NumericProperty::DcVoltageVolt, InputVolt);
   SetNumber(&circuit, reference, Analyzer::NumericProperty::DcVoltageVolt, ReferenceVolt);
   SetNumber(&circuit, converter, Analyzer::NumericProperty::OutputResistanceOhm, OutputOhm);
   SetNumber(&circuit, load0, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   SetNumber(&circuit, load1, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   SetNumber(&circuit, load2, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   SetNumber(&circuit, load3, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   ExpectNear(NetVoltageOfPin(circuit, result, converter, 3), 0.0, 1.0e-3, "adc D0 low");
   ExpectNear(NetVoltageOfPin(circuit, result, converter, 4), HighVolt, 1.0e-3, "adc D1 high");
   ExpectNear(NetVoltageOfPin(circuit, result, converter, 5), 0.0, 1.0e-3, "adc D2 low");
   ExpectNear(NetVoltageOfPin(circuit, result, converter, 6), HighVolt, 1.0e-3, "adc D3 high");
}

void TestSpdtThrow(void)
{
   constexpr double SupplyVolt = 10.0;
   constexpr double SeriesOhm = 1000.0;
   constexpr double ExpectedCurrent = SupplyVolt / SeriesOhm;
   Analyzer::Circuit closedThrow;
   AddGround(&closedThrow, 0, 0);
   AddGround(&closedThrow, 6, 4);
   uint64_t source = Place(&closedThrow, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   uint64_t series = Place(&closedThrow, Analyzer::ComponentKind::Resistor, 0, 2, 0);
   Place(&closedThrow, Analyzer::ComponentKind::SpdtSwitch, 4, 2, 0);
   AddSegment(&closedThrow, 2, 2, 4, 2);
   SetNumber(&closedThrow, source, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   SetNumber(&closedThrow, series, Analyzer::NumericProperty::ResistanceOhm, SeriesOhm);
   Analyzer::DcResult closed = Analyzer::SolveDc(closedThrow, nullptr);
   Expect(Analyzer::IsOk(closed.code), closed.message);
   ExpectNear(NetVoltageOfPin(closedThrow, closed, series, 1), 0.0, 1.0e-3, "spdt throw A voltage");
   ExpectNear(BranchCurrent(closed, source), ExpectedCurrent, 1.0e-4, "spdt throw A current");

   Analyzer::Circuit openThrow;
   AddGround(&openThrow, 0, 0);
   uint64_t openSource = Place(&openThrow, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   uint64_t openSeries = Place(&openThrow, Analyzer::ComponentKind::Resistor, 0, 2, 0);
   uint64_t toggle = Place(&openThrow, Analyzer::ComponentKind::SpdtSwitch, 4, 2, 0);
   AddSegment(&openThrow, 2, 2, 4, 2);
   SetNumber(&openThrow, openSource, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   SetNumber(&openThrow, openSeries, Analyzer::NumericProperty::ResistanceOhm, SeriesOhm);
   SetCode(&openThrow, toggle, Analyzer::EnumProperty::RestThrow, static_cast<int32_t>(Analyzer::ThrowPosition::ThrowB));
   Analyzer::DcResult opened = Analyzer::SolveDc(openThrow, nullptr);
   Expect(Analyzer::IsOk(opened.code), opened.message);
   ExpectNear(NetVoltageOfPin(openThrow, opened, openSeries, 1), SupplyVolt, 1.0e-3, "spdt throw B voltage");
   ExpectNear(BranchCurrent(opened, openSource), 0.0, 1.0e-6, "spdt throw B current");
}

void TestDipChannel(void)
{
   constexpr double SupplyVolt = 10.0;
   constexpr double SeriesOhm = 1000.0;
   constexpr uint8_t ClosedChannel = 0;
   Analyzer::Circuit circuit;
   AddGround(&circuit, 0, 0);
   AddGround(&circuit, 8, 2);
   uint64_t source = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, 0, 90);
   uint64_t series = Place(&circuit, Analyzer::ComponentKind::Resistor, 0, 2, 0);
   uint64_t dip = Place(&circuit, Analyzer::ComponentKind::DipSwitch, 4, 2, 0);
   AddSegment(&circuit, 2, 2, 4, 2);
   SetNumber(&circuit, source, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   SetNumber(&circuit, series, Analyzer::NumericProperty::ResistanceOhm, SeriesOhm);
   Analyzer::Component* pDip = circuit.FindComponent(dip);
   Expect(pDip != nullptr, "find dip switch");
   if (pDip != nullptr)
   {
      Analyzer::DipSwitchProperties* pProperties = std::get_if<Analyzer::DipSwitchProperties>(&pDip->properties);
      Expect(pProperties != nullptr, "dip properties");
      if (pProperties != nullptr)
      {
         pProperties->channelState[ClosedChannel] = Analyzer::SwitchState::Closed;
      }
   }
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   ExpectNear(NetVoltageOfPin(circuit, result, series, 1), 0.0, 1.0e-3, "dip channel voltage");
   ExpectNear(BranchCurrent(result, source), SupplyVolt / SeriesOhm, 1.0e-4, "dip channel current");
}

void TestTimerSetAndClear(void)
{
   constexpr double SupplyVolt = 5.0;
   constexpr double LoadOhm = 1000.0;
   constexpr double OutputOhm = 100.0;
   constexpr double HighVolt = SupplyVolt * LoadOhm / (LoadOhm + OutputOhm);
   Analyzer::Circuit setCircuit;
   AddGround(&setCircuit, 0, -2);
   AddGround(&setCircuit, 0, 2);
   AddGround(&setCircuit, 0, 4);
   AddGround(&setCircuit, 0, 6);
   AddGround(&setCircuit, 6, 2);
   uint64_t supply = Place(&setCircuit, Analyzer::ComponentKind::VoltageSource, 0, -2, 90);
   Place(&setCircuit, Analyzer::ComponentKind::Timer555, 0, 0, 0);
   uint64_t load = Place(&setCircuit, Analyzer::ComponentKind::Resistor, 4, 2, 0);
   AddSegment(&setCircuit, 4, 4, 2, 4);
   AddSegment(&setCircuit, 2, 4, 2, 0);
   AddSegment(&setCircuit, 2, 0, 0, 0);
   SetNumber(&setCircuit, supply, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   SetNumber(&setCircuit, load, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   Analyzer::DcResult setResult = Analyzer::SolveDc(setCircuit, nullptr);
   Expect(Analyzer::IsOk(setResult.code), setResult.message);
   ExpectNear(NetVoltageOfPin(setCircuit, setResult, load, 0), HighVolt, 1.0e-3, "555 set output");

   Analyzer::Circuit clearCircuit;
   AddGround(&clearCircuit, 0, -2);
   AddGround(&clearCircuit, 0, 2);
   AddGround(&clearCircuit, -2, 4);
   AddGround(&clearCircuit, -2, 6);
   AddGround(&clearCircuit, 6, 2);
   uint64_t clearSupply = Place(&clearCircuit, Analyzer::ComponentKind::VoltageSource, 0, -2, 90);
   uint64_t trigger = Place(&clearCircuit, Analyzer::ComponentKind::VoltageSource, -2, 4, 0);
   uint64_t threshold = Place(&clearCircuit, Analyzer::ComponentKind::VoltageSource, -2, 6, 0);
   Place(&clearCircuit, Analyzer::ComponentKind::Timer555, 0, 0, 0);
   uint64_t clearLoad = Place(&clearCircuit, Analyzer::ComponentKind::Resistor, 4, 2, 0);
   AddSegment(&clearCircuit, 4, 4, 2, 4);
   AddSegment(&clearCircuit, 2, 4, 2, 0);
   AddSegment(&clearCircuit, 2, 0, 0, 0);
   SetNumber(&clearCircuit, clearSupply, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   SetNumber(&clearCircuit, trigger, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   SetNumber(&clearCircuit, threshold, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   SetNumber(&clearCircuit, clearLoad, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   Analyzer::DcResult clearResult = Analyzer::SolveDc(clearCircuit, nullptr);
   Expect(Analyzer::IsOk(clearResult.code), clearResult.message);
   ExpectNear(NetVoltageOfPin(clearCircuit, clearResult, clearLoad, 0), 0.0, 1.0e-3, "555 clear output");
}

uint32_t PinNet(const Analyzer::Circuit& circuit, uint64_t componentId, uint8_t pinIndex)
{
   Analyzer::Netlist netlist;
   if (Analyzer::IsErr(Analyzer::BuildNetlist(circuit, &netlist)))
   {
      return 0;
   }
   for (const Analyzer::NetlistDevice& device : netlist.devices)
   {
      if (device.componentId == componentId && pinIndex < device.pinCount)
      {
         return device.nets[pinIndex];
      }
   }
   return 0;
}

void TestTimerAstable(void)
{
   constexpr double SupplyVolt = 5.0;
   constexpr double ChargeOhm = 10000.0;
   constexpr double DischargeOhm = 10000.0;
   constexpr double OutputOhm = 1.0;
   constexpr double LoadOhm = 10000.0;
   constexpr double CapacitanceFarad = 100.0e-9;
   constexpr double Ln2 = 0.69314718056;
   constexpr double StopSec = 0.008;
   constexpr double StepSec = 5.0e-6;
   constexpr uint8_t OutputPin = 5;
   constexpr uint32_t EdgeSlots = 4;
   constexpr double PeriodTolerance = 0.05;
   const double dischargeThevenin = SupplyVolt * OutputOhm / (ChargeOhm + OutputOhm);
   const double dischargeResistance = DischargeOhm + ((ChargeOhm * OutputOhm) / (ChargeOhm + OutputOhm));
   const double highTarget = (2.0 / 3.0) * SupplyVolt;
   const double lowTarget = (1.0 / 3.0) * SupplyVolt;
   const double highTime = (ChargeOhm + DischargeOhm) * CapacitanceFarad * Ln2;
   const double lowRatio = (highTarget - dischargeThevenin) / (lowTarget - dischargeThevenin);
   const double lowTime = dischargeResistance * CapacitanceFarad * std::log(lowRatio);
   const double expectedPeriod = highTime + lowTime;
   Analyzer::Circuit circuit;
   circuit.Settings().stopTimeSec = StopSec;
   circuit.Settings().maximumStepSec = StepSec;
   AddGround(&circuit, 0, -2);
   AddGround(&circuit, 0, 2);
   AddGround(&circuit, 6, 2);
   uint64_t supply = Place(&circuit, Analyzer::ComponentKind::VoltageSource, 0, -2, 90);
   uint64_t timer = Place(&circuit, Analyzer::ComponentKind::Timer555, 0, 0, 0);
   uint64_t charge = Place(&circuit, Analyzer::ComponentKind::Resistor, 2, 0, 0);
   uint64_t discharge = Place(&circuit, Analyzer::ComponentKind::Resistor, 8, 6, 90);
   uint64_t capacitor = Place(&circuit, Analyzer::ComponentKind::Capacitor, 0, 2, 90);
   uint64_t load = Place(&circuit, Analyzer::ComponentKind::Resistor, 4, 2, 0);
   AddSegment(&circuit, 0, 0, 2, 0);
   AddSegment(&circuit, 4, 0, 8, 0);
   AddSegment(&circuit, 8, 0, 8, 6);
   AddSegment(&circuit, 8, 8, 0, 8);
   AddSegment(&circuit, 0, 8, 0, 6);
   AddSegment(&circuit, 0, 4, 0, 6);
   AddSegment(&circuit, 4, 4, 2, 4);
   AddSegment(&circuit, 2, 4, 2, 0);
   SetNumber(&circuit, supply, Analyzer::NumericProperty::DcVoltageVolt, SupplyVolt);
   SetNumber(&circuit, charge, Analyzer::NumericProperty::ResistanceOhm, ChargeOhm);
   SetNumber(&circuit, discharge, Analyzer::NumericProperty::ResistanceOhm, DischargeOhm);
   SetNumber(&circuit, capacitor, Analyzer::NumericProperty::CapacitanceFarad, CapacitanceFarad);
   SetNumber(&circuit, load, Analyzer::NumericProperty::ResistanceOhm, LoadOhm);
   SetNumber(&circuit, timer, Analyzer::NumericProperty::OutputResistanceOhm, OutputOhm);
   SetNumber(&circuit, timer, Analyzer::NumericProperty::PropagationDelaySec, 0.0);
   Analyzer::TransientResult result = Analyzer::SolveTransient(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   uint32_t outputNet = PinNet(circuit, timer, OutputPin);
   std::array<double, EdgeSlots> rises = {};
   uint32_t riseCount = 0;
   double previous = 0.0;
   bool havePrevious = false;
   for (const Analyzer::TransientSample& sample : result.samples)
   {
      if (outputNet >= sample.netVoltage.size())
      {
         continue;
      }
      double value = sample.netVoltage[outputNet];
      if (havePrevious && previous < (SupplyVolt * 0.5) && value >= (SupplyVolt * 0.5) && riseCount < EdgeSlots)
      {
         rises[riseCount] = sample.timeSec;
         riseCount += 1;
      }
      previous = value;
      havePrevious = true;
   }
   Expect(riseCount >= 3, "555 astable edges, count " + std::to_string(riseCount));
   if (riseCount >= 3)
   {
      ExpectNear(rises[2] - rises[1], expectedPeriod, expectedPeriod * PeriodTolerance, "555 astable period");
   }
}

} // namespace

int RunDeviceTests(void)
{
   gFailures = 0;
   TestSegmentCurrent();
   TestComparatorHigh();
   TestRelayContact();
   TestDacCode();
   TestAdcCode();
   TestSpdtThrow();
   TestDipChannel();
   TestTimerSetAndClear();
   TestTimerAstable();
   return gFailures;
}
