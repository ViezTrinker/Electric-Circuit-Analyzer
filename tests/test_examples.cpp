/*!
 * \file test_examples.cpp
 * \brief Checks part movement and the example schematics.
 */

#include "core/circuit_file.h"
#include "core/example_circuits.h"
#include "core/netlist_builder.h"
#include "solver/simulator.h"

#include <cmath>
#include <filesystem>
#include <fstream>
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

bool IsOrthogonal(const Analyzer::Wire& wire)
{
   for (size_t index = 1; index < wire.points.size(); index += 1)
   {
      const Analyzer::GridPoint previous = wire.points[index - 1];
      const Analyzer::GridPoint next = wire.points[index];
      if (previous.positionX != next.positionX && previous.positionY != next.positionY)
      {
         return false;
      }
   }
   return wire.points.size() >= 2;
}

double PinVoltage(const Analyzer::Circuit& circuit, const Analyzer::DcResult& result, uint64_t componentId, uint8_t pinIndex)
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

Analyzer::LogicLevel DcLevel(const Analyzer::Circuit& circuit, const Analyzer::DcResult& result, uint64_t componentId, uint8_t pinIndex)
{
   const uint32_t net = PinNet(circuit, componentId, pinIndex);
   if (net < result.netLevel.size())
   {
      return result.netLevel[net];
   }
   return Analyzer::LogicLevel::Conflict;
}

double FirstLogicTime(const Analyzer::TransientResult& result, uint32_t net, Analyzer::LogicLevel level)
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

uint64_t FindKind(const Analyzer::Circuit& circuit, Analyzer::ComponentKind kind, uint32_t occurrence)
{
   uint32_t seen = 0;
   for (const Analyzer::Component& component : circuit.Components())
   {
      if (component.kind != kind)
      {
         continue;
      }
      if (seen == occurrence)
      {
         return component.id;
      }
      seen += 1;
   }
   return 0;
}

void TestMoveAndRotate(void)
{
   Analyzer::Circuit circuit;
   Analyzer::Component resistor = Analyzer::CreateComponent(Analyzer::ComponentKind::Resistor, Analyzer::GridPoint{0, 0});
   uint64_t resistorId = 0;
   Expect(Analyzer::IsOk(circuit.AddComponent(resistor, &resistorId)), "place resistor");
   Analyzer::Wire wire = {};
   wire.points.push_back(Analyzer::GridPoint{2, 0});
   wire.points.push_back(Analyzer::GridPoint{6, 0});
   Expect(Analyzer::IsOk(circuit.AddWire(wire, nullptr)), "place wire");
   Expect(Analyzer::IsOk(circuit.MoveComponent(resistorId, Analyzer::GridPoint{0, 2})), "move resistor");
   Expect(circuit.Wires().size() == 1 && IsOrthogonal(circuit.Wires().front()), "moved wire stays orthogonal");
   Expect(circuit.Wires().front().points.front() == Analyzer::GridPoint{2, 2}, "wire followed the pin");

   Analyzer::Circuit turned;
   Analyzer::Component straight = Analyzer::CreateComponent(Analyzer::ComponentKind::Resistor, Analyzer::GridPoint{0, 0});
   uint64_t straightId = 0;
   Expect(Analyzer::IsOk(turned.AddComponent(straight, &straightId)), "place straight resistor");
   Analyzer::Wire lead = {};
   lead.points.push_back(Analyzer::GridPoint{2, 0});
   lead.points.push_back(Analyzer::GridPoint{6, 0});
   Expect(Analyzer::IsOk(turned.AddWire(lead, nullptr)), "place lead");
   Expect(Analyzer::IsOk(turned.RotateComponent(straightId)), "rotate resistor");
   Expect(turned.FindComponent(straightId) != nullptr && turned.FindComponent(straightId)->rotationDegrees == 90, "rotation is 90 degrees");
   Expect(turned.Wires().size() == 1 && IsOrthogonal(turned.Wires().front()), "rotated wire stays orthogonal");
   Expect(turned.Wires().front().points.front() == Analyzer::GridPoint{0, 2}, "wire followed the rotated pin");
}

void CheckDivider(const Analyzer::Circuit& circuit, const Analyzer::DcResult& result)
{
   const uint64_t top = FindKind(circuit, Analyzer::ComponentKind::Resistor, 0);
   ExpectNear(PinVoltage(circuit, result, top, 1), 5.0, 1.0e-4, "divider midpoint");
}

void CheckRc(const Analyzer::Circuit& circuit, const Analyzer::AcResult& result)
{
   Expect(result.points.empty() == false, "rc sweep points");
   const uint64_t capacitor = FindKind(circuit, Analyzer::ComponentKind::Capacitor, 0);
   const uint32_t net = PinNet(circuit, capacitor, 0);
   double low = 0.0;
   double high = 0.0;
   bool haveLow = false;
   for (const Analyzer::AcPoint& point : result.points)
   {
      if (net >= point.netVoltageReal.size())
      {
         continue;
      }
      const double real = point.netVoltageReal[net];
      const double imag = point.netVoltageImag[net];
      const double magnitude = std::sqrt((real * real) + (imag * imag));
      if (haveLow == false)
      {
         low = magnitude;
         haveLow = true;
      }
      high = magnitude;
   }
   Expect(low > 0.9, "rc passes low frequencies");
   Expect(high < 0.2, "rc attenuates high frequencies");
}

void CheckRlc(const Analyzer::TransientResult& result, const Analyzer::Circuit& circuit)
{
   const uint64_t capacitor = FindKind(circuit, Analyzer::ComponentKind::Capacitor, 0);
   const uint32_t net = PinNet(circuit, capacitor, 0);
   double peak = 0.0;
   for (const Analyzer::TransientSample& sample : result.samples)
   {
      if (net < sample.netVoltage.size())
      {
         peak = std::max(peak, sample.netVoltage[net]);
      }
   }
   Expect(peak > 1.2, "rlc step overshoots");
}

void CheckRectifier(const Analyzer::TransientResult& result, const Analyzer::Circuit& circuit)
{
   const uint64_t resistor = FindKind(circuit, Analyzer::ComponentKind::Resistor, 0);
   const uint32_t net = PinNet(circuit, resistor, 0);
   double peak = 0.0;
   for (const Analyzer::TransientSample& sample : result.samples)
   {
      if (net < sample.netVoltage.size())
      {
         peak = std::max(peak, sample.netVoltage[net]);
      }
   }
   Expect(peak > 3.0, "rectifier conducts");
}

void CheckNpn(const Analyzer::Circuit& circuit, const Analyzer::DcResult& result)
{
   const uint64_t transistor = FindKind(circuit, Analyzer::ComponentKind::Npn, 0);
   const double collector = PinVoltage(circuit, result, transistor, 0);
   Expect(collector > 2.0 && collector < 4.8, "npn collector sits between the rails");
}

void CheckNmos(const Analyzer::DcResult& result, const Analyzer::Circuit& circuit)
{
   const uint64_t drain = FindKind(circuit, Analyzer::ComponentKind::VoltageSource, 1);
   ExpectNear(BranchCurrent(result, drain), 0.004, 1.0e-4, "nmos drain current");
}

void CheckNand(const Analyzer::Circuit& circuit, const Analyzer::DcResult& result)
{
   const uint64_t gate = FindKind(circuit, Analyzer::ComponentKind::NandGate, 0);
   Expect(DcLevel(circuit, result, gate, 2) == Analyzer::LogicLevel::High, "nand latch sets");
}

void CheckCounter(const Analyzer::TransientResult& result, const Analyzer::Circuit& circuit)
{
   const uint64_t counter = FindKind(circuit, Analyzer::ComponentKind::Counter, 0);
   const uint32_t bit = PinNet(circuit, counter, 3);
   const double rise = FirstLogicTime(result, bit, Analyzer::LogicLevel::High);
   Expect(rise > 0.0 && rise < 0.002, "counter bit rises");
}

void CheckLed(const Analyzer::DcResult& result, const Analyzer::Circuit& circuit)
{
   const uint64_t source = FindKind(circuit, Analyzer::ComponentKind::VoltageSource, 0);
   Expect(std::abs(BranchCurrent(result, source)) > 0.005, "led current");
}

void SaveExample(Analyzer::ExampleCircuit example, const std::string& text)
{
   const std::filesystem::path directory(ECA_EXAMPLES_DIR);
   std::filesystem::create_directories(directory);
   const std::filesystem::path file = directory / std::string(Analyzer::ExampleFileName(example));
   std::ofstream output(file, std::ios::binary);
   Expect(output.good(), "open example file");
   output << text;
   Expect(output.good(), "write example file");
}

void TestExamples(void)
{
   for (int8_t code = 0; code < static_cast<int8_t>(Analyzer::ExampleCircuit::Count); code += 1)
   {
      const auto example = static_cast<Analyzer::ExampleCircuit>(code);
      Analyzer::Circuit built;
      Expect(Analyzer::IsOk(Analyzer::BuildExample(example, &built)), Analyzer::ExampleTitle(example));
      std::string text;
      Expect(Analyzer::IsOk(Analyzer::WriteCircuitText(built, &text)), "write example text");
      SaveExample(example, text);
      Analyzer::Circuit circuit;
      Expect(Analyzer::IsOk(Analyzer::ReadCircuitText(text, &circuit)), "read example text");
      Expect(circuit.Name() == built.Name(), "example name");
      if (circuit.Settings().kind == Analyzer::AnalysisKind::AcSweep)
      {
         const Analyzer::AcResult result = Analyzer::SolveAc(circuit, nullptr);
         Expect(Analyzer::IsOk(result.code), result.message);
         if (example == Analyzer::ExampleCircuit::RcLowpass)
         {
            CheckRc(circuit, result);
         }
      }
      else if (circuit.Settings().kind == Analyzer::AnalysisKind::Transient)
      {
         const Analyzer::TransientResult result = Analyzer::SolveTransient(circuit, nullptr);
         Expect(Analyzer::IsOk(result.code) || result.code == Analyzer::Result::Warning, result.message);
         if (example == Analyzer::ExampleCircuit::RlcStep)
         {
            CheckRlc(result, circuit);
         }
         else if (example == Analyzer::ExampleCircuit::Rectifier)
         {
            CheckRectifier(result, circuit);
         }
         else if (example == Analyzer::ExampleCircuit::BinaryCounter)
         {
            CheckCounter(result, circuit);
         }
      }
      else
      {
         const Analyzer::DcResult result = Analyzer::SolveDc(circuit, nullptr);
         Expect(Analyzer::IsOk(result.code) || result.code == Analyzer::Result::Warning, result.message);
         if (example == Analyzer::ExampleCircuit::VoltageDivider)
         {
            CheckDivider(circuit, result);
         }
         else if (example == Analyzer::ExampleCircuit::NpnBias)
         {
            CheckNpn(circuit, result);
         }
         else if (example == Analyzer::ExampleCircuit::NmosSwitch)
         {
            CheckNmos(result, circuit);
         }
         else if (example == Analyzer::ExampleCircuit::NandSrLatch)
         {
            CheckNand(circuit, result);
         }
         else if (example == Analyzer::ExampleCircuit::LedAndSwitch)
         {
            CheckLed(result, circuit);
         }
      }
   }
}

} // namespace

int RunExampleTests(void)
{
   gFailures = 0;
   TestMoveAndRotate();
   TestExamples();
   return gFailures;
}
