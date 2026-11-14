/*!
 * \file test_digital.cpp
 * \brief Truth tables and event timing for the logic kernel.
 */

#include "core/netlist_builder.h"
#include "solver/simulator.h"

#include <cmath>
#include <cstdint>
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

void ExpectLevel(Analyzer::LogicLevel actual, Analyzer::LogicLevel expected, std::string_view message)
{
   if (actual != expected)
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

uint64_t Place(Analyzer::Circuit* pCircuit, Analyzer::ComponentKind kind, int32_t positionX, int32_t positionY)
{
   Analyzer::Component component = Analyzer::CreateComponent(kind, Analyzer::GridPoint{positionX, positionY});
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

uint64_t PullOn(Analyzer::Circuit* pCircuit, Analyzer::ComponentKind kind, uint64_t hostId, uint8_t pinIndex)
{
   Analyzer::Component* pHost = pCircuit->FindComponent(hostId);
   Expect(pHost != nullptr, "host for pull");
   if (pHost == nullptr)
   {
      return 0;
   }
   Analyzer::GridPoint pin = Analyzer::PinPosition(*pHost, pinIndex);
   return Place(pCircuit, kind, pin.positionX, pin.positionY);
}

uint64_t ClockOn(Analyzer::Circuit* pCircuit, uint64_t hostId, uint8_t pinIndex)
{
   Analyzer::Component* pHost = pCircuit->FindComponent(hostId);
   Expect(pHost != nullptr, "host for clock");
   if (pHost == nullptr)
   {
      return 0;
   }
   Analyzer::GridPoint pin = Analyzer::PinPosition(*pHost, pinIndex);
   return Place(pCircuit, Analyzer::ComponentKind::Clock, pin.positionX - Analyzer::BoxWidth, pin.positionY);
}

uint32_t NetOf(const Analyzer::Circuit& circuit, uint64_t componentId, uint8_t pinIndex)
{
   Analyzer::Netlist netlist;
   if (Analyzer::IsErr(Analyzer::BuildNetlist(circuit, &netlist)))
   {
      return UINT32_MAX;
   }
   for (const Analyzer::NetlistDevice& device : netlist.devices)
   {
      if (device.componentId == componentId && pinIndex < device.pinCount)
      {
         return device.nets[pinIndex];
      }
   }
   return UINT32_MAX;
}

Analyzer::LogicLevel DcLevel(const Analyzer::Circuit& circuit, const Analyzer::DcResult& result, uint64_t componentId, uint8_t pinIndex)
{
   uint32_t net = NetOf(circuit, componentId, pinIndex);
   if (net >= result.netLevel.size())
   {
      return Analyzer::LogicLevel::Conflict;
   }
   return result.netLevel[net];
}

double FirstTime(const Analyzer::TransientResult& result, uint32_t net, Analyzer::LogicLevel level)
{
   for (const Analyzer::TransientSample& sample : result.samples)
   {
      if (net < sample.netLevel.size() && sample.netLevel[net] == level)
      {
         return sample.timeSec;
      }
   }
   return -1.0;
}

void Tie(Analyzer::Circuit* pCircuit, Analyzer::GridPoint from, Analyzer::GridPoint to)
{
   Analyzer::Wire wire = {};
   wire.points.push_back(from);
   wire.points.push_back(to);
   Expect(Analyzer::IsOk(pCircuit->AddWire(wire, nullptr)), "tie nets");
}

void TestPullFight(void)
{
   Analyzer::Circuit circuit;
   uint64_t pullUp = Place(&circuit, Analyzer::ComponentKind::PullUp, 0, 0);
   Place(&circuit, Analyzer::ComponentKind::PullDown, 0, 0);
   Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   ExpectLevel(DcLevel(circuit, result, pullUp, 0), Analyzer::LogicLevel::Conflict, "opposed pulls");
}

void TestNandXor(void)
{
   Analyzer::LogicLevel nandExpect[4] = {
      Analyzer::LogicLevel::High,
      Analyzer::LogicLevel::High,
      Analyzer::LogicLevel::High,
      Analyzer::LogicLevel::Low
   };
   Analyzer::LogicLevel xorExpect[4] = {
      Analyzer::LogicLevel::Low,
      Analyzer::LogicLevel::High,
      Analyzer::LogicLevel::High,
      Analyzer::LogicLevel::Low
   };
   for (int32_t code = 0; code < 4; code += 1)
   {
      Analyzer::Circuit nandCircuit;
      uint64_t nand = Place(&nandCircuit, Analyzer::ComponentKind::NandGate, 0, 0);
      if ((code & 1) != 0)
      {
         PullOn(&nandCircuit, Analyzer::ComponentKind::PullUp, nand, 0);
      }
      else
      {
         PullOn(&nandCircuit, Analyzer::ComponentKind::PullDown, nand, 0);
      }
      if ((code & 2) != 0)
      {
         PullOn(&nandCircuit, Analyzer::ComponentKind::PullUp, nand, 1);
      }
      else
      {
         PullOn(&nandCircuit, Analyzer::ComponentKind::PullDown, nand, 1);
      }
      Analyzer::DcResult nandResult = Analyzer::SolveDc(nandCircuit, nullptr);
      Expect(Analyzer::IsOk(nandResult.code), nandResult.message);
      ExpectLevel(DcLevel(nandCircuit, nandResult, nand, 2), nandExpect[code], "nand truth");

      Analyzer::Circuit xorCircuit;
      uint64_t exclusive = Place(&xorCircuit, Analyzer::ComponentKind::XorGate, 0, 0);
      if ((code & 1) != 0)
      {
         PullOn(&xorCircuit, Analyzer::ComponentKind::PullUp, exclusive, 0);
      }
      else
      {
         PullOn(&xorCircuit, Analyzer::ComponentKind::PullDown, exclusive, 0);
      }
      if ((code & 2) != 0)
      {
         PullOn(&xorCircuit, Analyzer::ComponentKind::PullUp, exclusive, 1);
      }
      else
      {
         PullOn(&xorCircuit, Analyzer::ComponentKind::PullDown, exclusive, 1);
      }
      Analyzer::DcResult xorResult = Analyzer::SolveDc(xorCircuit, nullptr);
      Expect(Analyzer::IsOk(xorResult.code), xorResult.message);
      ExpectLevel(DcLevel(xorCircuit, xorResult, exclusive, 2), xorExpect[code], "xor truth");
   }

   Analyzer::Circuit floating;
   uint64_t floatingNand = Place(&floating, Analyzer::ComponentKind::NandGate, 0, 0);
   PullOn(&floating, Analyzer::ComponentKind::PullUp, floatingNand, 1);
   Analyzer::DcResult floatingResult = Analyzer::SolveDc(floating, nullptr);
   Expect(Analyzer::IsOk(floatingResult.code), floatingResult.message);
   ExpectLevel(DcLevel(floating, floatingResult, floatingNand, 2), Analyzer::LogicLevel::Conflict, "floating nand input");
}

void TestTriStateAndOpenDrain(void)
{
   Analyzer::Circuit disabled;
   uint64_t buffer = Place(&disabled, Analyzer::ComponentKind::TriStateBuffer, 0, 0);
   PullOn(&disabled, Analyzer::ComponentKind::PullDown, buffer, 1);
   PullOn(&disabled, Analyzer::ComponentKind::PullUp, buffer, 0);
   Analyzer::DcResult disabledResult = Analyzer::SolveDc(disabled, nullptr);
   Expect(Analyzer::IsOk(disabledResult.code), disabledResult.message);
   ExpectLevel(DcLevel(disabled, disabledResult, buffer, 2), Analyzer::LogicLevel::HighZ, "tri-state disabled");

   Analyzer::Circuit enabled;
   uint64_t enabledBuffer = Place(&enabled, Analyzer::ComponentKind::TriStateBuffer, 0, 0);
   PullOn(&enabled, Analyzer::ComponentKind::PullUp, enabledBuffer, 1);
   PullOn(&enabled, Analyzer::ComponentKind::PullUp, enabledBuffer, 0);
   Analyzer::DcResult enabledResult = Analyzer::SolveDc(enabled, nullptr);
   Expect(Analyzer::IsOk(enabledResult.code), enabledResult.message);
   ExpectLevel(DcLevel(enabled, enabledResult, enabledBuffer, 2), Analyzer::LogicLevel::High, "tri-state enabled");

   Analyzer::Circuit unknown;
   uint64_t unknownBuffer = Place(&unknown, Analyzer::ComponentKind::TriStateBuffer, 0, 0);
   PullOn(&unknown, Analyzer::ComponentKind::PullUp, unknownBuffer, 1);
   Analyzer::DcResult unknownResult = Analyzer::SolveDc(unknown, nullptr);
   Expect(Analyzer::IsOk(unknownResult.code), unknownResult.message);
   ExpectLevel(DcLevel(unknown, unknownResult, unknownBuffer, 2), Analyzer::LogicLevel::Conflict, "tri-state floating data");

   Analyzer::Circuit released;
   uint64_t openHigh = Place(&released, Analyzer::ComponentKind::Inverter, 0, 0);
   SetCode(&released, openHigh, Analyzer::EnumProperty::OutputStage, static_cast<int32_t>(Analyzer::OutputStage::OpenDrain));
   PullOn(&released, Analyzer::ComponentKind::PullDown, openHigh, 0);
   PullOn(&released, Analyzer::ComponentKind::PullUp, openHigh, 1);
   Analyzer::DcResult releasedResult = Analyzer::SolveDc(released, nullptr);
   Expect(Analyzer::IsOk(releasedResult.code), releasedResult.message);
   ExpectLevel(DcLevel(released, releasedResult, openHigh, 1), Analyzer::LogicLevel::High, "open-drain released");

   Analyzer::Circuit clamped;
   uint64_t openLow = Place(&clamped, Analyzer::ComponentKind::Inverter, 0, 0);
   SetCode(&clamped, openLow, Analyzer::EnumProperty::OutputStage, static_cast<int32_t>(Analyzer::OutputStage::OpenDrain));
   PullOn(&clamped, Analyzer::ComponentKind::PullUp, openLow, 0);
   PullOn(&clamped, Analyzer::ComponentKind::PullUp, openLow, 1);
   Analyzer::DcResult clampedResult = Analyzer::SolveDc(clamped, nullptr);
   Expect(Analyzer::IsOk(clampedResult.code), clampedResult.message);
   ExpectLevel(DcLevel(clamped, clampedResult, openLow, 1), Analyzer::LogicLevel::Low, "open-drain low");

   Analyzer::Circuit fight;
   uint64_t highBuffer = Place(&fight, Analyzer::ComponentKind::Buffer, 0, 0);
   uint64_t lowBuffer = Place(&fight, Analyzer::ComponentKind::Buffer, 0, 2);
   PullOn(&fight, Analyzer::ComponentKind::PullUp, highBuffer, 0);
   PullOn(&fight, Analyzer::ComponentKind::PullDown, lowBuffer, 0);
   Analyzer::Component* pHigh = fight.FindComponent(highBuffer);
   Analyzer::Component* pLow = fight.FindComponent(lowBuffer);
   Expect(pHigh != nullptr && pLow != nullptr, "buffers exist");
   if (pHigh != nullptr && pLow != nullptr)
   {
      Tie(&fight, Analyzer::PinPosition(*pHigh, 1), Analyzer::PinPosition(*pLow, 1));
   }
   Analyzer::DcResult fightResult = Analyzer::SolveDc(fight, nullptr);
   Expect(Analyzer::IsOk(fightResult.code), fightResult.message);
   ExpectLevel(DcLevel(fight, fightResult, highBuffer, 1), Analyzer::LogicLevel::Conflict, "opposed push-pull");
}

void TestInverterDelay(void)
{
   Analyzer::Circuit circuit;
   uint64_t inverter = Place(&circuit, Analyzer::ComponentKind::Inverter, 0, 0);
   uint64_t clock = ClockOn(&circuit, inverter, 0);
   SetNumber(&circuit, clock, Analyzer::NumericProperty::FrequencyHz, 1000000.0);
   SetNumber(&circuit, clock, Analyzer::NumericProperty::PropagationDelaySec, 0.0);
   SetNumber(&circuit, inverter, Analyzer::NumericProperty::PropagationDelaySec, 10.0e-9);
   circuit.Settings().stopTimeSec = 2.0e-6;
   Analyzer::TransientResult result = Analyzer::SolveTransient(circuit, nullptr);
   Expect(Analyzer::IsErr(result.code) == false, result.message);
   uint32_t outputNet = NetOf(circuit, inverter, 1);
   double rise = FirstTime(result, outputNet, Analyzer::LogicLevel::High);
   ExpectNear(rise, 510.0e-9, 2.0e-9, "inverter delay");
}

void TestDFlipFlop(void)
{
   Analyzer::Circuit circuit;
   uint64_t flop = Place(&circuit, Analyzer::ComponentKind::DFlipFlop, 0, 0);
   PullOn(&circuit, Analyzer::ComponentKind::PullUp, flop, 0);
   uint64_t clock = ClockOn(&circuit, flop, 1);
   PullOn(&circuit, Analyzer::ComponentKind::PullDown, flop, 4);
   PullOn(&circuit, Analyzer::ComponentKind::PullDown, flop, 5);
   SetNumber(&circuit, clock, Analyzer::NumericProperty::PropagationDelaySec, 0.0);
   circuit.Settings().stopTimeSec = 0.002;
   Analyzer::TransientResult result = Analyzer::SolveTransient(circuit, nullptr);
   Expect(Analyzer::IsOk(result.code), result.message);
   uint32_t outputNet = NetOf(circuit, flop, 2);
   double rise = FirstTime(result, outputNet, Analyzer::LogicLevel::High);
   ExpectNear(rise, 0.001 + 10.0e-9, 2.0e-9, "d flip-flop clock to q");

   Analyzer::Circuit cleared;
   uint64_t clearedFlop = Place(&cleared, Analyzer::ComponentKind::DFlipFlop, 0, 0);
   PullOn(&cleared, Analyzer::ComponentKind::PullUp, clearedFlop, 0);
   uint64_t clearedClock = ClockOn(&cleared, clearedFlop, 1);
   PullOn(&cleared, Analyzer::ComponentKind::PullDown, clearedFlop, 4);
   PullOn(&cleared, Analyzer::ComponentKind::PullUp, clearedFlop, 5);
   SetNumber(&cleared, clearedClock, Analyzer::NumericProperty::PropagationDelaySec, 0.0);
   cleared.Settings().stopTimeSec = 0.002;
   Analyzer::TransientResult clearedResult = Analyzer::SolveTransient(cleared, nullptr);
   Expect(Analyzer::IsErr(clearedResult.code) == false, clearedResult.message);
   uint32_t clearedNet = NetOf(cleared, clearedFlop, 2);
   Expect(FirstTime(clearedResult, clearedNet, Analyzer::LogicLevel::High) < 0.0, "clear holds q low");

   Analyzer::Circuit setup;
   uint64_t setupFlop = Place(&setup, Analyzer::ComponentKind::DFlipFlop, 0, 0);
   uint64_t dataClock = ClockOn(&setup, setupFlop, 0);
   uint64_t setupClock = ClockOn(&setup, setupFlop, 1);
   PullOn(&setup, Analyzer::ComponentKind::PullDown, setupFlop, 4);
   PullOn(&setup, Analyzer::ComponentKind::PullDown, setupFlop, 5);
   SetNumber(&setup, dataClock, Analyzer::NumericProperty::PropagationDelaySec, 0.0);
   SetNumber(&setup, dataClock, Analyzer::NumericProperty::FrequencyHz, 1.0);
   SetNumber(&setup, dataClock, Analyzer::NumericProperty::DelaySec, 0.001 - 100.0e-9);
   SetNumber(&setup, setupClock, Analyzer::NumericProperty::PropagationDelaySec, 0.0);
   SetNumber(&setup, setupFlop, Analyzer::NumericProperty::SetupTimeSec, 1.0e-6);
   setup.Settings().stopTimeSec = 0.002;
   Analyzer::TransientResult setupResult = Analyzer::SolveTransient(setup, nullptr);
   Expect(Analyzer::IsErr(setupResult.code) == false, setupResult.message);
   Expect(setupResult.warnings.empty() == false, "setup warning");
   uint32_t setupNet = NetOf(setup, setupFlop, 2);
   ExpectNear(FirstTime(setupResult, setupNet, Analyzer::LogicLevel::Conflict), 0.001 + 10.0e-9, 2.0e-9, "setup forces conflict");
}

void TestCounterAndShift(void)
{
   Analyzer::Circuit counterCircuit;
   uint64_t counter = Place(&counterCircuit, Analyzer::ComponentKind::Counter, 0, 0);
   uint64_t counterClock = ClockOn(&counterCircuit, counter, 0);
   PullOn(&counterCircuit, Analyzer::ComponentKind::PullDown, counter, 1);
   PullOn(&counterCircuit, Analyzer::ComponentKind::PullUp, counter, 2);
   SetNumber(&counterCircuit, counterClock, Analyzer::NumericProperty::PropagationDelaySec, 0.0);
   counterCircuit.Settings().stopTimeSec = 0.0015;
   Analyzer::TransientResult counterResult = Analyzer::SolveTransient(counterCircuit, nullptr);
   Expect(Analyzer::IsOk(counterResult.code), counterResult.message);
   uint32_t countBit0 = NetOf(counterCircuit, counter, 3);
   uint32_t countBit1 = NetOf(counterCircuit, counter, 4);
   ExpectNear(FirstTime(counterResult, countBit0, Analyzer::LogicLevel::High), 0.001 + 10.0e-9, 2.0e-9, "counter increments");
   Expect(FirstTime(counterResult, countBit1, Analyzer::LogicLevel::High) < 0.0, "counter bit 1 still low");

   Analyzer::Circuit shiftCircuit;
   uint64_t shift = Place(&shiftCircuit, Analyzer::ComponentKind::ShiftRegister, 0, 0);
   uint64_t shiftClock = ClockOn(&shiftCircuit, shift, 0);
   PullOn(&shiftCircuit, Analyzer::ComponentKind::PullDown, shift, 1);
   PullOn(&shiftCircuit, Analyzer::ComponentKind::PullUp, shift, 2);
   SetNumber(&shiftCircuit, shiftClock, Analyzer::NumericProperty::PropagationDelaySec, 0.0);
   shiftCircuit.Settings().stopTimeSec = 0.0015;
   Analyzer::TransientResult shiftResult = Analyzer::SolveTransient(shiftCircuit, nullptr);
   Expect(Analyzer::IsOk(shiftResult.code), shiftResult.message);
   uint32_t shiftBit0 = NetOf(shiftCircuit, shift, 3);
   uint32_t shiftBit1 = NetOf(shiftCircuit, shift, 4);
   ExpectNear(FirstTime(shiftResult, shiftBit0, Analyzer::LogicLevel::High), 0.001 + 10.0e-9, 2.0e-9, "shift enters q0");
   Expect(FirstTime(shiftResult, shiftBit1, Analyzer::LogicLevel::High) < 0.0, "shift q1 still low");
}

void TestBcdGlyphs(void)
{
   constexpr uint8_t codes[4] = {0, 1, 8, 10};
   constexpr uint8_t glyphs[4] = {0x3F, 0x06, 0x7F, 0x00};
   for (uint8_t index = 0; index < 4; index += 1)
   {
      Analyzer::Circuit circuit;
      uint64_t decoder = Place(&circuit, Analyzer::ComponentKind::BcdToSevenSegment, 0, 0);
      for (uint8_t bit = 0; bit < 4; bit += 1)
      {
         Analyzer::ComponentKind pull = (codes[index] & static_cast<uint8_t>(1u << bit)) != 0
            ? Analyzer::ComponentKind::PullUp
            : Analyzer::ComponentKind::PullDown;
         PullOn(&circuit, pull, decoder, bit);
      }
      Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
      Expect(Analyzer::IsOk(result.code), result.message);
      for (uint8_t segment = 0; segment < 7; segment += 1)
      {
         Analyzer::LogicLevel expected = (glyphs[index] & static_cast<uint8_t>(1u << segment)) != 0
            ? Analyzer::LogicLevel::High
            : Analyzer::LogicLevel::Low;
         ExpectLevel(DcLevel(circuit, result, decoder, static_cast<uint8_t>(4 + segment)), expected, "bcd glyph");
      }
   }
}

} // namespace

int RunDigitalTests(void)
{
   TestPullFight();
   TestNandXor();
   TestTriStateAndOpenDrain();
   TestInverterDelay();
   TestDFlipFlop();
   TestCounterAndShift();
   TestBcdGlyphs();
   return gFailures;
}
