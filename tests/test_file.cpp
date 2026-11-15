/*!
 * \file test_file.cpp
 * \brief Round-trip checks for the .eca.json circuit document.
 */

#include "core/circuit_file.h"

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

const Analyzer::Component* FindKind(const Analyzer::Circuit& circuit, Analyzer::ComponentKind kind)
{
   for (const Analyzer::Component& component : circuit.Components())
   {
      if (component.kind == kind)
      {
         return &component;
      }
   }
   return nullptr;
}

void TestRoundTrip(void)
{
   constexpr double FirstEventSec = 0.001;
   constexpr double SecondEventSec = 0.004;
   constexpr double StopSec = 0.02;
   Analyzer::Circuit circuit;
   circuit.SetName("Saved latch");
   circuit.Settings().kind = Analyzer::AnalysisKind::Transient;
   circuit.Settings().stopTimeSec = StopSec;
   circuit.Settings().initialConditions = Analyzer::InitialConditionMode::ComponentValues;
   Analyzer::Component flop = Analyzer::CreateComponent(Analyzer::ComponentKind::DFlipFlop, Analyzer::GridPoint{2, 4});
   flop.rotationDegrees = 90;
   Expect(Analyzer::IsOk(Analyzer::WriteEnumCode(&flop, Analyzer::EnumProperty::InitialState, static_cast<int32_t>(Analyzer::LogicLevel::High))), "set initial state");
   Expect(Analyzer::IsOk(Analyzer::WriteEnumCode(&flop, Analyzer::EnumProperty::LogicFamily, static_cast<int32_t>(Analyzer::LogicFamily::Ttl))), "set logic family");
   Expect(Analyzer::IsOk(circuit.AddComponent(flop, nullptr)), "add flip-flop");
   Analyzer::Component contact = Analyzer::CreateComponent(Analyzer::ComponentKind::SpstSwitch, Analyzer::GridPoint{0, 0});
   Analyzer::SpstProperties* pSwitch = std::get_if<Analyzer::SpstProperties>(&contact.properties);
   Expect(pSwitch != nullptr, "switch properties");
   if (pSwitch != nullptr)
   {
      pSwitch->events.push_back(Analyzer::SwitchEvent{FirstEventSec, Analyzer::SwitchState::Closed});
      pSwitch->events.push_back(Analyzer::SwitchEvent{SecondEventSec, Analyzer::SwitchState::Open});
   }
   Expect(Analyzer::IsOk(circuit.AddComponent(contact, nullptr)), "add switch");
   Analyzer::Wire wire = {};
   wire.name = "return";
   wire.points.push_back(Analyzer::GridPoint{0, 0});
   wire.points.push_back(Analyzer::GridPoint{4, 0});
   Expect(Analyzer::IsOk(circuit.AddWire(wire, nullptr)), "add wire");
   Analyzer::NetLabel label = {};
   label.name = "OUT";
   label.position = Analyzer::GridPoint{4, 2};
   Expect(Analyzer::IsOk(circuit.AddNetLabel(label, nullptr)), "add label");

   std::string text;
   Expect(Analyzer::IsOk(Analyzer::WriteCircuitText(circuit, &text)), "write circuit");
   Analyzer::Circuit loaded;
   loaded.SetName("untouched");
   const Analyzer::Result read = Analyzer::ReadCircuitText(text, &loaded);
   Expect(Analyzer::IsOk(read), std::string("read circuit ") + Analyzer::Describe(read));
   Expect(loaded.Name() == "Saved latch", "circuit name");
   Expect(loaded.Settings().kind == Analyzer::AnalysisKind::Transient, "analysis kind");
   Expect(loaded.Settings().stopTimeSec == StopSec, "stop time");
   Expect(loaded.Settings().initialConditions == Analyzer::InitialConditionMode::ComponentValues, "initial conditions");
   const Analyzer::Component* pFlop = FindKind(loaded, Analyzer::ComponentKind::DFlipFlop);
   Expect(pFlop != nullptr, "loaded flip-flop");
   if (pFlop != nullptr)
   {
      int32_t initial = -1;
      int32_t family = -1;
      Expect(Analyzer::IsOk(Analyzer::ReadEnumCode(*pFlop, Analyzer::EnumProperty::InitialState, &initial)), "read initial state");
      Expect(Analyzer::IsOk(Analyzer::ReadEnumCode(*pFlop, Analyzer::EnumProperty::LogicFamily, &family)), "read logic family");
      Expect(initial == static_cast<int32_t>(Analyzer::LogicLevel::High), "initial state survived");
      Expect(family == static_cast<int32_t>(Analyzer::LogicFamily::Ttl), "logic family survived");
      Expect(pFlop->rotationDegrees == 90, "rotation survived");
   }
   const Analyzer::Component* pContact = FindKind(loaded, Analyzer::ComponentKind::SpstSwitch);
   Expect(pContact != nullptr, "loaded switch");
   if (pContact != nullptr)
   {
      const Analyzer::SpstProperties* pProperties = std::get_if<Analyzer::SpstProperties>(&pContact->properties);
      Expect(pProperties != nullptr && pProperties->events.size() == 2, "switch event count");
      if (pProperties != nullptr && pProperties->events.size() == 2)
      {
         Expect(pProperties->events[0].timeSec == FirstEventSec, "first switch time");
         Expect(pProperties->events[0].state == Analyzer::SwitchState::Closed, "first switch state");
         Expect(pProperties->events[1].timeSec == SecondEventSec, "second switch time");
         Expect(pProperties->events[1].state == Analyzer::SwitchState::Open, "second switch state");
      }
   }
   Expect(loaded.Wires().size() == 1 && loaded.Wires().front().points.size() == 2, "wire survived");
   Expect(loaded.NetLabels().size() == 1 && loaded.NetLabels().front().name == "OUT", "label survived");

   Analyzer::Circuit preserved;
   preserved.SetName("keep");
   Expect(Analyzer::ReadCircuitText("{\"format\":\"electric-circuit-analyzer\",\"version\":2,\"components\":[]}", &preserved) == Analyzer::Result::UnknownVersion, "unknown version");
   Expect(preserved.Name() == "keep", "failed read keeps the circuit");
   Expect(Analyzer::ReadCircuitText("{\"format\":\"electric-circuit-analyzer\",\"version\":1,\"components\":[{\"kind\":\"memory\"}]}", &preserved) == Analyzer::Result::UnknownKind, "unknown kind");
   Expect(preserved.Name() == "keep", "unknown kind keeps the circuit");
}

} // namespace

int RunFileTests(void)
{
   gFailures = 0;
   TestRoundTrip();
   return gFailures;
}
