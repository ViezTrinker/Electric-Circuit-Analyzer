/*!
 * \file logic_bridge.cpp
 * \brief Couples settled logic levels to analog Thevenin drives.
 */

#include "logic_bridge.h"

#include <array>

#include "analog_solver.h"
#include "digital/logic_kernel.h"

namespace Analyzer
{

namespace
{

constexpr uint32_t MaxMixedPasses = 8;
constexpr double MinimumDriveResistanceOhm = 1.0e-3;

struct PinIndexList
{
   uint8_t count;
   std::array<uint8_t, MaxPinCount> pins;
};

PinIndexList EmptyPins(void)
{
   PinIndexList pins = {};
   pins.count = 0;
   pins.pins.fill(0);
   return pins;
}

void AddPin(PinIndexList* pPins, uint8_t pinIndex)
{
   if (pPins == nullptr || pPins->count >= MaxPinCount)
   {
      return;
   }
   pPins->pins[pPins->count] = pinIndex;
   pPins->count += 1;
}

bool IsLogicDevice(ComponentKind kind)
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
      case ComponentKind::Adc:
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

bool IsWideGate(ComponentKind kind)
{
   return kind == ComponentKind::AndGate
      || kind == ComponentKind::OrGate
      || kind == ComponentKind::NandGate
      || kind == ComponentKind::NorGate;
}

const LogicProperties* LogicOf(const ComponentProperties& properties)
{
   if (const LogicProperties* pLogic = std::get_if<LogicProperties>(&properties))
   {
      return pLogic;
   }
   if (const SequentialProperties* pSequential = std::get_if<SequentialProperties>(&properties))
   {
      return &pSequential->logic;
   }
   if (const ClockProperties* pClock = std::get_if<ClockProperties>(&properties))
   {
      return &pClock->logic;
   }
   if (const MuxProperties* pMux = std::get_if<MuxProperties>(&properties))
   {
      return &pMux->logic;
   }
   if (const DecoderProperties* pDecoder = std::get_if<DecoderProperties>(&properties))
   {
      return &pDecoder->logic;
   }
   if (const CounterProperties* pCounter = std::get_if<CounterProperties>(&properties))
   {
      return &pCounter->logic;
   }
   if (const ShiftRegisterProperties* pShift = std::get_if<ShiftRegisterProperties>(&properties))
   {
      return &pShift->logic;
   }
   if (const PullProperties* pPull = std::get_if<PullProperties>(&properties))
   {
      return &pPull->logic;
   }
   if (const AdcProperties* pAdc = std::get_if<AdcProperties>(&properties))
   {
      return &pAdc->logic;
   }
   if (const DacProperties* pDac = std::get_if<DacProperties>(&properties))
   {
      return &pDac->logic;
   }
   return nullptr;
}

PinIndexList InputPins(const NetlistDevice& device)
{
   PinIndexList pins = EmptyPins();
   if (IsWideGate(device.kind) || device.kind == ComponentKind::Multiplexer)
   {
      if (device.pinCount >= 2)
      {
         for (uint8_t pin = 0; pin + 1 < device.pinCount; pin += 1)
         {
            AddPin(&pins, pin);
         }
      }
      return pins;
   }
   switch (device.kind)
   {
      case ComponentKind::Buffer:
      case ComponentKind::Inverter:
      case ComponentKind::SchmittInverter:
      {
         if (device.pinCount > 0)
         {
            AddPin(&pins, 0);
         }
         break;
      }
      case ComponentKind::XorGate:
      case ComponentKind::XnorGate:
      case ComponentKind::SrLatch:
      case ComponentKind::DLatch:
      {
         AddPin(&pins, 0);
         AddPin(&pins, 1);
         break;
      }
      case ComponentKind::TriStateBuffer:
      case ComponentKind::FullAdder:
      case ComponentKind::Decoder:
      case ComponentKind::Counter:
      case ComponentKind::ShiftRegister:
      {
         AddPin(&pins, 0);
         AddPin(&pins, 1);
         AddPin(&pins, 2);
         break;
      }
      case ComponentKind::DFlipFlop:
      case ComponentKind::TFlipFlop:
      {
         AddPin(&pins, 0);
         AddPin(&pins, 1);
         AddPin(&pins, 4);
         AddPin(&pins, 5);
         break;
      }
      case ComponentKind::JkFlipFlop:
      {
         AddPin(&pins, 0);
         AddPin(&pins, 1);
         AddPin(&pins, 2);
         AddPin(&pins, 5);
         AddPin(&pins, 6);
         break;
      }
      case ComponentKind::BcdToSevenSegment:
      {
         AddPin(&pins, 0);
         AddPin(&pins, 1);
         AddPin(&pins, 2);
         AddPin(&pins, 3);
         break;
      }
      case ComponentKind::Adc:
      {
         AddPin(&pins, 0);
         AddPin(&pins, 1);
         AddPin(&pins, 2);
         break;
      }
      default:
      {
         break;
      }
   }
   return pins;
}

PinIndexList OutputPins(const NetlistDevice& device)
{
   PinIndexList pins = EmptyPins();
   if (IsWideGate(device.kind) || device.kind == ComponentKind::Multiplexer)
   {
      if (device.pinCount >= 2)
      {
         AddPin(&pins, static_cast<uint8_t>(device.pinCount - 1));
      }
      return pins;
   }
   switch (device.kind)
   {
      case ComponentKind::Buffer:
      case ComponentKind::Inverter:
      case ComponentKind::SchmittInverter:
      case ComponentKind::XorGate:
      case ComponentKind::XnorGate:
      {
         if (device.pinCount > 1)
         {
            AddPin(&pins, static_cast<uint8_t>(device.pinCount - 1));
         }
         break;
      }
      case ComponentKind::Clock:
      {
         AddPin(&pins, 0);
         break;
      }
      case ComponentKind::TriStateBuffer:
      {
         AddPin(&pins, 2);
         break;
      }
      case ComponentKind::SrLatch:
      case ComponentKind::DLatch:
      {
         AddPin(&pins, 2);
         AddPin(&pins, 3);
         break;
      }
      case ComponentKind::DFlipFlop:
      case ComponentKind::TFlipFlop:
      {
         AddPin(&pins, 2);
         AddPin(&pins, 3);
         break;
      }
      case ComponentKind::JkFlipFlop:
      {
         AddPin(&pins, 3);
         AddPin(&pins, 4);
         break;
      }
      case ComponentKind::Decoder:
      case ComponentKind::ShiftRegister:
      case ComponentKind::Counter:
      {
         for (uint8_t pin = 3; pin < device.pinCount; pin += 1)
         {
            AddPin(&pins, pin);
         }
         break;
      }
      case ComponentKind::BcdToSevenSegment:
      {
         for (uint8_t pin = 4; pin < device.pinCount; pin += 1)
         {
            AddPin(&pins, pin);
         }
         break;
      }
      case ComponentKind::FullAdder:
      {
         AddPin(&pins, 3);
         AddPin(&pins, 4);
         break;
      }
      case ComponentKind::Adc:
      {
         for (uint8_t pin = 3; pin < device.pinCount; pin += 1)
         {
            AddPin(&pins, pin);
         }
         break;
      }
      default:
      {
         break;
      }
   }
   return pins;
}

HoldMask AnalogTouch(const Netlist& netlist)
{
   HoldMask touch(netlist.netCount, 0);
   for (const NetlistDevice& device : netlist.devices)
   {
      if (KindIsAnalog(device.kind) == false)
      {
         continue;
      }
      for (uint8_t pin = 0; pin < device.pinCount; pin += 1)
      {
         uint32_t net = device.nets[pin];
         if (net < touch.size())
         {
            touch[net] = 1;
         }
      }
   }
   return touch;
}

HoldMask HoldFromTouch(const Netlist& netlist, const HoldMask& touch)
{
   HoldMask output(netlist.netCount, 0);
   for (const NetlistDevice& device : netlist.devices)
   {
      if (IsLogicDevice(device.kind) == false)
      {
         continue;
      }
      PinIndexList pins = OutputPins(device);
      for (uint8_t index = 0; index < pins.count; index += 1)
      {
         uint8_t pin = pins.pins[index];
         if (pin >= device.pinCount)
         {
            continue;
         }
         uint32_t net = device.nets[pin];
         if (net < output.size())
         {
            output[net] = 1;
         }
      }
   }
   HoldMask hold(netlist.netCount, 0);
   for (uint32_t net = 0; net < netlist.netCount; net += 1)
   {
      if (net < touch.size() && touch[net] != 0 && (net >= output.size() || output[net] == 0))
      {
         hold[net] = 1;
      }
   }
   return hold;
}

LogicLevel SenseVoltage(double voltage, double lowThresholdVolt, double highThresholdVolt, LogicLevel previous)
{
   if (voltage >= highThresholdVolt)
   {
      return LogicLevel::High;
   }
   if (voltage <= lowThresholdVolt)
   {
      return LogicLevel::Low;
   }
   if (previous == LogicLevel::High || previous == LogicLevel::Low)
   {
      return previous;
   }
   return LogicLevel::Conflict;
}

LogicLevel SenseNet(const Netlist& netlist, uint32_t net, double voltage, LogicLevel previous)
{
   bool sawInput = false;
   LogicLevel agreed = previous;
   for (const NetlistDevice& device : netlist.devices)
   {
      if (IsLogicDevice(device.kind) == false)
      {
         continue;
      }
      const LogicProperties* pLogic = LogicOf(device.properties);
      if (pLogic == nullptr)
      {
         continue;
      }
      PinIndexList inputs = InputPins(device);
      for (uint8_t index = 0; index < inputs.count; index += 1)
      {
         uint8_t pin = inputs.pins[index];
         if (pin >= device.pinCount || device.nets[pin] != net)
         {
            continue;
         }
         LogicLevel sensed = SenseVoltage(voltage, pLogic->vilVolt, pLogic->vihVolt, previous);
         if (sawInput == false)
         {
            agreed = sensed;
            sawInput = true;
         }
         else if (sensed != agreed)
         {
            return LogicLevel::Conflict;
         }
      }
   }
   return agreed;
}

void SenseHeldLevels(const Netlist& netlist, const HoldMask& holdMask, const VoltageList& voltage, LogicLevelList* pLevels)
{
   if (pLevels == nullptr)
   {
      return;
   }
   if (pLevels->size() < netlist.netCount)
   {
      pLevels->assign(netlist.netCount, LogicLevel::HighZ);
   }
   for (uint32_t net = 0; net < netlist.netCount; net += 1)
   {
      if (net >= holdMask.size() || holdMask[net] == 0)
      {
         continue;
      }
      double nodeVoltage = net < voltage.size() ? voltage[net] : 0.0;
      (*pLevels)[net] = SenseNet(netlist, net, nodeVoltage, (*pLevels)[net]);
   }
}

bool SameLevels(const LogicLevelList& left, const LogicLevelList& right)
{
   return left == right;
}

bool SameDrives(const LogicDriveList& left, const LogicDriveList& right)
{
   if (left.size() != right.size())
   {
      return false;
   }
   for (size_t index = 0; index < left.size(); index += 1)
   {
      if (left[index].netIndex != right[index].netIndex)
      {
         return false;
      }
      if (left[index].voltageVolt != right[index].voltageVolt)
      {
         return false;
      }
      if (left[index].resistanceOhm != right[index].resistanceOhm)
      {
         return false;
      }
   }
   return true;
}

Result AppendDrives(const Netlist& netlist, const HoldMask& touch, const HoldMask& hold, const LogicLevelList& levels, LogicDriveList* pDrives)
{
   if (pDrives == nullptr)
   {
      return Result::InvalidParameter;
   }
   pDrives->clear();
   HoldMask stamped(netlist.netCount, 0);
   for (const NetlistDevice& device : netlist.devices)
   {
      if (IsLogicDevice(device.kind) == false)
      {
         continue;
      }
      const LogicProperties* pLogic = LogicOf(device.properties);
      if (pLogic == nullptr)
      {
         continue;
      }
      PinIndexList outputs = OutputPins(device);
      for (uint8_t index = 0; index < outputs.count; index += 1)
      {
         uint8_t pin = outputs.pins[index];
         if (pin >= device.pinCount)
         {
            continue;
         }
         uint32_t net = device.nets[pin];
         if (net == GroundNetIndex || net >= netlist.netCount)
         {
            continue;
         }
         if (net >= touch.size() || touch[net] == 0 || net >= hold.size() || hold[net] != 0)
         {
            continue;
         }
         if (net < stamped.size() && stamped[net] != 0)
         {
            continue;
         }
         if (net >= levels.size())
         {
            continue;
         }
         LogicLevel level = levels[net];
         if (level != LogicLevel::High && level != LogicLevel::Low)
         {
            continue;
         }
         LogicDrive drive = {};
         drive.netIndex = net;
         drive.voltageVolt = level == LogicLevel::High ? pLogic->vohVolt : pLogic->volVolt;
         drive.resistanceOhm = pLogic->outputResistanceOhm > 0.0 ? pLogic->outputResistanceOhm : MinimumDriveResistanceOhm;
         pDrives->push_back(drive);
         if (net < stamped.size())
         {
            stamped[net] = 1;
         }
      }
   }
   return Result::Ok;
}

} // namespace

DcResult SolveOperatingPoint(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel)
{
   HoldMask touch = AnalogTouch(netlist);
   HoldMask hold = HoldFromTouch(netlist, touch);
   LogicDriveList drives;
   LogicLevelList sensed(netlist.netCount, LogicLevel::HighZ);
   DcResult latest = {};
   bool settled = false;
   for (uint32_t pass = 0; pass < MaxMixedPasses; pass += 1)
   {
      latest = SolveDcAnalog(netlist, settings, pCancel, &drives);
      if (IsErr(latest.code))
      {
         return latest;
      }
      LogicLevelList next = sensed;
      SenseHeldLevels(netlist, hold, latest.netVoltage, &next);
      LogicLevelList levels;
      Result read = ReadSettledLevels(netlist, next, hold, &levels, &latest.netVoltage);
      if (IsErr(read))
      {
         latest.code = read;
         latest.message = Describe(read);
         return latest;
      }
      LogicDriveList nextDrives;
      Result collected = AppendDrives(netlist, touch, hold, levels, &nextDrives);
      if (IsErr(collected))
      {
         latest.code = collected;
         latest.message = Describe(collected);
         return latest;
      }
      bool levelsMatch = SameLevels(next, sensed);
      bool drivesMatch = SameDrives(drives, nextDrives);
      sensed = next;
      drives = nextDrives;
      if (levelsMatch && drivesMatch)
      {
         settled = true;
         break;
      }
   }
   Result digital = SolveDigitalDcHeld(netlist, sensed, hold, &latest, &latest.netVoltage);
   if (IsErr(digital))
   {
      return latest;
   }
   if (settled == false)
   {
      latest.code = Result::Oscillation;
      latest.message = Describe(Result::Oscillation);
   }
   return latest;
}

Result CollectOperatingDrives(const Netlist& netlist, const LogicLevelList& levels, LogicDriveList* pDrives)
{
   HoldMask touch = AnalogTouch(netlist);
   HoldMask hold = HoldFromTouch(netlist, touch);
   return AppendDrives(netlist, touch, hold, levels, pDrives);
}

} // namespace Analyzer
