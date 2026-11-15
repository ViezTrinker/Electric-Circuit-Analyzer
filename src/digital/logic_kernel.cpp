/*!
 * \file logic_kernel.cpp
 * \brief Resolves logic nets and schedules gate and flip-flop events.
 */

#include "logic_kernel.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <queue>
#include <string>
#include <vector>

namespace Analyzer
{

namespace
{

constexpr uint32_t MaxDeltaCycles = 1000;
constexpr uint32_t MaxClockEdges = 1000000;
constexpr int64_t TimeLimitPs = 9000000000000000000LL;
constexpr double PicosecondsPerSecond = 1.0e12;
constexpr uint8_t NibbleMask = 0x0F;
constexpr uint8_t SegmentCount = 7;
constexpr uint8_t DigitCount = 16;

using LevelArray = std::array<LogicLevel, MaxPinCount>;
using DrivenArray = std::array<bool, MaxPinCount>;
using GenerationArray = std::array<uint32_t, MaxPinCount>;
using GlyphTable = std::array<uint8_t, DigitCount>;

constexpr GlyphTable SegmentGlyph = {
   0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
   0x7F, 0x6F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

enum class EvalMode : int8_t
{
   Settle = 0,
   Event
};

enum class PendingMode : int8_t
{
   KeepEarlier = 0,
   ReplaceEarlier
};

enum class AssertState : int8_t
{
   Inactive = 0,
   Active,
   Unknown
};

struct PinSet
{
   uint8_t count;
   std::array<uint8_t, MaxPinCount> pins;
};

struct OutputPlan
{
   DrivenArray driven;
   LevelArray level;
};

struct DeviceMemory
{
   LogicLevel stored;
   LogicLevel previousClock;
   LogicLevel previousData;
   uint8_t bits;
   int64_t dataStableSincePs;
   int64_t lastEdgePs;
   bool timingViolation;
   bool timingReported;
   LevelArray published;
   LevelArray scheduled;
   DrivenArray hasSchedule;
   GenerationArray generation;
   GenerationArray validAfter;
};

struct LogicEvent
{
   int64_t timePs;
   uint32_t delta;
   uint32_t generation;
   uint32_t deviceIndex;
   uint8_t pinIndex;
   LogicLevel level;
};

struct EventLater
{
   bool operator()(const LogicEvent& left, const LogicEvent& right) const
   {
      if (left.timePs != right.timePs)
      {
         return left.timePs > right.timePs;
      }
      if (left.delta != right.delta)
      {
         return left.delta > right.delta;
      }
      return left.generation > right.generation;
   }
};

using EventQueue = std::priority_queue<LogicEvent, std::vector<LogicEvent>, EventLater>;
using Fanout = std::vector<std::vector<uint32_t>>;

int64_t SecondsToPicoseconds(double seconds)
{
   if (std::isfinite(seconds) == false || seconds <= 0.0)
   {
      return 0;
   }
   double scaled = seconds * PicosecondsPerSecond;
   if (scaled >= static_cast<double>(TimeLimitPs))
   {
      return TimeLimitPs;
   }
   return static_cast<int64_t>(std::llround(scaled));
}

bool IsBinary(LogicLevel level)
{
   return level == LogicLevel::High || level == LogicLevel::Low;
}

LogicLevel Complement(LogicLevel level)
{
   if (level == LogicLevel::High)
   {
      return LogicLevel::Low;
   }
   if (level == LogicLevel::Low)
   {
      return LogicLevel::High;
   }
   return LogicLevel::Conflict;
}

LogicLevel AfterStage(LogicLevel level, OutputStage stage)
{
   if (level == LogicLevel::High && stage == OutputStage::OpenDrain)
   {
      return LogicLevel::HighZ;
   }
   return level;
}

AssertState Sense(LogicLevel level, Polarity polarity)
{
   if (IsBinary(level) == false)
   {
      return AssertState::Unknown;
   }
   bool activeHigh = polarity == Polarity::ActiveHigh;
   bool levelHigh = level == LogicLevel::High;
   if (activeHigh == levelHigh)
   {
      return AssertState::Active;
   }
   return AssertState::Inactive;
}

bool BitIsSet(uint8_t value, uint8_t bitIndex)
{
   uint8_t mask = static_cast<uint8_t>(1u << bitIndex);
   return (value & mask) != 0;
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

bool IsLogicKind(ComponentKind kind)
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

PinSet MakePinSet(void)
{
   PinSet set = {};
   set.count = 0;
   set.pins.fill(0);
   return set;
}

void AddPin(PinSet* pSet, uint8_t pinIndex)
{
   if (pSet == nullptr || pSet->count >= MaxPinCount)
   {
      return;
   }
   pSet->pins[pSet->count] = pinIndex;
   pSet->count += 1;
}

PinSet InputPins(const NetlistDevice& device)
{
   PinSet set = MakePinSet();
   if (IsWideGate(device.kind) || device.kind == ComponentKind::Multiplexer)
   {
      if (device.pinCount >= 2)
      {
         for (uint8_t pin = 0; pin + 1 < device.pinCount; pin += 1)
         {
            AddPin(&set, pin);
         }
      }
      return set;
   }
   switch (device.kind)
   {
      case ComponentKind::Buffer:
      case ComponentKind::Inverter:
      case ComponentKind::SchmittInverter:
      case ComponentKind::Clock:
      {
         if (device.kind != ComponentKind::Clock && device.pinCount > 0)
         {
            AddPin(&set, 0);
         }
         break;
      }
      case ComponentKind::XorGate:
      case ComponentKind::XnorGate:
      case ComponentKind::SrLatch:
      case ComponentKind::DLatch:
      {
         AddPin(&set, 0);
         AddPin(&set, 1);
         break;
      }
      case ComponentKind::TriStateBuffer:
      case ComponentKind::FullAdder:
      case ComponentKind::Decoder:
      case ComponentKind::Counter:
      case ComponentKind::ShiftRegister:
      {
         AddPin(&set, 0);
         AddPin(&set, 1);
         AddPin(&set, 2);
         break;
      }
      case ComponentKind::DFlipFlop:
      case ComponentKind::TFlipFlop:
      {
         AddPin(&set, 0);
         AddPin(&set, 1);
         AddPin(&set, 4);
         AddPin(&set, 5);
         break;
      }
      case ComponentKind::JkFlipFlop:
      {
         AddPin(&set, 0);
         AddPin(&set, 1);
         AddPin(&set, 2);
         AddPin(&set, 5);
         AddPin(&set, 6);
         break;
      }
      case ComponentKind::BcdToSevenSegment:
      {
         AddPin(&set, 0);
         AddPin(&set, 1);
         AddPin(&set, 2);
         AddPin(&set, 3);
         break;
      }
      case ComponentKind::Adc:
      {
         AddPin(&set, 0);
         AddPin(&set, 1);
         AddPin(&set, 2);
         break;
      }
      default:
      {
         break;
      }
   }
   return set;
}

PinSet OutputPins(const NetlistDevice& device)
{
   PinSet set = MakePinSet();
   if (IsWideGate(device.kind) || device.kind == ComponentKind::Multiplexer)
   {
      if (device.pinCount >= 2)
      {
         AddPin(&set, static_cast<uint8_t>(device.pinCount - 1));
      }
      return set;
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
            AddPin(&set, static_cast<uint8_t>(device.pinCount - 1));
         }
         break;
      }
      case ComponentKind::Clock:
      {
         AddPin(&set, 0);
         break;
      }
      case ComponentKind::TriStateBuffer:
      {
         AddPin(&set, 2);
         break;
      }
      case ComponentKind::SrLatch:
      case ComponentKind::DLatch:
      {
         AddPin(&set, 2);
         AddPin(&set, 3);
         break;
      }
      case ComponentKind::DFlipFlop:
      case ComponentKind::TFlipFlop:
      {
         AddPin(&set, 2);
         AddPin(&set, 3);
         break;
      }
      case ComponentKind::JkFlipFlop:
      {
         AddPin(&set, 3);
         AddPin(&set, 4);
         break;
      }
      case ComponentKind::Decoder:
      case ComponentKind::ShiftRegister:
      {
         for (uint8_t pin = 3; pin < device.pinCount; pin += 1)
         {
            AddPin(&set, pin);
         }
         break;
      }
      case ComponentKind::Counter:
      {
         for (uint8_t pin = 3; pin < device.pinCount; pin += 1)
         {
            AddPin(&set, pin);
         }
         break;
      }
      case ComponentKind::BcdToSevenSegment:
      {
         for (uint8_t pin = 4; pin < device.pinCount; pin += 1)
         {
            AddPin(&set, pin);
         }
         break;
      }
      case ComponentKind::FullAdder:
      {
         AddPin(&set, 3);
         AddPin(&set, 4);
         break;
      }
      case ComponentKind::Adc:
      {
         for (uint8_t pin = 3; pin < device.pinCount; pin += 1)
         {
            AddPin(&set, pin);
         }
         break;
      }
      default:
      {
         break;
      }
   }
   return set;
}

OutputPlan EmptyPlan(void)
{
   OutputPlan plan = {};
   plan.driven.fill(false);
   plan.level.fill(LogicLevel::HighZ);
   return plan;
}

void Drive(OutputPlan* pPlan, uint8_t pinIndex, LogicLevel level, OutputStage stage)
{
   if (pPlan == nullptr || pinIndex >= MaxPinCount)
   {
      return;
   }
   pPlan->driven[pinIndex] = true;
   pPlan->level[pinIndex] = AfterStage(level, stage);
}

void DrivePair(OutputPlan* pPlan, uint8_t outputPin, uint8_t complementPin, LogicLevel stored, OutputStage stage)
{
   Drive(pPlan, outputPin, stored, stage);
   Drive(pPlan, complementPin, Complement(stored), stage);
}

LogicLevel CombineAnd(const LevelArray& input, uint8_t inputCount)
{
   bool unknown = false;
   for (uint8_t index = 0; index < inputCount; index += 1)
   {
      if (input[index] == LogicLevel::Low)
      {
         return LogicLevel::Low;
      }
      if (IsBinary(input[index]) == false)
      {
         unknown = true;
      }
   }
   if (unknown)
   {
      return LogicLevel::Conflict;
   }
   return LogicLevel::High;
}

LogicLevel CombineOr(const LevelArray& input, uint8_t inputCount)
{
   bool unknown = false;
   for (uint8_t index = 0; index < inputCount; index += 1)
   {
      if (input[index] == LogicLevel::High)
      {
         return LogicLevel::High;
      }
      if (IsBinary(input[index]) == false)
      {
         unknown = true;
      }
   }
   if (unknown)
   {
      return LogicLevel::Conflict;
   }
   return LogicLevel::Low;
}

LogicLevel CombineXor(LogicLevel left, LogicLevel right)
{
   if (IsBinary(left) == false || IsBinary(right) == false)
   {
      return LogicLevel::Conflict;
   }
   if (left == right)
   {
      return LogicLevel::Low;
   }
   return LogicLevel::High;
}

bool ClockIsPeriodic(const ClockProperties& clock)
{
   return clock.frequencyHz > 0.0 && clock.dutyCycle > 0.0 && clock.dutyCycle < 1.0;
}

LogicLevel ClockLevelAt(const ClockProperties& clock, int64_t timePs)
{
   int64_t delayPs = SecondsToPicoseconds(clock.delaySec);
   LogicLevel resting = clock.startLevel == LogicLevel::High ? LogicLevel::High : LogicLevel::Low;
   if (timePs < delayPs || ClockIsPeriodic(clock) == false)
   {
      return resting;
   }
   int64_t periodPs = SecondsToPicoseconds(1.0 / clock.frequencyHz);
   int64_t highPs = SecondsToPicoseconds(clock.dutyCycle / clock.frequencyHz);
   if (periodPs <= 0 || highPs <= 0 || highPs >= periodPs)
   {
      return LogicLevel::Low;
   }
   int64_t elapsed = timePs - delayPs;
   int64_t position = elapsed % periodPs;
   if (position < highPs)
   {
      return LogicLevel::High;
   }
   return LogicLevel::Low;
}

int64_t NextClockEdge(const ClockProperties& clock, int64_t timePs)
{
   if (ClockIsPeriodic(clock) == false)
   {
      return -1;
   }
   int64_t delayPs = SecondsToPicoseconds(clock.delaySec);
   int64_t periodPs = SecondsToPicoseconds(1.0 / clock.frequencyHz);
   int64_t highPs = SecondsToPicoseconds(clock.dutyCycle / clock.frequencyHz);
   if (periodPs <= 0 || highPs <= 0 || highPs >= periodPs)
   {
      return -1;
   }
   if (timePs < delayPs)
   {
      return delayPs;
   }
   int64_t elapsed = timePs - delayPs;
   int64_t position = elapsed % periodPs;
   int64_t cycleStart = timePs - position;
   if (position < highPs)
   {
      int64_t falling = cycleStart + highPs;
      if (falling > timePs)
      {
         return falling;
      }
   }
   int64_t rising = cycleStart + periodPs;
   if (rising <= timePs)
   {
      rising += periodPs;
   }
   return rising;
}

struct LogicSim
{
   const Netlist* pNetlist = nullptr;
   std::vector<DeviceMemory> memory;
   std::vector<bool> isLogic;
   LogicLevelList levels;
   Fanout fanout;
   EventQueue events;
   std::vector<std::string> timingNotes;
   Result code;
   int64_t stopPs;
   HoldMask holdNet;
   const VoltageList* pAnalogVoltage = nullptr;

   Result Bind(const Netlist* pBound);
   void Resolve(void);
   LogicLevel RawPin(uint32_t deviceIndex, uint8_t pinIndex) const;
   LogicLevel LogicPin(uint32_t deviceIndex, uint8_t pinIndex) const;
   void ReportTiming(uint32_t deviceIndex);
   bool TakeEdge(uint32_t deviceIndex, uint8_t clockPin, ClockEdge edge, EvalMode mode, int64_t timePs);
   void NoteData(uint32_t deviceIndex, LogicLevel data, EvalMode mode, int64_t timePs);
   OutputPlan Compute(uint32_t deviceIndex, EvalMode mode, int64_t timePs);
   bool ApplyNow(uint32_t deviceIndex, const OutputPlan& plan);
   void EnqueueAt(uint32_t deviceIndex, uint8_t pinIndex, LogicLevel level, int64_t timePs, uint32_t delta, PendingMode pending);
   void SchedulePlan(uint32_t deviceIndex, const OutputPlan& plan, int64_t timePs, uint32_t delta);
   Result Settle(void);
   void ArmMemory(void);
   void ScheduleClocks(void);
   void Publish(DcResult* pResult) const;
   Result Run(CancelToken* pCancel, TransientResult* pResult);
   Result AdvanceTo(int64_t timePs, const LogicLevelList* pHeldLevels, CancelToken* pCancel);
};

Result LogicSim::Bind(const Netlist* pBound)
{
   if (pBound == nullptr)
   {
      return Result::InvalidParameter;
   }
   pNetlist = pBound;
   code = Result::Ok;
   stopPs = 0;
   memory.assign(pNetlist->devices.size(), DeviceMemory{});
   isLogic.assign(pNetlist->devices.size(), false);
   levels.assign(pNetlist->netCount, LogicLevel::HighZ);
   holdNet.assign(pNetlist->netCount, 0);
   pAnalogVoltage = nullptr;
   fanout.assign(pNetlist->netCount, {});
   for (size_t index = 0; index < pNetlist->devices.size(); index += 1)
   {
      const NetlistDevice& device = pNetlist->devices[index];
      DeviceMemory& slot = memory[index];
      slot.stored = LogicLevel::Low;
      slot.previousClock = LogicLevel::Low;
      slot.previousData = LogicLevel::Low;
      slot.bits = 0;
      slot.dataStableSincePs = 0;
      slot.lastEdgePs = -1;
      slot.timingViolation = false;
      slot.timingReported = false;
      slot.published.fill(LogicLevel::HighZ);
      slot.scheduled.fill(LogicLevel::HighZ);
      slot.hasSchedule.fill(false);
      slot.generation.fill(0);
      slot.validAfter.fill(0);
      if (IsLogicKind(device.kind) == false)
      {
         continue;
      }
      isLogic[index] = true;
      if (const SequentialProperties* pSequential = std::get_if<SequentialProperties>(&device.properties))
      {
         if (pSequential->initialState == LogicLevel::High || pSequential->initialState == LogicLevel::Conflict)
         {
            slot.stored = pSequential->initialState;
         }
      }
      if (const CounterProperties* pCounter = std::get_if<CounterProperties>(&device.properties))
      {
         slot.bits = static_cast<uint8_t>(pCounter->initialValue & NibbleMask);
      }
      if (const ShiftRegisterProperties* pShift = std::get_if<ShiftRegisterProperties>(&device.properties))
      {
         slot.bits = static_cast<uint8_t>(pShift->initialValue & NibbleMask);
      }
      PinSet inputs = InputPins(device);
      for (uint8_t input = 0; input < inputs.count; input += 1)
      {
         uint8_t pin = inputs.pins[input];
         if (pin >= device.pinCount)
         {
            continue;
         }
         uint32_t net = device.nets[pin];
         if (net < fanout.size())
         {
            fanout[net].push_back(static_cast<uint32_t>(index));
         }
      }
   }
   return Result::Ok;
}

void LogicSim::Resolve(void)
{
   struct NetDrive
   {
      bool strongHigh;
      bool strongLow;
      bool pullHigh;
      bool pullLow;
   };
   std::vector<NetDrive> drive(levels.size(), NetDrive{false, false, false, false});
   for (size_t index = 0; index < pNetlist->devices.size(); index += 1)
   {
      const NetlistDevice& device = pNetlist->devices[index];
      if (isLogic[index] == false)
      {
         continue;
      }
      if (device.kind == ComponentKind::PullUp || device.kind == ComponentKind::PullDown)
      {
         if (device.pinCount == 0)
         {
            continue;
         }
         uint32_t net = device.nets[0];
         if (net >= drive.size())
         {
            continue;
         }
         if (device.kind == ComponentKind::PullUp)
         {
            drive[net].pullHigh = true;
         }
         else
         {
            drive[net].pullLow = true;
         }
         continue;
      }
      PinSet outputs = OutputPins(device);
      for (uint8_t output = 0; output < outputs.count; output += 1)
      {
         uint8_t pin = outputs.pins[output];
         if (pin >= device.pinCount)
         {
            continue;
         }
         uint32_t net = device.nets[pin];
         if (net >= drive.size())
         {
            continue;
         }
         LogicLevel level = memory[index].published[pin];
         if (level == LogicLevel::High)
         {
            drive[net].strongHigh = true;
         }
         else if (level == LogicLevel::Low)
         {
            drive[net].strongLow = true;
         }
         else if (level == LogicLevel::Conflict)
         {
            drive[net].strongHigh = true;
            drive[net].strongLow = true;
         }
      }
   }
   for (size_t net = 0; net < levels.size(); net += 1)
   {
      if (net < holdNet.size() && holdNet[net] != 0)
      {
         continue;
      }
      const NetDrive& node = drive[net];
      if (node.strongHigh && node.strongLow)
      {
         levels[net] = LogicLevel::Conflict;
      }
      else if (node.strongHigh)
      {
         levels[net] = LogicLevel::High;
      }
      else if (node.strongLow)
      {
         levels[net] = LogicLevel::Low;
      }
      else if (node.pullHigh && node.pullLow)
      {
         levels[net] = LogicLevel::Conflict;
      }
      else if (node.pullHigh)
      {
         levels[net] = LogicLevel::High;
      }
      else if (node.pullLow)
      {
         levels[net] = LogicLevel::Low;
      }
      else
      {
         levels[net] = LogicLevel::HighZ;
      }
   }
}

LogicLevel LogicSim::RawPin(uint32_t deviceIndex, uint8_t pinIndex) const
{
   const NetlistDevice& device = pNetlist->devices[deviceIndex];
   if (pinIndex >= device.pinCount)
   {
      return LogicLevel::Conflict;
   }
   uint32_t net = device.nets[pinIndex];
   if (net >= levels.size())
   {
      return LogicLevel::Conflict;
   }
   return levels[net];
}

LogicLevel LogicSim::LogicPin(uint32_t deviceIndex, uint8_t pinIndex) const
{
   LogicLevel raw = RawPin(deviceIndex, pinIndex);
   if (IsBinary(raw))
   {
      return raw;
   }
   return LogicLevel::Conflict;
}

void LogicSim::ReportTiming(uint32_t deviceIndex)
{
   DeviceMemory& slot = memory[deviceIndex];
   if (slot.timingViolation == false || slot.timingReported)
   {
      return;
   }
   slot.timingReported = true;
   timingNotes.push_back(pNetlist->devices[deviceIndex].name + " violated setup or hold time.");
}

void LogicSim::NoteData(uint32_t deviceIndex, LogicLevel data, EvalMode mode, int64_t timePs)
{
   DeviceMemory& slot = memory[deviceIndex];
   if (data == slot.previousData)
   {
      return;
   }
   slot.previousData = data;
   if (mode != EvalMode::Event)
   {
      return;
   }
   const NetlistDevice& device = pNetlist->devices[deviceIndex];
   const SequentialProperties* pSequential = std::get_if<SequentialProperties>(&device.properties);
   int64_t holdPs = 0;
   if (pSequential != nullptr)
   {
      holdPs = SecondsToPicoseconds(pSequential->holdTimeSec);
   }
   if (holdPs > 0 && slot.lastEdgePs >= 0 && (timePs - slot.lastEdgePs) < holdPs)
   {
      slot.stored = LogicLevel::Conflict;
      slot.timingViolation = true;
      ReportTiming(deviceIndex);
   }
   slot.dataStableSincePs = timePs;
}

bool LogicSim::TakeEdge(uint32_t deviceIndex, uint8_t clockPin, ClockEdge edge, EvalMode mode, int64_t timePs)
{
   DeviceMemory& slot = memory[deviceIndex];
   LogicLevel current = RawPin(deviceIndex, clockPin);
   bool rising = slot.previousClock == LogicLevel::Low && current == LogicLevel::High;
   bool falling = slot.previousClock == LogicLevel::High && current == LogicLevel::Low;
   slot.previousClock = current;
   if (mode != EvalMode::Event)
   {
      return false;
   }
   bool matched = edge == ClockEdge::Rising ? rising : falling;
   if (matched)
   {
      slot.lastEdgePs = timePs;
   }
   return matched;
}

OutputPlan LogicSim::Compute(uint32_t deviceIndex, EvalMode mode, int64_t timePs)
{
   const NetlistDevice& device = pNetlist->devices[deviceIndex];
   DeviceMemory& slot = memory[deviceIndex];
   OutputPlan plan = EmptyPlan();
   const LogicProperties* pLogic = LogicOf(device.properties);
   OutputStage stage = pLogic == nullptr ? OutputStage::PushPull : pLogic->outputStage;
   if (device.kind == ComponentKind::PullUp || device.kind == ComponentKind::PullDown || device.kind == ComponentKind::LogicProbe)
   {
      return plan;
   }
   if (device.kind == ComponentKind::Clock)
   {
      const ClockProperties* pClock = std::get_if<ClockProperties>(&device.properties);
      LogicLevel level = LogicLevel::Low;
      if (pClock != nullptr)
      {
         level = ClockLevelAt(*pClock, mode == EvalMode::Event ? timePs : 0);
      }
      Drive(&plan, 0, level, stage);
      return plan;
   }
   if (device.kind == ComponentKind::Buffer || device.kind == ComponentKind::Inverter || device.kind == ComponentKind::SchmittInverter
      || device.kind == ComponentKind::XorGate || device.kind == ComponentKind::XnorGate || IsWideGate(device.kind))
   {
      PinSet inputs = InputPins(device);
      LevelArray values = {};
      values.fill(LogicLevel::Conflict);
      for (uint8_t input = 0; input < inputs.count; input += 1)
      {
         values[input] = LogicPin(deviceIndex, inputs.pins[input]);
      }
      LogicLevel combined = LogicLevel::Conflict;
      if (device.kind == ComponentKind::Buffer)
      {
         combined = values[0];
      }
      else if (device.kind == ComponentKind::Inverter || device.kind == ComponentKind::SchmittInverter)
      {
         combined = Complement(values[0]);
      }
      else if (device.kind == ComponentKind::AndGate || device.kind == ComponentKind::NandGate)
      {
         combined = CombineAnd(values, inputs.count);
         if (device.kind == ComponentKind::NandGate)
         {
            combined = Complement(combined);
         }
      }
      else if (device.kind == ComponentKind::OrGate || device.kind == ComponentKind::NorGate)
      {
         combined = CombineOr(values, inputs.count);
         if (device.kind == ComponentKind::NorGate)
         {
            combined = Complement(combined);
         }
      }
      else
      {
         combined = CombineXor(values[0], values[1]);
         if (device.kind == ComponentKind::XnorGate)
         {
            combined = Complement(combined);
         }
      }
      PinSet outputs = OutputPins(device);
      if (outputs.count > 0)
      {
         Drive(&plan, outputs.pins[0], combined, stage);
      }
      return plan;
   }
   if (device.kind == ComponentKind::TriStateBuffer)
   {
      AssertState enable = Sense(LogicPin(deviceIndex, 1), pLogic == nullptr ? Polarity::ActiveHigh : pLogic->enablePolarity);
      if (enable == AssertState::Inactive)
      {
         Drive(&plan, 2, LogicLevel::HighZ, OutputStage::PushPull);
         return plan;
      }
      if (enable == AssertState::Unknown)
      {
         Drive(&plan, 2, LogicLevel::Conflict, stage);
         return plan;
      }
      Drive(&plan, 2, LogicPin(deviceIndex, 0), stage);
      return plan;
   }
   if (device.kind == ComponentKind::SrLatch)
   {
      const SequentialProperties* pSequential = std::get_if<SequentialProperties>(&device.properties);
      Polarity polarity = pSequential == nullptr ? Polarity::ActiveHigh : pSequential->srPolarity;
      AssertState set = Sense(LogicPin(deviceIndex, 0), polarity);
      AssertState reset = Sense(LogicPin(deviceIndex, 1), polarity);
      if (set == AssertState::Unknown || reset == AssertState::Unknown || (set == AssertState::Active && reset == AssertState::Active))
      {
         slot.stored = LogicLevel::Conflict;
      }
      else if (set == AssertState::Active)
      {
         slot.stored = LogicLevel::High;
      }
      else if (reset == AssertState::Active)
      {
         slot.stored = LogicLevel::Low;
      }
      DrivePair(&plan, 2, 3, slot.stored, stage);
      return plan;
   }
   if (device.kind == ComponentKind::DLatch)
   {
      const SequentialProperties* pSequential = std::get_if<SequentialProperties>(&device.properties);
      Polarity polarity = pSequential == nullptr ? Polarity::ActiveHigh : pSequential->enablePolarity;
      AssertState enable = Sense(LogicPin(deviceIndex, 1), polarity);
      if (enable == AssertState::Unknown)
      {
         slot.stored = LogicLevel::Conflict;
      }
      else if (enable == AssertState::Active)
      {
         slot.stored = LogicPin(deviceIndex, 0);
      }
      DrivePair(&plan, 2, 3, slot.stored, stage);
      return plan;
   }
   if (device.kind == ComponentKind::DFlipFlop || device.kind == ComponentKind::TFlipFlop || device.kind == ComponentKind::JkFlipFlop)
   {
      const SequentialProperties* pSequential = std::get_if<SequentialProperties>(&device.properties);
      ClockEdge edge = pSequential == nullptr ? ClockEdge::Rising : pSequential->clockEdge;
      Polarity presetPolarity = pSequential == nullptr ? Polarity::ActiveHigh : pSequential->presetPolarity;
      Polarity clearPolarity = pSequential == nullptr ? Polarity::ActiveHigh : pSequential->clearPolarity;
      uint8_t clockPin = device.kind == ComponentKind::JkFlipFlop ? 2 : 1;
      uint8_t presetPin = device.kind == ComponentKind::JkFlipFlop ? 5 : 4;
      uint8_t clearPin = device.kind == ComponentKind::JkFlipFlop ? 6 : 5;
      uint8_t outputPin = device.kind == ComponentKind::JkFlipFlop ? 3 : 2;
      uint8_t complementPin = device.kind == ComponentKind::JkFlipFlop ? 4 : 3;
      if (device.kind == ComponentKind::DFlipFlop)
      {
         NoteData(deviceIndex, LogicPin(deviceIndex, 0), mode, timePs);
      }
      bool edged = TakeEdge(deviceIndex, clockPin, edge, mode, timePs);
      AssertState preset = Sense(LogicPin(deviceIndex, presetPin), presetPolarity);
      AssertState clear = Sense(LogicPin(deviceIndex, clearPin), clearPolarity);
      if (preset == AssertState::Unknown || clear == AssertState::Unknown || (preset == AssertState::Active && clear == AssertState::Active))
      {
         slot.stored = LogicLevel::Conflict;
      }
      else if (clear == AssertState::Active)
      {
         slot.stored = LogicLevel::Low;
      }
      else if (preset == AssertState::Active)
      {
         slot.stored = LogicLevel::High;
      }
      else if (edged)
      {
         int64_t setupPs = pSequential == nullptr ? 0 : SecondsToPicoseconds(pSequential->setupTimeSec);
         bool setupViolation = setupPs > 0 && (timePs - slot.dataStableSincePs) < setupPs;
         if (device.kind == ComponentKind::DFlipFlop && setupViolation)
         {
            slot.stored = LogicLevel::Conflict;
            slot.timingViolation = true;
            ReportTiming(deviceIndex);
         }
         else if (device.kind == ComponentKind::DFlipFlop)
         {
            slot.stored = LogicPin(deviceIndex, 0);
         }
         else if (device.kind == ComponentKind::TFlipFlop)
         {
            if (LogicPin(deviceIndex, 0) == LogicLevel::High)
            {
               slot.stored = Complement(slot.stored);
            }
            else if (LogicPin(deviceIndex, 0) != LogicLevel::Low)
            {
               slot.stored = LogicLevel::Conflict;
            }
         }
         else
         {
            LogicLevel jay = LogicPin(deviceIndex, 0);
            LogicLevel kay = LogicPin(deviceIndex, 1);
            if (IsBinary(jay) == false || IsBinary(kay) == false)
            {
               slot.stored = LogicLevel::Conflict;
            }
            else if (jay == LogicLevel::High && kay == LogicLevel::High)
            {
               slot.stored = Complement(slot.stored);
            }
            else if (jay == LogicLevel::High)
            {
               slot.stored = LogicLevel::High;
            }
            else if (kay == LogicLevel::High)
            {
               slot.stored = LogicLevel::Low;
            }
         }
      }
      DrivePair(&plan, outputPin, complementPin, slot.stored, stage);
      return plan;
   }
   if (device.kind == ComponentKind::Counter || device.kind == ComponentKind::ShiftRegister)
   {
      ClockEdge edge = ClockEdge::Rising;
      Polarity clearPolarity = Polarity::ActiveHigh;
      if (const CounterProperties* pCounter = std::get_if<CounterProperties>(&device.properties))
      {
         edge = pCounter->clockEdge;
         clearPolarity = pCounter->clearPolarity;
      }
      if (const ShiftRegisterProperties* pShift = std::get_if<ShiftRegisterProperties>(&device.properties))
      {
         edge = pShift->clockEdge;
         clearPolarity = pShift->clearPolarity;
      }
      bool edged = TakeEdge(deviceIndex, 0, edge, mode, timePs);
      AssertState clear = Sense(LogicPin(deviceIndex, 1), clearPolarity);
      if (clear == AssertState::Active)
      {
         slot.bits = 0;
         slot.stored = LogicLevel::Low;
      }
      else if (edged && device.kind == ComponentKind::Counter && LogicPin(deviceIndex, 2) == LogicLevel::High)
      {
         slot.bits = static_cast<uint8_t>((slot.bits + 1u) & NibbleMask);
      }
      else if (edged && device.kind == ComponentKind::ShiftRegister)
      {
         uint8_t serial = LogicPin(deviceIndex, 2) == LogicLevel::High ? 1 : 0;
         if (IsBinary(LogicPin(deviceIndex, 2)) == false)
         {
            slot.bits = 0;
            slot.stored = LogicLevel::Conflict;
         }
         else
         {
            slot.bits = static_cast<uint8_t>(((slot.bits << 1u) | serial) & NibbleMask);
            slot.stored = LogicLevel::Low;
         }
      }
      if (slot.stored == LogicLevel::Conflict && device.kind == ComponentKind::ShiftRegister)
      {
         for (uint8_t bitIndex = 0; bitIndex < 4; bitIndex += 1)
         {
            Drive(&plan, static_cast<uint8_t>(3 + bitIndex), LogicLevel::Conflict, stage);
         }
         return plan;
      }
      for (uint8_t bitIndex = 0; bitIndex < 4; bitIndex += 1)
      {
         LogicLevel bitLevel = BitIsSet(slot.bits, bitIndex) ? LogicLevel::High : LogicLevel::Low;
         Drive(&plan, static_cast<uint8_t>(3 + bitIndex), bitLevel, stage);
      }
      if (device.kind == ComponentKind::Counter && device.pinCount > 7)
      {
         LogicLevel carry = slot.bits == NibbleMask ? LogicLevel::High : LogicLevel::Low;
         Drive(&plan, 7, carry, stage);
      }
      return plan;
   }
   if (device.kind == ComponentKind::Decoder || device.kind == ComponentKind::BcdToSevenSegment)
   {
      const DecoderProperties* pDecoder = std::get_if<DecoderProperties>(&device.properties);
      Polarity outputPolarity = pDecoder == nullptr ? Polarity::ActiveHigh : pDecoder->outputPolarity;
      Polarity enablePolarity = pDecoder == nullptr ? Polarity::ActiveHigh : pDecoder->enablePolarity;
      LogicLevel asserted = outputPolarity == Polarity::ActiveHigh ? LogicLevel::High : LogicLevel::Low;
      LogicLevel released = Complement(asserted);
      if (device.kind == ComponentKind::Decoder)
      {
         AssertState enable = Sense(LogicPin(deviceIndex, 2), enablePolarity);
         LogicLevel selectA = LogicPin(deviceIndex, 0);
         LogicLevel selectB = LogicPin(deviceIndex, 1);
         for (uint8_t output = 0; output < 4; output += 1)
         {
            LogicLevel level = released;
            if (enable == AssertState::Unknown || (enable == AssertState::Active && (IsBinary(selectA) == false || IsBinary(selectB) == false)))
            {
               level = LogicLevel::Conflict;
            }
            else if (enable == AssertState::Active)
            {
               uint8_t selectCode = 0;
               if (selectA == LogicLevel::High)
               {
                  selectCode = static_cast<uint8_t>(selectCode | 1u);
               }
               if (selectB == LogicLevel::High)
               {
                  selectCode = static_cast<uint8_t>(selectCode | 2u);
               }
               level = selectCode == output ? asserted : released;
            }
            Drive(&plan, static_cast<uint8_t>(3 + output), level, stage);
         }
         return plan;
      }
      bool valid = true;
      uint8_t nibble = 0;
      for (uint8_t bitIndex = 0; bitIndex < 4; bitIndex += 1)
      {
         LogicLevel bitLevel = LogicPin(deviceIndex, bitIndex);
         if (IsBinary(bitLevel) == false)
         {
            valid = false;
         }
         else if (bitLevel == LogicLevel::High)
         {
            nibble = static_cast<uint8_t>(nibble | static_cast<uint8_t>(1u << bitIndex));
         }
      }
      uint8_t glyph = valid && nibble < DigitCount ? SegmentGlyph[nibble] : 0;
      for (uint8_t segment = 0; segment < SegmentCount; segment += 1)
      {
         LogicLevel level = LogicLevel::Conflict;
         if (valid)
         {
            level = BitIsSet(glyph, segment) ? asserted : released;
         }
         Drive(&plan, static_cast<uint8_t>(4 + segment), level, stage);
      }
      return plan;
   }
   if (device.kind == ComponentKind::FullAdder)
   {
      LogicLevel inputA = LogicPin(deviceIndex, 0);
      LogicLevel inputB = LogicPin(deviceIndex, 1);
      LogicLevel carryIn = LogicPin(deviceIndex, 2);
      if (IsBinary(inputA) == false || IsBinary(inputB) == false || IsBinary(carryIn) == false)
      {
         Drive(&plan, 3, LogicLevel::Conflict, stage);
         Drive(&plan, 4, LogicLevel::Conflict, stage);
         return plan;
      }
      uint8_t highCount = 0;
      if (inputA == LogicLevel::High)
      {
         highCount += 1;
      }
      if (inputB == LogicLevel::High)
      {
         highCount += 1;
      }
      if (carryIn == LogicLevel::High)
      {
         highCount += 1;
      }
      LogicLevel sum = (highCount % 2) == 1 ? LogicLevel::High : LogicLevel::Low;
      LogicLevel carryOut = highCount >= 2 ? LogicLevel::High : LogicLevel::Low;
      Drive(&plan, 3, sum, stage);
      Drive(&plan, 4, carryOut, stage);
      return plan;
   }
   if (device.kind == ComponentKind::Multiplexer)
   {
      uint8_t dataCount = device.pinCount >= 7 ? 4 : 2;
      uint8_t selectCount = device.pinCount >= 7 ? 2 : 1;
      uint8_t selectCode = 0;
      bool selectValid = true;
      for (uint8_t select = 0; select < selectCount; select += 1)
      {
         LogicLevel selectLevel = LogicPin(deviceIndex, static_cast<uint8_t>(dataCount + select));
         if (IsBinary(selectLevel) == false)
         {
            selectValid = false;
         }
         else if (selectLevel == LogicLevel::High)
         {
            selectCode = static_cast<uint8_t>(selectCode | static_cast<uint8_t>(1u << select));
         }
      }
      LogicLevel chosen = LogicLevel::Conflict;
      if (selectValid && selectCode < dataCount)
      {
         chosen = LogicPin(deviceIndex, selectCode);
      }
      if (device.pinCount > 0)
      {
         Drive(&plan, static_cast<uint8_t>(device.pinCount - 1), chosen, stage);
      }
      return plan;
   }
   if (device.kind == ComponentKind::Adc)
   {
      constexpr double AdcFullScaleCount = 16.0;
      constexpr uint8_t AdcCodeMaximum = 15;
      constexpr uint8_t AdcOutputBase = 3;
      double inputVolt = 0.0;
      double referenceVolt = 0.0;
      if (pAnalogVoltage != nullptr && device.pinCount > 1)
      {
         uint32_t inputNet = device.nets[0];
         uint32_t referenceNet = device.nets[1];
         if (inputNet < pAnalogVoltage->size())
         {
            inputVolt = (*pAnalogVoltage)[inputNet];
         }
         if (referenceNet < pAnalogVoltage->size())
         {
            referenceVolt = (*pAnalogVoltage)[referenceNet];
         }
      }
      uint8_t convertedCode = 0;
      if (referenceVolt > 0.0 && std::isfinite(inputVolt) && std::isfinite(referenceVolt))
      {
         double scaled = std::floor((inputVolt / referenceVolt) * AdcFullScaleCount);
         if (scaled < 0.0)
         {
            scaled = 0.0;
         }
         if (scaled > static_cast<double>(AdcCodeMaximum))
         {
            scaled = static_cast<double>(AdcCodeMaximum);
         }
         convertedCode = static_cast<uint8_t>(scaled);
      }
      for (uint8_t bit = 0; bit < 4; bit += 1)
      {
         uint8_t pin = static_cast<uint8_t>(AdcOutputBase + bit);
         if (pin >= device.pinCount)
         {
            break;
         }
         LogicLevel bitLevel = (convertedCode & static_cast<uint8_t>(1u << bit)) == 0 ? LogicLevel::Low : LogicLevel::High;
         Drive(&plan, pin, bitLevel, stage);
      }
      return plan;
   }
   return plan;
}

bool LogicSim::ApplyNow(uint32_t deviceIndex, const OutputPlan& plan)
{
   bool changed = false;
   DeviceMemory& slot = memory[deviceIndex];
   for (uint8_t pin = 0; pin < MaxPinCount; pin += 1)
   {
      if (plan.driven[pin] == false || slot.published[pin] == plan.level[pin])
      {
         continue;
      }
      slot.published[pin] = plan.level[pin];
      changed = true;
   }
   return changed;
}

void LogicSim::EnqueueAt(uint32_t deviceIndex, uint8_t pinIndex, LogicLevel level, int64_t timePs, uint32_t delta, PendingMode pending)
{
   if (pinIndex >= MaxPinCount || timePs < 0 || timePs > stopPs)
   {
      return;
   }
   if (delta >= MaxDeltaCycles)
   {
      code = Result::Oscillation;
      return;
   }
   DeviceMemory& slot = memory[deviceIndex];
   if (pending == PendingMode::ReplaceEarlier && slot.hasSchedule[pinIndex] && slot.scheduled[pinIndex] == level)
   {
      return;
   }
   if (slot.hasSchedule[pinIndex] == false && slot.published[pinIndex] == level)
   {
      return;
   }
   slot.generation[pinIndex] += 1;
   if (pending == PendingMode::ReplaceEarlier)
   {
      slot.validAfter[pinIndex] = slot.generation[pinIndex];
   }
   slot.hasSchedule[pinIndex] = true;
   slot.scheduled[pinIndex] = level;
   LogicEvent event = {};
   event.timePs = timePs;
   event.delta = delta;
   event.generation = slot.generation[pinIndex];
   event.deviceIndex = deviceIndex;
   event.pinIndex = pinIndex;
   event.level = level;
   events.push(event);
}

void LogicSim::SchedulePlan(uint32_t deviceIndex, const OutputPlan& plan, int64_t timePs, uint32_t delta)
{
   int64_t delayPs = 0;
   const LogicProperties* pLogic = LogicOf(pNetlist->devices[deviceIndex].properties);
   if (pLogic != nullptr)
   {
      delayPs = SecondsToPicoseconds(pLogic->propagationDelaySec);
   }
   int64_t whenPs = timePs;
   uint32_t whenDelta = delta + 1;
   if (delayPs > 0)
   {
      if (timePs > TimeLimitPs - delayPs)
      {
         code = Result::Oscillation;
         return;
      }
      whenPs = timePs + delayPs;
      whenDelta = 0;
   }
   for (uint8_t pin = 0; pin < MaxPinCount; pin += 1)
   {
      if (plan.driven[pin] == false)
      {
         continue;
      }
      EnqueueAt(deviceIndex, pin, plan.level[pin], whenPs, whenDelta, PendingMode::ReplaceEarlier);
      if (code == Result::Oscillation)
      {
         return;
      }
   }
}

Result LogicSim::Settle(void)
{
   Resolve();
   bool changed = true;
   uint32_t pass = 0;
   while (changed)
   {
      if (pass >= MaxDeltaCycles)
      {
         code = Result::Oscillation;
         return code;
      }
      pass += 1;
      changed = false;
      for (uint32_t index = 0; index < pNetlist->devices.size(); index += 1)
      {
         if (isLogic[index] == false)
         {
            continue;
         }
         OutputPlan plan = Compute(index, EvalMode::Settle, 0);
         if (ApplyNow(index, plan))
         {
            changed = true;
            Resolve();
         }
      }
   }
   return Result::Ok;
}

void LogicSim::ArmMemory(void)
{
   for (uint32_t index = 0; index < pNetlist->devices.size(); index += 1)
   {
      const NetlistDevice& device = pNetlist->devices[index];
      if (isLogic[index] == false)
      {
         continue;
      }
      DeviceMemory& slot = memory[index];
      slot.dataStableSincePs = 0;
      slot.lastEdgePs = -1;
      if (device.kind == ComponentKind::DFlipFlop || device.kind == ComponentKind::TFlipFlop)
      {
         slot.previousClock = RawPin(index, 1);
         slot.previousData = LogicPin(index, 0);
      }
      else if (device.kind == ComponentKind::JkFlipFlop)
      {
         slot.previousClock = RawPin(index, 2);
         slot.previousData = LogicPin(index, 0);
      }
      else if (device.kind == ComponentKind::Counter || device.kind == ComponentKind::ShiftRegister)
      {
         slot.previousClock = RawPin(index, 0);
      }
   }
}

void LogicSim::ScheduleClocks(void)
{
   for (uint32_t index = 0; index < pNetlist->devices.size(); index += 1)
   {
      const NetlistDevice& device = pNetlist->devices[index];
      const ClockProperties* pClock = std::get_if<ClockProperties>(&device.properties);
      if (pClock == nullptr)
      {
         continue;
      }
      if (device.kind != ComponentKind::Clock)
      {
         continue;
      }
      int64_t delayPs = 0;
      if (const LogicProperties* pLogic = LogicOf(device.properties))
      {
         delayPs = SecondsToPicoseconds(pLogic->propagationDelaySec);
      }
      LogicLevel previous = ClockLevelAt(*pClock, 0);
      int64_t cursor = 0;
      for (uint32_t edge = 0; edge < MaxClockEdges; edge += 1)
      {
         int64_t next = NextClockEdge(*pClock, cursor);
         if (next < 0)
         {
            break;
         }
         LogicLevel level = ClockLevelAt(*pClock, next);
         int64_t whenPs = next;
         if (delayPs > 0 && next <= TimeLimitPs - delayPs)
         {
            whenPs = next + delayPs;
         }
         if (whenPs > stopPs)
         {
            break;
         }
         if (level != previous)
         {
            EnqueueAt(index, 0, level, whenPs, 0, PendingMode::KeepEarlier);
            previous = level;
         }
         if (next <= cursor)
         {
            break;
         }
         cursor = next;
      }
   }
}

void LogicSim::Publish(DcResult* pResult) const
{
   if (pResult == nullptr)
   {
      return;
   }
   if (pResult->netLevel.size() < levels.size())
   {
      pResult->netLevel.assign(levels.size(), LogicLevel::HighZ);
   }
   if (pResult->netVoltage.size() < levels.size())
   {
      pResult->netVoltage.assign(levels.size(), 0.0);
   }
   for (size_t net = 0; net < levels.size(); net += 1)
   {
      pResult->netLevel[net] = levels[net];
   }
   for (NodeValue& node : pResult->nodes)
   {
      if (node.netIndex < levels.size())
      {
         node.level = levels[node.netIndex];
      }
   }
   if (pResult->nodes.empty())
   {
      for (uint32_t net = 0; net < levels.size(); net += 1)
      {
         NodeValue node = {};
         node.netIndex = net;
         node.name = net < pNetlist->netNames.size() ? pNetlist->netNames[net] : std::to_string(net);
         node.value = 0.0;
         node.phaseDeg = 0.0;
         node.level = levels[net];
         pResult->nodes.push_back(node);
      }
   }
   if (pResult->warnings.empty())
   {
      pResult->warnings = pNetlist->warnings;
   }
   for (const std::string& note : timingNotes)
   {
      pResult->warnings.push_back(note);
   }
   for (size_t index = 0; index < pNetlist->devices.size(); index += 1)
   {
      if (isLogic[index] == false)
      {
         continue;
      }
      const NetlistDevice& device = pNetlist->devices[index];
      PinSet outputs = OutputPins(device);
      if (outputs.count == 0 || outputs.pins[0] >= device.pinCount)
      {
         continue;
      }
      uint32_t net = device.nets[outputs.pins[0]];
      BranchValue branch = {};
      branch.componentId = device.componentId;
      branch.name = device.name;
      branch.currentAmpere = 0.0;
      branch.voltageVolt = 0.0;
      branch.powerWatt = 0.0;
      branch.level = net < levels.size() ? levels[net] : LogicLevel::HighZ;
      pResult->branches.push_back(branch);
   }
}

Result LogicSim::AdvanceTo(int64_t timePs, const LogicLevelList* pHeldLevels, CancelToken* pCancel)
{
   while (events.empty() == false && code != Result::Oscillation)
   {
      if (pCancel != nullptr && pCancel->IsRequested())
      {
         code = Result::Error;
         return code;
      }
      LogicEvent first = events.top();
      if (first.timePs > timePs || first.timePs > stopPs)
      {
         break;
      }
      std::vector<LogicEvent> batch;
      while (events.empty() == false)
      {
         LogicEvent next = events.top();
         if (next.timePs != first.timePs || next.delta != first.delta)
         {
            break;
         }
         events.pop();
         batch.push_back(next);
      }
      bool driveChanged = false;
      for (const LogicEvent& event : batch)
      {
         if (event.deviceIndex >= memory.size() || event.pinIndex >= MaxPinCount)
         {
            continue;
         }
         DeviceMemory& slot = memory[event.deviceIndex];
         if (event.generation < slot.validAfter[event.pinIndex])
         {
            continue;
         }
         slot.hasSchedule[event.pinIndex] = false;
         if (slot.published[event.pinIndex] == event.level)
         {
            continue;
         }
         slot.published[event.pinIndex] = event.level;
         driveChanged = true;
      }
      if (driveChanged == false)
      {
         continue;
      }
      LogicLevelList before = levels;
      Resolve();
      std::vector<bool> dirty(memory.size(), false);
      for (size_t net = 0; net < levels.size(); net += 1)
      {
         if (levels[net] == before[net])
         {
            continue;
         }
         for (uint32_t listener : fanout[net])
         {
            if (listener < dirty.size())
            {
               dirty[listener] = true;
            }
         }
      }
      for (uint32_t index = 0; index < dirty.size(); index += 1)
      {
         if (dirty[index] == false)
         {
            continue;
         }
         OutputPlan plan = Compute(index, EvalMode::Event, first.timePs);
         SchedulePlan(index, plan, first.timePs, first.delta);
         if (code == Result::Oscillation)
         {
            return code;
         }
      }
   }
   if (pHeldLevels == nullptr)
   {
      return code;
   }
   std::vector<bool> dirty(memory.size(), false);
   bool levelChanged = false;
   for (size_t net = 0; net < levels.size(); net += 1)
   {
      if (net >= holdNet.size() || holdNet[net] == 0 || net >= pHeldLevels->size())
      {
         continue;
      }
      if (levels[net] == (*pHeldLevels)[net])
      {
         continue;
      }
      levels[net] = (*pHeldLevels)[net];
      levelChanged = true;
      if (net < fanout.size())
      {
         for (uint32_t listener : fanout[net])
         {
            if (listener < dirty.size())
            {
               dirty[listener] = true;
            }
         }
      }
   }
   if (levelChanged == false)
   {
      return code;
   }
   for (uint32_t index = 0; index < dirty.size(); index += 1)
   {
      if (dirty[index] == false)
      {
         continue;
      }
      OutputPlan plan = Compute(index, EvalMode::Event, timePs);
      SchedulePlan(index, plan, timePs, 0);
      if (code == Result::Oscillation)
      {
         return code;
      }
   }
   return code;
}

Result LogicSim::Run(CancelToken* pCancel, TransientResult* pResult)
{
   TransientSample initial = {};
   initial.timeSec = 0.0;
   initial.netVoltage.assign(levels.size(), 0.0);
   initial.netLevel = levels;
   pResult->samples.push_back(initial);
   while (events.empty() == false && code != Result::Oscillation)
   {
      if (pCancel != nullptr && pCancel->IsRequested())
      {
         code = Result::Error;
         pResult->message = "Cancelled.";
         return code;
      }
      LogicEvent first = events.top();
      if (first.timePs > stopPs)
      {
         break;
      }
      std::vector<LogicEvent> batch;
      while (events.empty() == false)
      {
         LogicEvent next = events.top();
         if (next.timePs != first.timePs || next.delta != first.delta)
         {
            break;
         }
         events.pop();
         batch.push_back(next);
      }
      bool driveChanged = false;
      for (const LogicEvent& event : batch)
      {
         if (event.deviceIndex >= memory.size() || event.pinIndex >= MaxPinCount)
         {
            continue;
         }
         DeviceMemory& slot = memory[event.deviceIndex];
         if (event.generation < slot.validAfter[event.pinIndex])
         {
            continue;
         }
         slot.hasSchedule[event.pinIndex] = false;
         if (slot.published[event.pinIndex] == event.level)
         {
            continue;
         }
         slot.published[event.pinIndex] = event.level;
         driveChanged = true;
      }
      if (driveChanged == false)
      {
         continue;
      }
      LogicLevelList before = levels;
      Resolve();
      std::vector<bool> dirty(memory.size(), false);
      bool levelChanged = false;
      for (size_t net = 0; net < levels.size(); net += 1)
      {
         if (levels[net] == before[net])
         {
            continue;
         }
         levelChanged = true;
         for (uint32_t listener : fanout[net])
         {
            if (listener < dirty.size())
            {
               dirty[listener] = true;
            }
         }
      }
      if (levelChanged)
      {
         TransientSample sample = {};
         sample.timeSec = static_cast<double>(first.timePs) / PicosecondsPerSecond;
         sample.netVoltage.assign(levels.size(), 0.0);
         sample.netLevel = levels;
         pResult->samples.push_back(sample);
      }
      for (uint32_t index = 0; index < dirty.size(); index += 1)
      {
         if (dirty[index] == false)
         {
            continue;
         }
         OutputPlan plan = Compute(index, EvalMode::Event, first.timePs);
         SchedulePlan(index, plan, first.timePs, first.delta);
         if (code == Result::Oscillation)
         {
            return code;
         }
      }
   }
   if (pResult->samples.empty() == false)
   {
      double stopSec = static_cast<double>(stopPs) / PicosecondsPerSecond;
      if (pResult->samples.back().timeSec < stopSec)
      {
         TransientSample endpoint = {};
         endpoint.timeSec = stopSec;
         endpoint.netVoltage.assign(levels.size(), 0.0);
         endpoint.netLevel = levels;
         pResult->samples.push_back(endpoint);
      }
   }
   if (code == Result::Ok && timingNotes.empty() == false)
   {
      code = Result::Warning;
   }
   return code;
}

} // namespace

Result PrepareHeldSim(const Netlist& netlist, const LogicLevelList& seededLevels, const HoldMask& holdMask, const VoltageList* pAnalogVoltage, LogicSim* pSim)
{
   if (pSim == nullptr)
   {
      return Result::InvalidParameter;
   }
   Result bound = pSim->Bind(&netlist);
   if (IsErr(bound))
   {
      return bound;
   }
   pSim->pAnalogVoltage = pAnalogVoltage;
   pSim->holdNet = holdMask;
   if (pSim->holdNet.size() < pSim->levels.size())
   {
      pSim->holdNet.resize(pSim->levels.size(), 0);
   }
   for (size_t net = 0; net < pSim->levels.size(); net += 1)
   {
      if (pSim->holdNet[net] == 0 || net >= seededLevels.size())
      {
         continue;
      }
      pSim->levels[net] = seededLevels[net];
   }
   return pSim->Settle();
}

Result SolveDigitalDc(const Netlist& netlist, DcResult* pResult)
{
   HoldMask emptyMask;
   LogicLevelList emptyLevels;
   return SolveDigitalDcHeld(netlist, emptyLevels, emptyMask, pResult);
}

Result SolveDigitalDcHeld(const Netlist& netlist, const LogicLevelList& seededLevels, const HoldMask& holdMask, DcResult* pResult, const VoltageList* pAnalogVoltage)
{
   if (pResult == nullptr)
   {
      return Result::InvalidParameter;
   }
   LogicSim sim;
   Result settled = PrepareHeldSim(netlist, seededLevels, holdMask, pAnalogVoltage, &sim);
   if (sim.pNetlist == nullptr)
   {
      return settled;
   }
   sim.Publish(pResult);
   if (IsErr(settled))
   {
      pResult->code = settled;
      pResult->message = Describe(settled);
      return settled;
   }
   return Result::Ok;
}

Result ReadSettledLevels(const Netlist& netlist, const LogicLevelList& seededLevels, const HoldMask& holdMask, LogicLevelList* pLevels, const VoltageList* pAnalogVoltage)
{
   if (pLevels == nullptr)
   {
      return Result::InvalidParameter;
   }
   LogicSim sim;
   Result settled = PrepareHeldSim(netlist, seededLevels, holdMask, pAnalogVoltage, &sim);
   *pLevels = sim.levels;
   return settled;
}

Result SolveDigitalTransient(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel, TransientResult* pResult)
{
   if (pResult == nullptr)
   {
      return Result::InvalidParameter;
   }
   pResult->code = Result::Ok;
   pResult->message = Describe(Result::Ok);
   if (settings.stopTimeSec <= 0.0)
   {
      pResult->code = Result::InvalidTimeStep;
      pResult->message = Describe(Result::InvalidTimeStep);
      return pResult->code;
   }
   LogicSim sim;
   Result bound = sim.Bind(&netlist);
   if (IsErr(bound))
   {
      pResult->code = bound;
      pResult->message = Describe(bound);
      return bound;
   }
   sim.stopPs = SecondsToPicoseconds(settings.stopTimeSec);
   Result settled = sim.Settle();
   sim.Publish(&pResult->initial);
   pResult->initial.code = settled;
   pResult->initial.message = Describe(settled);
   if (IsErr(settled))
   {
      pResult->code = settled;
      pResult->message = Describe(settled);
      return settled;
   }
   sim.ArmMemory();
   sim.ScheduleClocks();
   Result ran = sim.Run(pCancel, pResult);
   for (const std::string& note : sim.timingNotes)
   {
      pResult->initial.warnings.push_back(note);
   }
   pResult->code = ran;
   pResult->message = ran == Result::Error ? pResult->message : Describe(ran);
   pResult->warnings = pResult->initial.warnings;
   return ran;
}

struct LogicSession
{
   LogicSim sim;
};

Result OpenLogicSession(const Netlist& netlist, const AnalysisSettings& settings, const HoldMask& holdMask, const LogicLevelList& seededLevels, const VoltageList* pAnalogVoltage, LogicSession** pSession, LogicLevelList* pLevels)
{
   if (pSession == nullptr)
   {
      return Result::InvalidParameter;
   }
   *pSession = nullptr;
   if (settings.stopTimeSec <= 0.0)
   {
      return Result::InvalidTimeStep;
   }
   LogicSession* pCreated = new LogicSession();
   Result bound = pCreated->sim.Bind(&netlist);
   if (IsErr(bound))
   {
      delete pCreated;
      return bound;
   }
   pCreated->sim.stopPs = SecondsToPicoseconds(settings.stopTimeSec);
   pCreated->sim.pAnalogVoltage = pAnalogVoltage;
   pCreated->sim.holdNet = holdMask;
   if (pCreated->sim.holdNet.size() < pCreated->sim.levels.size())
   {
      pCreated->sim.holdNet.resize(pCreated->sim.levels.size(), 0);
   }
   for (size_t net = 0; net < pCreated->sim.levels.size(); net += 1)
   {
      if (pCreated->sim.holdNet[net] == 0 || net >= seededLevels.size())
      {
         continue;
      }
      pCreated->sim.levels[net] = seededLevels[net];
   }
   Result settled = pCreated->sim.Settle();
   pCreated->sim.ArmMemory();
   pCreated->sim.ScheduleClocks();
   if (pLevels != nullptr)
   {
      *pLevels = pCreated->sim.levels;
   }
   if (IsErr(settled))
   {
      delete pCreated;
      return settled;
   }
   *pSession = pCreated;
   return Result::Ok;
}

void CloseLogicSession(LogicSession* pSession)
{
   delete pSession;
}

int64_t NextLogicEventPicoseconds(const LogicSession* pSession)
{
   if (pSession == nullptr || pSession->sim.events.empty())
   {
      return -1;
   }
   LogicEvent next = pSession->sim.events.top();
   if (next.timePs > pSession->sim.stopPs)
   {
      return -1;
   }
   return next.timePs;
}

Result AdvanceLogicSession(LogicSession* pSession, int64_t timePs, const LogicLevelList* pHeldLevels, CancelToken* pCancel, LogicLevelList* pLevels)
{
   if (pSession == nullptr)
   {
      return Result::InvalidParameter;
   }
   Result advanced = pSession->sim.AdvanceTo(timePs, pHeldLevels, pCancel);
   if (pLevels != nullptr)
   {
      *pLevels = pSession->sim.levels;
   }
   return advanced;
}

} // namespace Analyzer
