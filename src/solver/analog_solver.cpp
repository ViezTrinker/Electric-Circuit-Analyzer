/*!
 * \file analog_solver.cpp
 * \brief Modified nodal analysis for DC, AC, and transient circuits.
 */

#include "analog_solver.h"

#include "dense_system.h"
#include "device_model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>
#include <string>

namespace Analyzer
{

namespace
{

constexpr uint32_t NoBranch = UINT32_MAX;
constexpr uint32_t MaxNewtonIterations = 80;
constexpr uint8_t SegmentLeadCount = 8;
constexpr uint8_t SegmentCommonPin = 8;
constexpr uint8_t DacBitCount = 4;
constexpr uint8_t DacReferencePin = 4;
constexpr uint8_t DacOutputPin = 5;
constexpr double DacFullScaleCode = 15.0;
constexpr double MinimumDriveOhm = 1.0e-3;
constexpr uint32_t MaxTransientSteps = 200000;
constexpr double VoltageAbsoluteTolerance = 1.0e-6;
constexpr double VoltageRelativeTolerance = 1.0e-5;
constexpr double JunctionStepFactor = 2.0;
constexpr uint8_t TimerSupplyPin = 0;
constexpr uint8_t TimerGroundPin = 1;
constexpr uint8_t TimerTriggerPin = 2;
constexpr uint8_t TimerThresholdPin = 3;
constexpr uint8_t TimerDischargePin = 4;
constexpr uint8_t TimerOutputPin = 5;
constexpr uint8_t TimerResetPin = 6;
constexpr double TimerTriggerFraction = 1.0 / 3.0;
constexpr double TimerThresholdFraction = 2.0 / 3.0;
constexpr double TimerResetFraction = 0.4;
constexpr double TimerNoSchedule = -1.0;
constexpr uint32_t TimerSettleExtraIterations = 2;
constexpr uint8_t TimerOutputLow = 0;

struct SolverDevice
{
   uint32_t branch;
   uint32_t extra[8];
};

struct PreparedCircuit
{
   Result code;
   std::string message;
   uint32_t totalNets;
   uint32_t branchCount;
   std::vector<SolverDevice> devices;
};

using ComplexValue = std::complex<double>;

template <typename ScalarType>
struct NodeRef
{
   bool mapped;
   uint32_t index;
};

template <typename ScalarType>
NodeRef<ScalarType> MapNode(uint32_t net)
{
   NodeRef<ScalarType> node{false, 0};
   if (net != GroundNetIndex)
   {
      node.mapped = true;
      node.index = net - 1;
   }
   return node;
}

template <typename ScalarType>
void StampConductance(DenseSystem<ScalarType>* pSystem, uint32_t netA, uint32_t netB, ScalarType conductance)
{
   NodeRef<ScalarType> nodeA = MapNode<ScalarType>(netA);
   NodeRef<ScalarType> nodeB = MapNode<ScalarType>(netB);
   if (nodeA.mapped)
   {
      pSystem->Add(nodeA.index, nodeA.index, conductance);
   }
   if (nodeB.mapped)
   {
      pSystem->Add(nodeB.index, nodeB.index, conductance);
   }
   if (nodeA.mapped && nodeB.mapped)
   {
      pSystem->Add(nodeA.index, nodeB.index, -conductance);
      pSystem->Add(nodeB.index, nodeA.index, -conductance);
   }
}

template <typename ScalarType>
void StampInjection(DenseSystem<ScalarType>* pSystem, uint32_t net, ScalarType current)
{
   NodeRef<ScalarType> node = MapNode<ScalarType>(net);
   if (node.mapped)
   {
      pSystem->AddRhs(node.index, current);
   }
}

template <typename ScalarType>
void StampVoltageSource(DenseSystem<ScalarType>* pSystem, uint32_t netPlus, uint32_t netMinus, uint32_t branch, ScalarType volts)
{
   NodeRef<ScalarType> plus = MapNode<ScalarType>(netPlus);
   NodeRef<ScalarType> minus = MapNode<ScalarType>(netMinus);
   if (plus.mapped)
   {
      pSystem->Add(plus.index, branch, ScalarType(-1.0));
      pSystem->Add(branch, plus.index, ScalarType(1.0));
   }
   if (minus.mapped)
   {
      pSystem->Add(minus.index, branch, ScalarType(1.0));
      pSystem->Add(branch, minus.index, ScalarType(-1.0));
   }
   pSystem->AddRhs(branch, volts);
}

double VoltageBetween(const VoltageList& voltage, uint32_t netA, uint32_t netB)
{
   double left = netA < voltage.size() ? voltage[netA] : 0.0;
   double right = netB < voltage.size() ? voltage[netB] : 0.0;
   return left - right;
}

bool SameNet(uint32_t netA, uint32_t netB)
{
   return netA == netB;
}

Result RequireFinitePositive(double value)
{
   if (std::isfinite(value) == false || value <= 0.0)
   {
      return Result::InvalidParameter;
   }
   return Result::Ok;
}

void UniteNet(std::vector<uint32_t>* pParent, uint32_t left, uint32_t right)
{
   uint32_t leftRoot = left;
   uint32_t rightRoot = right;
   while ((*pParent)[leftRoot] != leftRoot)
   {
      leftRoot = (*pParent)[leftRoot];
   }
   while ((*pParent)[rightRoot] != rightRoot)
   {
      rightRoot = (*pParent)[rightRoot];
   }
   if (leftRoot != rightRoot)
   {
      (*pParent)[rightRoot] = leftRoot;
   }
}

uint32_t FindNet(std::vector<uint32_t>* pParent, uint32_t net)
{
   uint32_t root = net;
   while ((*pParent)[root] != root)
   {
      root = (*pParent)[root];
   }
   return root;
}

bool WouldLoop(std::vector<uint32_t>* pParent, uint32_t left, uint32_t right)
{
   if (FindNet(pParent, left) == FindNet(pParent, right))
   {
      return true;
   }
   UniteNet(pParent, left, right);
   return false;
}

double SourceVoltageAt(const VoltageSourceProperties& source, double timeSec, bool transient)
{
   if (transient == false)
   {
      return source.dcVoltageVolt;
   }
   if (source.transientWaveform == SourceWaveform::Sine)
   {
      if (timeSec < source.sineDelaySec)
      {
         return source.sineOffsetVolt;
      }
      double elapsed = timeSec - source.sineDelaySec;
      double angle = (2.0 * std::numbers::pi * source.sineFrequencyHz * elapsed) + (source.sinePhaseDeg * std::numbers::pi / 180.0);
      return source.sineOffsetVolt + (source.sineAmplitudeVolt * std::sin(angle));
   }
   if (source.transientWaveform == SourceWaveform::Pulse)
   {
      if (source.pulsePeriodSec <= 0.0 || timeSec < source.pulseDelaySec)
      {
         return source.pulseInitialVolt;
      }
      double elapsed = std::fmod(timeSec - source.pulseDelaySec, source.pulsePeriodSec);
      double riseEnd = source.pulseRiseSec;
      double highEnd = riseEnd + source.pulseWidthSec;
      double fallEnd = highEnd + source.pulseFallSec;
      if (elapsed < riseEnd && source.pulseRiseSec > 0.0)
      {
         double blend = elapsed / source.pulseRiseSec;
         return source.pulseInitialVolt + (blend * (source.pulseLevelVolt - source.pulseInitialVolt));
      }
      if (elapsed < highEnd)
      {
         return source.pulseLevelVolt;
      }
      if (elapsed < fallEnd && source.pulseFallSec > 0.0)
      {
         double blend = (elapsed - highEnd) / source.pulseFallSec;
         return source.pulseLevelVolt + (blend * (source.pulseInitialVolt - source.pulseLevelVolt));
      }
      return source.pulseInitialVolt;
   }
   return source.dcVoltageVolt;
}

double SourceCurrentAt(const CurrentSourceProperties& source, double timeSec, bool transient)
{
   if (transient == false)
   {
      return source.dcCurrentAmpere;
   }
   if (source.transientWaveform == SourceWaveform::Sine)
   {
      if (timeSec < source.sineDelaySec)
      {
         return source.sineOffsetAmpere;
      }
      double elapsed = timeSec - source.sineDelaySec;
      double angle = (2.0 * std::numbers::pi * source.sineFrequencyHz * elapsed) + (source.sinePhaseDeg * std::numbers::pi / 180.0);
      return source.sineOffsetAmpere + (source.sineAmplitudeAmpere * std::sin(angle));
   }
   if (source.transientWaveform == SourceWaveform::Pulse)
   {
      if (source.pulsePeriodSec <= 0.0 || timeSec < source.pulseDelaySec)
      {
         return source.pulseInitialAmpere;
      }
      double elapsed = std::fmod(timeSec - source.pulseDelaySec, source.pulsePeriodSec);
      if (elapsed < source.pulseRiseSec + source.pulseWidthSec)
      {
         return source.pulseLevelAmpere;
      }
      return source.pulseInitialAmpere;
   }
   return source.dcCurrentAmpere;
}

bool SwitchClosedAt(const SpstProperties& properties, double timeSec, bool transient)
{
   SwitchState state = properties.restState;
   if (transient)
   {
      for (const SwitchEvent& event : properties.events)
      {
         if (event.timeSec <= timeSec)
         {
            state = event.state;
         }
      }
   }
   return state == SwitchState::Closed;
}

bool PushbuttonClosedAt(const PushbuttonProperties& properties, double timeSec, bool transient)
{
   SwitchState mechanical = properties.mechanicalState;
   if (transient)
   {
      for (const SwitchEvent& event : properties.events)
      {
         if (event.timeSec <= timeSec)
         {
            mechanical = event.state;
         }
      }
   }
   bool pressed = mechanical == SwitchState::Closed;
   if (properties.contact == ContactForm::NormallyOpen)
   {
      return pressed;
   }
   return pressed == false;
}

enum class SwitchSchedule : int8_t
{
   OperatingPoint = 0,
   Transient = 1
};

PreparedCircuit Prepare(const Netlist& netlist, bool directCurrent, SwitchSchedule schedule, double timeSec)
{
   PreparedCircuit prepared = {};
   prepared.code = Result::Ok;
   prepared.totalNets = netlist.netCount;
   prepared.branchCount = 0;
   prepared.devices.resize(netlist.devices.size());
   std::vector<uint32_t> parent(std::max<uint32_t>(netlist.netCount, 1));
   for (uint32_t net = 0; net < parent.size(); net += 1)
   {
      parent[net] = net;
   }
   for (size_t index = 0; index < netlist.devices.size(); index += 1)
   {
      const NetlistDevice& device = netlist.devices[index];
      SolverDevice& slot = prepared.devices[index];
      slot.branch = NoBranch;
      for (uint8_t extra = 0; extra < 8; extra += 1)
      {
         slot.extra[extra] = device.pinCount > extra ? device.nets[extra] : GroundNetIndex;
      }
      bool idealShort = false;
      uint32_t shortA = device.nets[0];
      uint32_t shortB = device.pinCount > 1 ? device.nets[1] : device.nets[0];
      if (device.kind == ComponentKind::VoltageSource)
      {
         idealShort = true;
         shortA = device.nets[1];
         shortB = device.nets[0];
      }
      else if (device.kind == ComponentKind::Ammeter)
      {
         const MeterProperties* pMeter = std::get_if<MeterProperties>(&device.properties);
         if (pMeter == nullptr)
         {
            prepared.code = Result::InvalidParameter;
            prepared.message = device.name + " is missing meter properties.";
            return prepared;
         }
         if (pMeter->mode == MeterMode::Ideal || pMeter->burdenResistanceOhm <= 0.0)
         {
            idealShort = true;
         }
      }
      else if (device.kind == ComponentKind::Inductor && directCurrent)
      {
         const InductorProperties* pInductor = std::get_if<InductorProperties>(&device.properties);
         if (pInductor != nullptr && pInductor->seriesResistanceOhm <= 0.0)
         {
            idealShort = true;
         }
      }
      else if (device.kind == ComponentKind::SpstSwitch)
      {
         const SpstProperties* pSwitch = std::get_if<SpstProperties>(&device.properties);
         bool atEvent = schedule == SwitchSchedule::Transient;
         bool closed = pSwitch != nullptr && SwitchClosedAt(*pSwitch, atEvent ? timeSec : 0.0, atEvent);
         if (closed && pSwitch->closedMode == ResistanceMode::Ideal)
         {
            idealShort = true;
         }
      }
      else if (device.kind == ComponentKind::Pushbutton)
      {
         const PushbuttonProperties* pButton = std::get_if<PushbuttonProperties>(&device.properties);
         bool atEvent = schedule == SwitchSchedule::Transient;
         bool closed = pButton != nullptr && PushbuttonClosedAt(*pButton, atEvent ? timeSec : 0.0, atEvent);
         if (closed && pButton->closedMode == ResistanceMode::Ideal)
         {
            idealShort = true;
         }
      }
      else if (device.kind == ComponentKind::SpdtSwitch)
      {
         const SpdtProperties* pSpdt = std::get_if<SpdtProperties>(&device.properties);
         if (pSpdt != nullptr && device.pinCount >= 3 && pSpdt->closedMode == ResistanceMode::Ideal)
         {
            idealShort = true;
            shortA = device.nets[0];
            shortB = pSpdt->restPosition == ThrowPosition::ThrowB ? device.nets[2] : device.nets[1];
         }
      }
      if (idealShort)
      {
         if (WouldLoop(&parent, shortA, shortB))
         {
            prepared.code = Result::VoltageSourceLoop;
            prepared.message = device.name + " closes a loop of voltage sources or ideal shorts.";
            return prepared;
         }
         slot.branch = prepared.branchCount;
         prepared.branchCount += 1;
      }
      if (device.kind == ComponentKind::DipSwitch)
      {
         const DipSwitchProperties* pDip = std::get_if<DipSwitchProperties>(&device.properties);
         for (uint8_t channel = 0; channel < DipChannelCount; channel += 1)
         {
            slot.extra[channel] = NoBranch;
            if (pDip == nullptr || device.pinCount < static_cast<uint8_t>(DipChannelCount * 2))
            {
               continue;
            }
            if (pDip->channelState[channel] == SwitchState::Closed && pDip->closedMode == ResistanceMode::Ideal)
            {
               uint32_t leftNet = device.nets[channel * 2];
               uint32_t rightNet = device.nets[channel * 2 + 1];
               if (WouldLoop(&parent, leftNet, rightNet))
               {
                  prepared.code = Result::VoltageSourceLoop;
                  prepared.message = device.name + " closes a loop of voltage sources or ideal shorts.";
                  return prepared;
               }
               slot.extra[channel] = prepared.branchCount;
               prepared.branchCount += 1;
            }
         }
      }
      if (device.kind == ComponentKind::Diode || device.kind == ComponentKind::Led)
      {
         const DiodeProperties* pDiode = std::get_if<DiodeProperties>(&device.properties);
         if (device.kind == ComponentKind::Led)
         {
            const LedProperties* pLed = std::get_if<LedProperties>(&device.properties);
            pDiode = pLed == nullptr ? nullptr : &pLed->diode;
         }
         if (pDiode != nullptr && pDiode->seriesResistanceOhm > 0.0)
         {
            slot.extra[0] = prepared.totalNets;
            prepared.totalNets += 1;
         }
      }
      if (device.kind == ComponentKind::Capacitor)
      {
         const CapacitorProperties* pCapacitor = std::get_if<CapacitorProperties>(&device.properties);
         if (pCapacitor != nullptr && pCapacitor->esrOhm > 0.0)
         {
            slot.extra[0] = prepared.totalNets;
            prepared.totalNets += 1;
         }
      }
      if (device.kind == ComponentKind::SevenSegment)
      {
         const SevenSegmentProperties* pDisplay = std::get_if<SevenSegmentProperties>(&device.properties);
         if (pDisplay != nullptr && pDisplay->seriesResistanceOhm > 0.0)
         {
            for (uint8_t segment = 0; segment < SegmentLeadCount; segment += 1)
            {
               slot.extra[segment] = prepared.totalNets;
               prepared.totalNets += 1;
            }
         }
      }
   }
   if (prepared.totalNets > parent.size())
   {
      // Extra series nodes are not part of the ideal-short graph.
   }
   return prepared;
}

uint32_t UnknownCountOf(const PreparedCircuit& prepared)
{
   if (prepared.totalNets == 0)
   {
      return prepared.branchCount;
   }
   return (prepared.totalNets - 1) + prepared.branchCount;
}

void StampGmin(DenseSystem<double>* pSystem, uint32_t totalNets, double gminSiemens)
{
   for (uint32_t net = 1; net < totalNets; net += 1)
   {
      StampConductance(pSystem, net, GroundNetIndex, gminSiemens);
   }
}

template <typename ScalarType>
void AddGain(DenseSystem<ScalarType>* pSystem, uint32_t rowNet, uint32_t columnNet, ScalarType gain)
{
   NodeRef<ScalarType> row = MapNode<ScalarType>(rowNet);
   NodeRef<ScalarType> column = MapNode<ScalarType>(columnNet);
   if (row.mapped && column.mapped)
   {
      pSystem->Add(row.index, column.index, gain);
   }
}

template <typename ScalarType>
void AddControlled(DenseSystem<ScalarType>* pSystem, uint32_t intoNet, uint32_t plusNet, uint32_t minusNet, ScalarType gain)
{
   AddGain(pSystem, intoNet, plusNet, gain);
   AddGain(pSystem, intoNet, minusNet, -gain);
}

void RecordSample(TransientResult* pResult, const VoltageList& voltage, uint32_t netCount, double timeSec)
{
   TransientSample sample = {};
   sample.timeSec = timeSec;
   uint32_t kept = netCount;
   if (static_cast<uint32_t>(voltage.size()) < kept)
   {
      kept = static_cast<uint32_t>(voltage.size());
   }
   sample.netVoltage.assign(voltage.begin(), voltage.begin() + kept);
   sample.netLevel.assign(netCount, LogicLevel::HighZ);
   pResult->samples.push_back(sample);
}

Result StampResistor(DenseSystem<double>* pSystem, uint32_t netA, uint32_t netB, double resistanceOhm)
{
   if (IsErr(RequireFinitePositive(resistanceOhm)))
   {
      return Result::InvalidParameter;
   }
   if (SameNet(netA, netB))
   {
      return Result::Ok;
   }
   StampConductance(pSystem, netA, netB, 1.0 / resistanceOhm);
   return Result::Ok;
}

double PinVoltage(const VoltageList& voltage, uint32_t net)
{
   if (net >= voltage.size())
   {
      return 0.0;
   }
   return voltage[net];
}

uint8_t NextTimerLevel(const NetlistDevice& device, const VoltageList& voltage, uint8_t currentLevel)
{
   if (device.pinCount <= TimerResetPin)
   {
      return currentLevel;
   }
   double ground = PinVoltage(voltage, device.nets[TimerGroundPin]);
   double span = PinVoltage(voltage, device.nets[TimerSupplyPin]) - ground;
   if (span <= 0.0)
   {
      return TimerOutputLow;
   }
   double reset = PinVoltage(voltage, device.nets[TimerResetPin]) - ground;
   if (reset < TimerResetFraction * span)
   {
      return TimerOutputLow;
   }
   double trigger = PinVoltage(voltage, device.nets[TimerTriggerPin]) - ground;
   if (trigger < TimerTriggerFraction * span)
   {
      return 1;
   }
   double threshold = PinVoltage(voltage, device.nets[TimerThresholdPin]) - ground;
   if (threshold > TimerThresholdFraction * span)
   {
      return TimerOutputLow;
   }
   return currentLevel;
}

void ScheduleTimer(const NetlistDevice& device, const VoltageList& voltage, double timeSec, double stepSec, uint8_t* pLevel, double* pChangeAtSec)
{
   if (pLevel == nullptr || pChangeAtSec == nullptr)
   {
      return;
   }
   const Timer555Properties* pTimer = std::get_if<Timer555Properties>(&device.properties);
   double delaySec = 0.0;
   if (pTimer != nullptr && pTimer->propagationDelaySec > 0.0)
   {
      delaySec = pTimer->propagationDelaySec;
   }
   uint8_t desired = NextTimerLevel(device, voltage, *pLevel);
   if (desired == *pLevel)
   {
      *pChangeAtSec = TimerNoSchedule;
      return;
   }
   if (*pChangeAtSec < 0.0)
   {
      *pChangeAtSec = timeSec + delaySec;
   }
   if (timeSec + stepSec >= *pChangeAtSec)
   {
      *pLevel = desired;
      *pChangeAtSec = TimerNoSchedule;
   }
}

template <typename ScalarType>
Result StampTimerRails(DenseSystem<ScalarType>* pSystem, const NetlistDevice& device, uint8_t level)
{
   const Timer555Properties* pTimer = std::get_if<Timer555Properties>(&device.properties);
   if (pTimer == nullptr || device.pinCount <= TimerResetPin)
   {
      return Result::InvalidParameter;
   }
   double resistance = pTimer->outputResistanceOhm > 0.0 ? pTimer->outputResistanceOhm : MinimumDriveOhm;
   ScalarType conductance(1.0 / resistance);
   uint32_t rail = level == TimerOutputLow ? device.nets[TimerGroundPin] : device.nets[TimerSupplyPin];
   StampConductance(pSystem, device.nets[TimerOutputPin], rail, conductance);
   if (level == TimerOutputLow)
   {
      StampConductance(pSystem, device.nets[TimerDischargePin], device.nets[TimerGroundPin], conductance);
   }
   return Result::Ok;
}

template <typename ScalarType>
Result StampSwitchArm(DenseSystem<ScalarType>* pSystem, uint32_t netA, uint32_t netB, SwitchState state, ResistanceMode closedMode, ResistanceMode openMode, double closedOhm, double openOhm, uint32_t branchIndex, uint32_t branchUnknown)
{
   if (state == SwitchState::Closed && closedMode == ResistanceMode::Ideal)
   {
      if (branchIndex == NoBranch)
      {
         return Result::InvalidParameter;
      }
      StampVoltageSource(pSystem, netA, netB, branchUnknown, ScalarType(0.0));
      return Result::Ok;
   }
   double resistance = 0.0;
   if (state == SwitchState::Closed)
   {
      resistance = closedOhm;
   }
   else if (openMode == ResistanceMode::Resistive)
   {
      resistance = openOhm;
   }
   else
   {
      return Result::Ok;
   }
   if (IsErr(RequireFinitePositive(resistance)))
   {
      return Result::InvalidParameter;
   }
   StampConductance(pSystem, netA, netB, ScalarType(1.0 / resistance));
   return Result::Ok;
}

template <typename ScalarType>
Result StampSpdt(DenseSystem<ScalarType>* pSystem, const NetlistDevice& device, const SolverDevice& slot, uint32_t totalNets)
{
   const SpdtProperties* pSpdt = std::get_if<SpdtProperties>(&device.properties);
   if (pSpdt == nullptr || device.pinCount < 3)
   {
      return Result::InvalidParameter;
   }
   uint32_t selected = device.nets[1];
   uint32_t other = device.nets[2];
   if (pSpdt->restPosition == ThrowPosition::ThrowB)
   {
      selected = device.nets[2];
      other = device.nets[1];
   }
   uint32_t branchUnknown = 0;
   if (slot.branch != NoBranch)
   {
      branchUnknown = (totalNets - 1) + slot.branch;
   }
   Result closedArm = StampSwitchArm(pSystem, device.nets[0], selected, SwitchState::Closed, pSpdt->closedMode, pSpdt->openMode, pSpdt->closedResistanceOhm, pSpdt->openResistanceOhm, slot.branch, branchUnknown);
   if (IsErr(closedArm))
   {
      return closedArm;
   }
   return StampSwitchArm(pSystem, device.nets[0], other, SwitchState::Open, pSpdt->closedMode, pSpdt->openMode, pSpdt->closedResistanceOhm, pSpdt->openResistanceOhm, NoBranch, 0);
}

template <typename ScalarType>
Result StampDip(DenseSystem<ScalarType>* pSystem, const NetlistDevice& device, const SolverDevice& slot, uint32_t totalNets)
{
   const DipSwitchProperties* pDip = std::get_if<DipSwitchProperties>(&device.properties);
   if (pDip == nullptr || device.pinCount < static_cast<uint8_t>(DipChannelCount * 2))
   {
      return Result::InvalidParameter;
   }
   for (uint8_t channel = 0; channel < DipChannelCount; channel += 1)
   {
      uint32_t branchIndex = slot.extra[channel];
      uint32_t branchUnknown = 0;
      if (branchIndex != NoBranch)
      {
         branchUnknown = (totalNets - 1) + branchIndex;
      }
      Result arm = StampSwitchArm(pSystem, device.nets[channel * 2], device.nets[channel * 2 + 1], pDip->channelState[channel], pDip->closedMode, pDip->openMode, pDip->closedResistanceOhm, pDip->openResistanceOhm, branchIndex, branchUnknown);
      if (IsErr(arm))
      {
         return arm;
      }
   }
   return Result::Ok;
}

Result StampOneDevice(DenseSystem<double>* pSystem, const NetlistDevice& device, const VoltageList& voltage, const VoltageList& history, double inductorCurrent, const AnalysisSettings& settings, double timeSec, bool transient, double stepSec, IntegrationMethod method)
{
   if (device.kind == ComponentKind::Ground || device.kind == ComponentKind::Voltmeter && std::get_if<MeterProperties>(&device.properties) != nullptr && std::get_if<MeterProperties>(&device.properties)->mode == MeterMode::Ideal)
   {
      return Result::Ok;
   }
   if (device.kind == ComponentKind::Resistor)
   {
      const ResistorProperties* pResistor = std::get_if<ResistorProperties>(&device.properties);
      if (pResistor == nullptr)
      {
         return Result::InvalidParameter;
      }
      return StampResistor(pSystem, device.nets[0], device.nets[1], pResistor->resistanceOhm);
   }
   if (device.kind == ComponentKind::Potentiometer)
   {
      const PotentiometerProperties* pPotentiometer = std::get_if<PotentiometerProperties>(&device.properties);
      if (pPotentiometer == nullptr)
      {
         return Result::InvalidParameter;
      }
      double upper = std::max(pPotentiometer->resistanceOhm * (1.0 - pPotentiometer->wiperPosition), pPotentiometer->resistanceOhm * MinimumWiperFraction);
      double lower = std::max(pPotentiometer->resistanceOhm * pPotentiometer->wiperPosition, pPotentiometer->resistanceOhm * MinimumWiperFraction);
      Result upperResult = StampResistor(pSystem, device.nets[0], device.nets[1], upper);
      if (IsErr(upperResult))
      {
         return upperResult;
      }
      return StampResistor(pSystem, device.nets[1], device.nets[2], lower);
   }
   if (device.kind == ComponentKind::Capacitor)
   {
      const CapacitorProperties* pCapacitor = std::get_if<CapacitorProperties>(&device.properties);
      if (pCapacitor == nullptr || IsErr(RequireFinitePositive(pCapacitor->capacitanceFarad)))
      {
         return Result::InvalidParameter;
      }
      if (transient == false)
      {
         return Result::Ok;
      }
      double geq = pCapacitor->capacitanceFarad / stepSec;
      if (method == IntegrationMethod::Trapezoidal)
      {
         geq *= 2.0;
      }
      double previous = 0.0;
      if (device.nets[0] < history.size() && device.nets[1] < history.size())
      {
         previous = history[device.nets[0]] - history[device.nets[1]];
      }
      StampConductance(pSystem, device.nets[0], device.nets[1], geq);
      StampInjection(pSystem, device.nets[0], geq * previous);
      StampInjection(pSystem, device.nets[1], -(geq * previous));
      if (method == IntegrationMethod::Trapezoidal)
      {
         StampInjection(pSystem, device.nets[0], inductorCurrent);
         StampInjection(pSystem, device.nets[1], -inductorCurrent);
      }
      return Result::Ok;
   }
   if (device.kind == ComponentKind::Inductor)
   {
      const InductorProperties* pInductor = std::get_if<InductorProperties>(&device.properties);
      if (pInductor == nullptr || IsErr(RequireFinitePositive(pInductor->inductanceHenry)))
      {
         return Result::InvalidParameter;
      }
      if (transient)
      {
         double geq = stepSec / pInductor->inductanceHenry;
         if (method == IntegrationMethod::Trapezoidal)
         {
            geq *= 2.0;
         }
         StampConductance(pSystem, device.nets[0], device.nets[1], geq);
         StampInjection(pSystem, device.nets[0], -inductorCurrent);
         StampInjection(pSystem, device.nets[1], inductorCurrent);
         if (pInductor->seriesResistanceOhm > 0.0)
         {
            return StampResistor(pSystem, device.nets[0], device.nets[1], pInductor->seriesResistanceOhm);
         }
         return Result::Ok;
      }
      if (pInductor->seriesResistanceOhm > 0.0)
      {
         return StampResistor(pSystem, device.nets[0], device.nets[1], pInductor->seriesResistanceOhm);
      }
      return Result::Ok;
   }
   (void)voltage;
   (void)settings;
   (void)timeSec;
   return Result::Ok;
}

void LimitJunctionAlpha(const VoltageList& previous, const VoltageList& next, uint32_t anodeNet, uint32_t cathodeNet, double thermalVolt, double* pAlpha)
{
   if (pAlpha == nullptr || thermalVolt <= 0.0)
   {
      return;
   }
   double step = VoltageBetween(next, anodeNet, cathodeNet) - VoltageBetween(previous, anodeNet, cathodeNet);
   double limit = JunctionStepFactor * thermalVolt;
   if (step > limit)
   {
      *pAlpha = std::min(*pAlpha, limit / step);
   }
}

void StampDiodeJunction(DenseSystem<double>* pSystem, const DiodeProperties& diode, uint32_t anodeNet, uint32_t cathodeNet, const VoltageList& voltage, double gminSiemens)
{
   double junctionVoltage = VoltageBetween(voltage, anodeNet, cathodeNet);
   DiodeLinearization linear = EvaluateDiode(diode, junctionVoltage);
   double equivalent = linear.currentAmpere - (linear.conductanceSiemens * junctionVoltage);
   StampConductance(pSystem, anodeNet, cathodeNet, linear.conductanceSiemens + gminSiemens);
   StampInjection(pSystem, anodeNet, -equivalent);
   StampInjection(pSystem, cathodeNet, equivalent);
}

bool IsLeadConnected(const Netlist& netlist, size_t deviceIndex, uint32_t net)
{
   for (size_t index = 0; index < netlist.devices.size(); index += 1)
   {
      if (index == deviceIndex)
      {
         continue;
      }
      const NetlistDevice& other = netlist.devices[index];
      for (uint8_t pin = 0; pin < other.pinCount; pin += 1)
      {
         if (other.nets[pin] == net)
         {
            return true;
         }
      }
   }
   return false;
}

Result StampMixedCatalog(DenseSystem<double>* pSystem, const Netlist& netlist, size_t deviceIndex, const SolverDevice& slot, const VoltageList& voltage, double gminSiemens, uint8_t* pRelayOn, uint8_t* pComparatorHigh, uint8_t* pDacBits)
{
   if (pSystem == nullptr || deviceIndex >= netlist.devices.size() || pRelayOn == nullptr || pComparatorHigh == nullptr || pDacBits == nullptr)
   {
      return Result::InvalidParameter;
   }
   const NetlistDevice& device = netlist.devices[deviceIndex];
   if (device.kind == ComponentKind::SevenSegment)
   {
      const SevenSegmentProperties* pDisplay = std::get_if<SevenSegmentProperties>(&device.properties);
      if (pDisplay == nullptr || device.pinCount <= SegmentCommonPin)
      {
         return Result::InvalidParameter;
      }
      if (pDisplay->seriesResistanceOhm < 0.0 || std::isfinite(pDisplay->forwardVoltageVolt) == false)
      {
         return Result::InvalidParameter;
      }
      DiodeProperties junction = {};
      junction.forwardVoltageVolt = pDisplay->forwardVoltageVolt;
      junction.forwardCurrentAmpere = pDisplay->forwardCurrentAmpere;
      junction.emissionCoefficient = DefaultEmissionCoefficient;
      junction.temperatureCelsius = DefaultTemperatureCelsius;
      uint32_t commonNet = device.nets[SegmentCommonPin];
      for (uint8_t segment = 0; segment < SegmentLeadCount; segment += 1)
      {
         uint32_t segmentNet = device.nets[segment];
         if (IsLeadConnected(netlist, deviceIndex, segmentNet) == false)
         {
            continue;
         }
         uint32_t junctionNet = slot.extra[segment];
         if (pDisplay->seriesResistanceOhm > 0.0)
         {
            Result series = StampResistor(pSystem, segmentNet, junctionNet, pDisplay->seriesResistanceOhm);
            if (IsErr(series))
            {
               return series;
            }
         }
         uint32_t anodeNet = junctionNet;
         uint32_t cathodeNet = commonNet;
         if (pDisplay->common == SegmentCommon::CommonAnode)
         {
            anodeNet = commonNet;
            cathodeNet = junctionNet;
         }
         StampDiodeJunction(pSystem, junction, anodeNet, cathodeNet, voltage, gminSiemens);
      }
      return Result::Ok;
   }
   if (device.kind == ComponentKind::Comparator)
   {
      const ComparatorProperties* pComparator = std::get_if<ComparatorProperties>(&device.properties);
      if (pComparator == nullptr || device.pinCount < 5)
      {
         return Result::InvalidParameter;
      }
      double difference = VoltageBetween(voltage, device.nets[0], device.nets[1]);
      if (difference > pComparator->hysteresisVolt)
      {
         *pComparatorHigh = 1;
      }
      else if (difference < -pComparator->hysteresisVolt)
      {
         *pComparatorHigh = 0;
      }
      double resistance = pComparator->outputResistanceOhm > 0.0 ? pComparator->outputResistanceOhm : MinimumDriveOhm;
      uint32_t railNet = *pComparatorHigh == 0 ? device.nets[4] : device.nets[3];
      return StampResistor(pSystem, device.nets[2], railNet, resistance);
   }
   if (device.kind == ComponentKind::Relay)
   {
      const RelayProperties* pRelay = std::get_if<RelayProperties>(&device.properties);
      if (pRelay == nullptr || device.pinCount < 5 || IsErr(RequireFinitePositive(pRelay->coilResistanceOhm)))
      {
         return Result::InvalidParameter;
      }
      Result coil = StampResistor(pSystem, device.nets[0], device.nets[1], pRelay->coilResistanceOhm);
      if (IsErr(coil))
      {
         return coil;
      }
      double coilCurrent = VoltageBetween(voltage, device.nets[0], device.nets[1]) / pRelay->coilResistanceOhm;
      double magnitude = std::abs(coilCurrent);
      if (magnitude >= pRelay->pullInCurrentAmpere)
      {
         *pRelayOn = 1;
      }
      else if (magnitude <= pRelay->dropOutCurrentAmpere)
      {
         *pRelayOn = 0;
      }
      uint32_t throwNet = *pRelayOn == 0 ? device.nets[4] : device.nets[3];
      return StampResistor(pSystem, device.nets[2], throwNet, DefaultClosedResistanceOhm);
   }
   if (device.kind == ComponentKind::Dac)
   {
      const DacProperties* pDac = std::get_if<DacProperties>(&device.properties);
      if (pDac == nullptr || device.pinCount <= DacOutputPin)
      {
         return Result::InvalidParameter;
      }
      uint8_t code = 0;
      for (uint8_t bit = 0; bit < DacBitCount; bit += 1)
      {
         uint32_t net = device.nets[bit];
         double pinVoltage = net < voltage.size() ? voltage[net] : 0.0;
         bool bitHigh = (*pDacBits & static_cast<uint8_t>(1u << bit)) != 0;
         if (pinVoltage >= pDac->logic.vihVolt)
         {
            bitHigh = true;
         }
         else if (pinVoltage <= pDac->logic.vilVolt)
         {
            bitHigh = false;
         }
         if (bitHigh)
         {
            code = static_cast<uint8_t>(code | static_cast<uint8_t>(1u << bit));
         }
      }
      *pDacBits = code;
      uint32_t referenceNet = device.nets[DacReferencePin];
      double reference = referenceNet < voltage.size() ? voltage[referenceNet] : 0.0;
      double outputVolt = (static_cast<double>(code) / DacFullScaleCode) * reference;
      double resistance = pDac->logic.outputResistanceOhm > 0.0 ? pDac->logic.outputResistanceOhm : MinimumDriveOhm;
      uint32_t outputNet = device.nets[DacOutputPin];
      StampConductance(pSystem, outputNet, GroundNetIndex, 1.0 / resistance);
      StampInjection(pSystem, outputNet, outputVolt / resistance);
      return Result::Ok;
   }
   return Result::Ok;
}

} // namespace

enum class DriveStamp : int8_t
{
   DcNorton = 0
};

Result StampLogicDrives(DenseSystem<double>* pSystem, const LogicDriveList* pDrives, DriveStamp stamp, double sourceScale)
{
   if (pSystem == nullptr)
   {
      return Result::InvalidParameter;
   }
   if (pDrives == nullptr)
   {
      return Result::Ok;
   }
   for (const LogicDrive& drive : *pDrives)
   {
      if (std::isfinite(drive.resistanceOhm) == false || drive.resistanceOhm <= 0.0)
      {
         return Result::InvalidParameter;
      }
      if (std::isfinite(drive.voltageVolt) == false)
      {
         return Result::InvalidParameter;
      }
      StampConductance(pSystem, drive.netIndex, GroundNetIndex, 1.0 / drive.resistanceOhm);
      if (stamp == DriveStamp::DcNorton)
      {
         StampInjection(pSystem, drive.netIndex, sourceScale * drive.voltageVolt / drive.resistanceOhm);
      }
   }
   return Result::Ok;
}

DcResult SolveDcAnalog(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel, const LogicDriveList* pDrives)
{
   DcResult result = {};
   result.code = Result::Ok;
   result.netVoltage.assign(netlist.netCount, 0.0);
   result.netLevel.assign(netlist.netCount, LogicLevel::HighZ);
   if (netlist.hasAnalog == false)
   {
      return result;
   }
   PreparedCircuit prepared = Prepare(netlist, true, SwitchSchedule::OperatingPoint, 0.0);
   if (IsErr(prepared.code))
   {
      result.code = prepared.code;
      result.message = prepared.message.empty() ? Describe(prepared.code) : prepared.message;
      return result;
   }
   uint32_t unknownCount = UnknownCountOf(prepared);
   VoltageList voltage(prepared.totalNets, 0.0);
   VoltageList proposed(prepared.totalNets, 0.0);
   std::vector<uint8_t> relayEnergized(netlist.devices.size(), 0);
   std::vector<uint8_t> comparatorHigh(netlist.devices.size(), 0);
   std::vector<uint8_t> dacBits(netlist.devices.size(), 0);
   std::vector<uint8_t> timerLevel(netlist.devices.size(), TimerOutputLow);
   std::vector<double> branchCurrents(prepared.branchCount, 0.0);
   bool converged = false;
   bool freezeTimers = false;
   constexpr uint32_t SourceLadderCount = 6;
   using SourceLadder = std::array<double, SourceLadderCount>;
   constexpr SourceLadder SourceScales{1.0, 0.0625, 0.125, 0.25, 0.5, 1.0};
   for (uint32_t sourcePass = 0; sourcePass < SourceLadderCount && converged == false; sourcePass += 1)
   {
   const double sourceScale = SourceScales[sourcePass];
   if (sourcePass == 1)
   {
      voltage.assign(voltage.size(), 0.0);
      std::fill(branchCurrents.begin(), branchCurrents.end(), 0.0);
      std::fill(timerLevel.begin(), timerLevel.end(), TimerOutputLow);
   }
   freezeTimers = false;
   bool solvedThisPass = false;
   bool passConverged = false;
   for (uint32_t iteration = 0; iteration < MaxNewtonIterations + TimerSettleExtraIterations; iteration += 1)
   {
      if (pCancel != nullptr && pCancel->IsRequested())
      {
         result.code = Result::Error;
         result.message = "Cancelled.";
         return result;
      }
      if (iteration == MaxNewtonIterations)
      {
         freezeTimers = true;
         std::fill(timerLevel.begin(), timerLevel.end(), TimerOutputLow);
      }
      bool timerChanged = false;
      if (freezeTimers == false)
      {
         for (size_t index = 0; index < netlist.devices.size(); index += 1)
         {
            const NetlistDevice& device = netlist.devices[index];
            if (device.kind != ComponentKind::Timer555)
            {
               continue;
            }
            uint8_t nextLevel = NextTimerLevel(device, voltage, timerLevel[index]);
            if (nextLevel != timerLevel[index])
            {
               timerLevel[index] = nextLevel;
               timerChanged = true;
            }
         }
      }
      DenseSystem<double> system;
      system.Reset(unknownCount);
      StampGmin(&system, prepared.totalNets, settings.gminSiemens);
      for (size_t index = 0; index < netlist.devices.size(); index += 1)
      {
         const NetlistDevice& device = netlist.devices[index];
         const SolverDevice& slot = prepared.devices[index];
         if (device.kind == ComponentKind::VoltageSource)
         {
            const VoltageSourceProperties* pSource = std::get_if<VoltageSourceProperties>(&device.properties);
            if (pSource == nullptr || slot.branch == NoBranch)
            {
               result.code = Result::InvalidParameter;
               result.message = device.name + " is not a voltage source.";
               return result;
            }
            uint32_t branchUnknown = (prepared.totalNets - 1) + slot.branch;
            StampVoltageSource(&system, device.nets[1], device.nets[0], branchUnknown, pSource->dcVoltageVolt * sourceScale);
         }
         else if (device.kind == ComponentKind::CurrentSource)
         {
            const CurrentSourceProperties* pSource = std::get_if<CurrentSourceProperties>(&device.properties);
            if (pSource == nullptr)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            StampInjection(&system, device.nets[0], -pSource->dcCurrentAmpere * sourceScale);
            StampInjection(&system, device.nets[1], pSource->dcCurrentAmpere * sourceScale);
         }
         else if (device.kind == ComponentKind::Resistor || device.kind == ComponentKind::Potentiometer || device.kind == ComponentKind::Inductor || device.kind == ComponentKind::Capacitor || device.kind == ComponentKind::Ground || device.kind == ComponentKind::Voltmeter)
         {
            VoltageList empty;
            Result stamped = StampOneDevice(&system, device, voltage, empty, 0.0, settings, 0.0, false, 1.0, IntegrationMethod::BackwardEuler);
            if (device.kind == ComponentKind::Inductor && slot.branch != NoBranch)
            {
               uint32_t branchUnknown = (prepared.totalNets - 1) + slot.branch;
               StampVoltageSource(&system, device.nets[0], device.nets[1], branchUnknown, 0.0);
            }
            if (device.kind == ComponentKind::Voltmeter)
            {
               const MeterProperties* pMeter = std::get_if<MeterProperties>(&device.properties);
               if (pMeter != nullptr && pMeter->mode == MeterMode::NonIdeal)
               {
                  Result meterStamp = StampResistor(&system, device.nets[0], device.nets[1], pMeter->inputResistanceOhm);
                  if (IsErr(meterStamp))
                  {
                     result.code = meterStamp;
                     result.message = device.name + " has an invalid input resistance.";
                     return result;
                  }
               }
            }
            if (IsErr(stamped))
            {
               result.code = stamped;
               result.message = device.name + " has an invalid value.";
               return result;
            }
         }
         else if (device.kind == ComponentKind::Ammeter)
         {
            const MeterProperties* pMeter = std::get_if<MeterProperties>(&device.properties);
            if (pMeter == nullptr)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            if (slot.branch != NoBranch)
            {
               uint32_t branchUnknown = (prepared.totalNets - 1) + slot.branch;
               StampVoltageSource(&system, device.nets[1], device.nets[0], branchUnknown, 0.0);
            }
            else
            {
               Result stamped = StampResistor(&system, device.nets[0], device.nets[1], pMeter->burdenResistanceOhm);
               if (IsErr(stamped))
               {
                  result.code = stamped;
                  result.message = device.name + " has an invalid burden.";
                  return result;
               }
            }
         }
         else if (device.kind == ComponentKind::SpstSwitch || device.kind == ComponentKind::Pushbutton)
         {
            bool closed = false;
            ResistanceMode closedMode = ResistanceMode::Ideal;
            ResistanceMode openMode = ResistanceMode::Ideal;
            double closedResistance = 0.0;
            double openResistance = 0.0;
            if (device.kind == ComponentKind::SpstSwitch)
            {
               const SpstProperties* pSwitch = std::get_if<SpstProperties>(&device.properties);
               if (pSwitch == nullptr)
               {
                  result.code = Result::InvalidParameter;
                  return result;
               }
               closed = SwitchClosedAt(*pSwitch, 0.0, false);
               closedMode = pSwitch->closedMode;
               openMode = pSwitch->openMode;
               closedResistance = pSwitch->closedResistanceOhm;
               openResistance = pSwitch->openResistanceOhm;
            }
            else
            {
               const PushbuttonProperties* pButton = std::get_if<PushbuttonProperties>(&device.properties);
               if (pButton == nullptr)
               {
                  result.code = Result::InvalidParameter;
                  return result;
               }
               closed = PushbuttonClosedAt(*pButton, 0.0, false);
               closedMode = pButton->closedMode;
               openMode = pButton->openMode;
               closedResistance = pButton->closedResistanceOhm;
               openResistance = pButton->openResistanceOhm;
            }
            if (closed && closedMode == ResistanceMode::Ideal && slot.branch != NoBranch)
            {
               uint32_t branchUnknown = (prepared.totalNets - 1) + slot.branch;
               StampVoltageSource(&system, device.nets[0], device.nets[1], branchUnknown, 0.0);
            }
            else if (closed && closedMode == ResistanceMode::Resistive)
            {
               Result stamped = StampResistor(&system, device.nets[0], device.nets[1], closedResistance);
               if (IsErr(stamped))
               {
                  result.code = stamped;
                  return result;
               }
            }
            else if (closed == false && openMode == ResistanceMode::Resistive)
            {
               Result stamped = StampResistor(&system, device.nets[0], device.nets[1], openResistance);
               if (IsErr(stamped))
               {
                  result.code = stamped;
                  return result;
               }
            }
         }
         else if (device.kind == ComponentKind::SpdtSwitch)
         {
            Result stamped = StampSpdt(&system, device, slot, prepared.totalNets);
            if (IsErr(stamped))
            {
               result.code = stamped;
               result.message = device.name + " has an invalid contact.";
               return result;
            }
         }
         else if (device.kind == ComponentKind::DipSwitch)
         {
            Result stamped = StampDip(&system, device, slot, prepared.totalNets);
            if (IsErr(stamped))
            {
               result.code = stamped;
               result.message = device.name + " has an invalid contact.";
               return result;
            }
         }
         else if (device.kind == ComponentKind::Timer555)
         {
            Result stamped = StampTimerRails(&system, device, timerLevel[index]);
            if (IsErr(stamped))
            {
               result.code = stamped;
               result.message = device.name + " has an invalid output resistance.";
               return result;
            }
         }
         else if (device.kind == ComponentKind::Diode || device.kind == ComponentKind::Led)
         {
            const DiodeProperties* pDiode = nullptr;
            DiodeProperties copy = {};
            if (device.kind == ComponentKind::Led)
            {
               const LedProperties* pLed = std::get_if<LedProperties>(&device.properties);
               if (pLed == nullptr)
               {
                  result.code = Result::InvalidParameter;
                  return result;
               }
               copy = pLed->diode;
               pDiode = &copy;
            }
            else
            {
               pDiode = std::get_if<DiodeProperties>(&device.properties);
            }
            if (pDiode == nullptr)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            uint32_t anode = device.nets[0];
            uint32_t junction = slot.extra[0];
            uint32_t cathode = device.nets[1];
            if (pDiode->seriesResistanceOhm > 0.0)
            {
               Result series = StampResistor(&system, anode, junction, pDiode->seriesResistanceOhm);
               if (IsErr(series))
               {
                  result.code = series;
                  return result;
               }
            }
            double junctionVoltage = VoltageBetween(voltage, junction, cathode);
            double thermal = pDiode->emissionCoefficient * ThermalVoltage(pDiode->temperatureCelsius);
            double limited = LimitJunctionStep(junctionVoltage, junctionVoltage, thermal * JunctionStepFactor);
            (void)limited;
            DiodeLinearization linear = EvaluateDiode(*pDiode, junctionVoltage);
            double equivalent = linear.currentAmpere - (linear.conductanceSiemens * junctionVoltage);
            StampConductance(&system, junction, cathode, linear.conductanceSiemens + settings.gminSiemens);
            StampInjection(&system, junction, -equivalent);
            StampInjection(&system, cathode, equivalent);
         }
         else if (device.kind == ComponentKind::Npn || device.kind == ComponentKind::Pnp)
         {
            const BjtProperties* pBjt = std::get_if<BjtProperties>(&device.properties);
            if (pBjt == nullptr)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            double base = voltage[device.nets[1]];
            double collector = voltage[device.nets[0]];
            double emitter = voltage[device.nets[2]];
            BjtLinearization linear = EvaluateBjt(*pBjt, base - emitter, base - collector);
            double vbe = base - emitter;
            double vbc = base - collector;
            double baseEquivalent = linear.baseCurrent - (linear.baseEmitterConductance * vbe) - (linear.baseCollectorConductance * vbc);
            double collectorEquivalent = linear.collectorCurrent - (linear.collectorEmitterTransconductance * vbe) - (linear.collectorCollectorConductance * vbc);
            StampConductance(&system, device.nets[1], device.nets[2], linear.baseEmitterConductance);
            StampConductance(&system, device.nets[1], device.nets[0], linear.baseCollectorConductance);
            AddControlled(&system, device.nets[0], device.nets[1], device.nets[2], linear.collectorEmitterTransconductance);
            AddControlled(&system, device.nets[0], device.nets[1], device.nets[0], linear.collectorCollectorConductance);
            AddControlled(&system, device.nets[2], device.nets[1], device.nets[2], -linear.collectorEmitterTransconductance);
            AddControlled(&system, device.nets[2], device.nets[1], device.nets[0], -linear.collectorCollectorConductance);
            StampInjection(&system, device.nets[1], -baseEquivalent);
            StampInjection(&system, device.nets[0], -collectorEquivalent);
            StampInjection(&system, device.nets[2], baseEquivalent + collectorEquivalent);
         }
         else if (device.kind == ComponentKind::Nmos || device.kind == ComponentKind::Pmos)
         {
            const MosfetProperties* pMosfet = std::get_if<MosfetProperties>(&device.properties);
            if (pMosfet == nullptr)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            double gate = voltage[device.nets[1]];
            double drain = voltage[device.nets[0]];
            double source = voltage[device.nets[2]];
            MosfetLinearization linear = EvaluateMosfet(device.kind, *pMosfet, gate - source, drain - source);
            double vgs = gate - source;
            double vds = drain - source;
            double equivalent = linear.drainCurrent - (linear.gateSourceTransconductance * vgs) - (linear.drainSourceConductance * vds);
            AddGain(&system, device.nets[0], device.nets[1], linear.gateSourceTransconductance);
            AddGain(&system, device.nets[0], device.nets[2], -linear.gateSourceTransconductance - linear.drainSourceConductance);
            AddGain(&system, device.nets[0], device.nets[0], linear.drainSourceConductance);
            AddGain(&system, device.nets[2], device.nets[1], -linear.gateSourceTransconductance);
            AddGain(&system, device.nets[2], device.nets[2], linear.gateSourceTransconductance + linear.drainSourceConductance);
            AddGain(&system, device.nets[2], device.nets[0], -linear.drainSourceConductance);
            StampInjection(&system, device.nets[0], -equivalent);
            StampInjection(&system, device.nets[2], equivalent);
         }
         else if (device.kind == ComponentKind::SevenSegment || device.kind == ComponentKind::Comparator || device.kind == ComponentKind::Relay || device.kind == ComponentKind::Dac)
         {
            Result stamped = StampMixedCatalog(&system, netlist, index, slot, voltage, settings.gminSiemens, &relayEnergized[index], &comparatorHigh[index], &dacBits[index]);
            if (IsErr(stamped))
            {
               result.code = stamped;
               result.message = device.name + " has an invalid value.";
               return result;
            }
         }
      }
      Result drivesStamped = StampLogicDrives(&system, pDrives, DriveStamp::DcNorton, sourceScale);
      if (IsErr(drivesStamped))
      {
         result.code = drivesStamped;
         result.message = Describe(drivesStamped);
         return result;
      }
      if (unknownCount == 0)
      {
         passConverged = true;
         break;
      }
      Result solved = system.FactorizeAndSolve();
      if (IsErr(solved))
      {
         if (solvedThisPass == false && sourcePass + 1 == SourceLadderCount)
         {
            result.code = solved;
            result.message = Describe(solved);
            return result;
         }
         break;
      }
      solvedThisPass = true;
      for (uint32_t net = 1; net < prepared.totalNets; net += 1)
      {
         proposed[net] = system.Solution(net - 1);
      }
      for (uint32_t branch = 0; branch < prepared.branchCount; branch += 1)
      {
         branchCurrents[branch] = system.Solution((prepared.totalNets - 1) + branch);
      }
      double alpha = 1.0;
      for (size_t index = 0; index < netlist.devices.size(); index += 1)
      {
         const NetlistDevice& device = netlist.devices[index];
         if (device.kind == ComponentKind::Diode || device.kind == ComponentKind::Led)
         {
            const DiodeProperties* pDiode = std::get_if<DiodeProperties>(&device.properties);
            if (device.kind == ComponentKind::Led)
            {
               const LedProperties* pLed = std::get_if<LedProperties>(&device.properties);
               pDiode = pLed == nullptr ? nullptr : &pLed->diode;
            }
            if (pDiode == nullptr)
            {
               continue;
            }
            uint32_t junction = prepared.devices[index].extra[0];
            double thermal = pDiode->emissionCoefficient * ThermalVoltage(pDiode->temperatureCelsius);
            LimitJunctionAlpha(voltage, proposed, junction, device.nets[1], thermal, &alpha);
         }
         else if (device.kind == ComponentKind::SevenSegment)
         {
            const SevenSegmentProperties* pDisplay = std::get_if<SevenSegmentProperties>(&device.properties);
            if (pDisplay == nullptr || device.pinCount <= SegmentCommonPin)
            {
               continue;
            }
            double thermal = DefaultEmissionCoefficient * ThermalVoltage(DefaultTemperatureCelsius);
            uint32_t commonNet = device.nets[SegmentCommonPin];
            for (uint8_t segment = 0; segment < SegmentLeadCount; segment += 1)
            {
               if (IsLeadConnected(netlist, index, device.nets[segment]) == false)
               {
                  continue;
               }
               uint32_t junctionNet = prepared.devices[index].extra[segment];
               uint32_t anodeNet = junctionNet;
               uint32_t cathodeNet = commonNet;
               if (pDisplay->common == SegmentCommon::CommonAnode)
               {
                  anodeNet = commonNet;
                  cathodeNet = junctionNet;
               }
               LimitJunctionAlpha(voltage, proposed, anodeNet, cathodeNet, thermal, &alpha);
            }
         }
         else if (device.kind == ComponentKind::Npn || device.kind == ComponentKind::Pnp)
         {
            const BjtProperties* pBjt = std::get_if<BjtProperties>(&device.properties);
            if (pBjt == nullptr || device.pinCount < 3)
            {
               continue;
            }
            double thermal = pBjt->emissionCoefficient * ThermalVoltage(pBjt->temperatureCelsius);
            LimitJunctionAlpha(voltage, proposed, device.nets[1], device.nets[2], thermal, &alpha);
            LimitJunctionAlpha(voltage, proposed, device.nets[1], device.nets[0], thermal, &alpha);
         }
      }
      double maxChange = 0.0;
      for (uint32_t net = 1; net < prepared.totalNets; net += 1)
      {
         double updated = voltage[net] + (alpha * (proposed[net] - voltage[net]));
         maxChange = std::max(maxChange, std::abs(updated - voltage[net]));
         voltage[net] = updated;
      }
      if (timerChanged == false && alpha > 0.999 && maxChange <= (VoltageAbsoluteTolerance + VoltageRelativeTolerance))
      {
         passConverged = true;
         break;
      }
   }
   if (passConverged && sourceScale == 1.0)
   {
      converged = true;
   }
   if (passConverged == false && sourcePass > 0)
   {
      break;
   }
   }
   if (converged == false)
   {
      result.code = Result::NoConvergence;
      result.message = Describe(Result::NoConvergence);
      return result;
   }
   result.operatingVoltage = voltage;
   result.netVoltage.assign(netlist.netCount, 0.0);
   for (uint32_t net = 0; net < netlist.netCount && net < voltage.size(); net += 1)
   {
      result.netVoltage[net] = voltage[net];
      NodeValue node = {};
      node.netIndex = net;
      node.name = net < netlist.netNames.size() ? netlist.netNames[net] : std::to_string(net);
      node.value = voltage[net];
      node.level = LogicLevel::HighZ;
      result.nodes.push_back(node);
   }
   for (size_t index = 0; index < netlist.devices.size(); index += 1)
   {
      const NetlistDevice& device = netlist.devices[index];
      if (KindIsAnalog(device.kind) == false || device.kind == ComponentKind::Ground)
      {
         continue;
      }
      BranchValue branch = {};
      branch.componentId = device.componentId;
      branch.name = device.name;
      branch.level = LogicLevel::HighZ;
      if (device.pinCount >= 2)
      {
         branch.voltageVolt = VoltageBetween(result.netVoltage, device.nets[0], device.nets[1]);
      }
      if (device.kind == ComponentKind::Resistor)
      {
         const ResistorProperties* pResistor = std::get_if<ResistorProperties>(&device.properties);
         if (pResistor != nullptr && pResistor->resistanceOhm > 0.0)
         {
            branch.currentAmpere = branch.voltageVolt / pResistor->resistanceOhm;
            branch.powerWatt = branch.currentAmpere * branch.voltageVolt;
         }
      }
      else if (prepared.devices[index].branch != NoBranch && prepared.devices[index].branch < branchCurrents.size())
      {
         branch.currentAmpere = branchCurrents[prepared.devices[index].branch];
         if (device.kind == ComponentKind::VoltageSource)
         {
            const VoltageSourceProperties* pSource = std::get_if<VoltageSourceProperties>(&device.properties);
            if (pSource != nullptr)
            {
               branch.voltageVolt = pSource->dcVoltageVolt;
               branch.powerWatt = branch.voltageVolt * branch.currentAmpere;
            }
         }
      }
      result.branches.push_back(branch);
   }
   result.deviceState = timerLevel;
   result.warnings = netlist.warnings;
   result.message = Describe(Result::Ok);
   return result;
}

namespace
{

double BiasAt(const DcResult& bias, uint32_t net)
{
   if (net < bias.operatingVoltage.size())
   {
      return bias.operatingVoltage[net];
   }
   if (net < bias.netVoltage.size())
   {
      return bias.netVoltage[net];
   }
   return 0.0;
}

const DiodeProperties* DiodeOf(const NetlistDevice& device, DiodeProperties* pCopy)
{
   if (device.kind == ComponentKind::Led)
   {
      const LedProperties* pLed = std::get_if<LedProperties>(&device.properties);
      if (pLed == nullptr || pCopy == nullptr)
      {
         return nullptr;
      }
      *pCopy = pLed->diode;
      return pCopy;
   }
   return std::get_if<DiodeProperties>(&device.properties);
}

Result StampAcSemiconductors(DenseSystem<ComplexValue>* pSystem, const Netlist& netlist, const PreparedCircuit& prepared, const DcResult& bias, double omega, double gminSiemens)
{
   if (pSystem == nullptr)
   {
      return Result::InvalidParameter;
   }
   ComplexValue imaginary(0.0, omega);
   for (size_t index = 0; index < netlist.devices.size() && index < prepared.devices.size(); index += 1)
   {
      const NetlistDevice& device = netlist.devices[index];
      const SolverDevice& slot = prepared.devices[index];
      if (device.kind == ComponentKind::Diode || device.kind == ComponentKind::Led)
      {
         DiodeProperties copy = {};
         const DiodeProperties* pDiode = DiodeOf(device, &copy);
         if (pDiode == nullptr || device.pinCount < 2)
         {
            return Result::InvalidParameter;
         }
         uint32_t junction = slot.extra[0];
         uint32_t cathode = device.nets[1];
         if (pDiode->seriesResistanceOhm > 0.0)
         {
            StampConductance(pSystem, device.nets[0], junction, ComplexValue(1.0 / pDiode->seriesResistanceOhm, 0.0));
         }
         double junctionVoltage = BiasAt(bias, junction) - BiasAt(bias, cathode);
         DiodeLinearization linear = EvaluateDiode(*pDiode, junctionVoltage);
         double capacitance = JunctionCapacitance(pDiode->junctionCapacitanceFarad, pDiode->junctionPotentialVolt, pDiode->gradingCoefficient, junctionVoltage);
         capacitance += pDiode->transitTimeSec * linear.conductanceSiemens;
         ComplexValue admittance(linear.conductanceSiemens + gminSiemens, 0.0);
         admittance += imaginary * capacitance;
         StampConductance(pSystem, junction, cathode, admittance);
      }
      else if (device.kind == ComponentKind::Npn || device.kind == ComponentKind::Pnp)
      {
         const BjtProperties* pBjt = std::get_if<BjtProperties>(&device.properties);
         if (pBjt == nullptr || device.pinCount < 3)
         {
            return Result::InvalidParameter;
         }
         double base = BiasAt(bias, device.nets[1]);
         double collector = BiasAt(bias, device.nets[0]);
         double emitter = BiasAt(bias, device.nets[2]);
         double baseEmitter = base - emitter;
         double baseCollector = base - collector;
         BjtLinearization linear = EvaluateBjt(*pBjt, baseEmitter, baseCollector);
         double emitterCapacitance = JunctionCapacitance(pBjt->cjeFarad, DefaultJunctionPotentialVolt, DefaultGradingCoefficient, baseEmitter);
         emitterCapacitance += pBjt->forwardTransitTimeSec * linear.collectorEmitterTransconductance;
         double collectorCapacitance = JunctionCapacitance(pBjt->cjcFarad, DefaultJunctionPotentialVolt, DefaultGradingCoefficient, baseCollector);
         collectorCapacitance += pBjt->reverseTransitTimeSec * linear.baseCollectorConductance * pBjt->reverseBeta;
         StampConductance(pSystem, device.nets[1], device.nets[2], ComplexValue(linear.baseEmitterConductance, 0.0) + (imaginary * emitterCapacitance));
         StampConductance(pSystem, device.nets[1], device.nets[0], ComplexValue(linear.baseCollectorConductance, 0.0) + (imaginary * collectorCapacitance));
         AddControlled(pSystem, device.nets[0], device.nets[1], device.nets[2], ComplexValue(linear.collectorEmitterTransconductance, 0.0));
         AddControlled(pSystem, device.nets[0], device.nets[1], device.nets[0], ComplexValue(linear.collectorCollectorConductance, 0.0));
         AddControlled(pSystem, device.nets[2], device.nets[1], device.nets[2], ComplexValue(-linear.collectorEmitterTransconductance, 0.0));
         AddControlled(pSystem, device.nets[2], device.nets[1], device.nets[0], ComplexValue(-linear.collectorCollectorConductance, 0.0));
      }
      else if (device.kind == ComponentKind::Nmos || device.kind == ComponentKind::Pmos)
      {
         const MosfetProperties* pMosfet = std::get_if<MosfetProperties>(&device.properties);
         if (pMosfet == nullptr || device.pinCount < 3)
         {
            return Result::InvalidParameter;
         }
         double gate = BiasAt(bias, device.nets[1]);
         double drain = BiasAt(bias, device.nets[0]);
         double source = BiasAt(bias, device.nets[2]);
         MosfetLinearization linear = EvaluateMosfet(device.kind, *pMosfet, gate - source, drain - source);
         AddGain(pSystem, device.nets[0], device.nets[1], ComplexValue(linear.gateSourceTransconductance, 0.0));
         AddGain(pSystem, device.nets[0], device.nets[2], ComplexValue(-linear.gateSourceTransconductance - linear.drainSourceConductance, 0.0));
         AddGain(pSystem, device.nets[0], device.nets[0], ComplexValue(linear.drainSourceConductance, 0.0));
         AddGain(pSystem, device.nets[2], device.nets[1], ComplexValue(-linear.gateSourceTransconductance, 0.0));
         AddGain(pSystem, device.nets[2], device.nets[2], ComplexValue(linear.gateSourceTransconductance + linear.drainSourceConductance, 0.0));
         AddGain(pSystem, device.nets[2], device.nets[0], ComplexValue(-linear.drainSourceConductance, 0.0));
         double gateSourceCapacitance = pMosfet->cgsoFaradPerMeter * pMosfet->channelWidthMeter;
         double gateDrainCapacitance = pMosfet->cgdoFaradPerMeter * pMosfet->channelWidthMeter;
         if (gateSourceCapacitance > 0.0)
         {
            StampConductance(pSystem, device.nets[1], device.nets[2], imaginary * gateSourceCapacitance);
         }
         if (gateDrainCapacitance > 0.0)
         {
            StampConductance(pSystem, device.nets[1], device.nets[0], imaginary * gateDrainCapacitance);
         }
      }
   }
   return Result::Ok;
}

} // namespace

AcResult SolveAcAnalog(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel, const LogicDriveList* pDrives, const DcResult* pBias)
{
   AcResult result = {};
   if (pBias != nullptr)
   {
      result.bias = *pBias;
   }
   else
   {
      result.bias = SolveDcAnalog(netlist, settings, pCancel, pDrives);
   }
   if (IsErr(result.bias.code))
   {
      result.code = result.bias.code;
      result.message = result.bias.message;
      return result;
   }
   if (settings.sweepStartHz <= 0.0 || settings.sweepStopHz <= 0.0 || settings.sweepStartHz > settings.sweepStopHz)
   {
      result.code = Result::InvalidSweep;
      result.message = Describe(Result::InvalidSweep);
      return result;
   }
   std::vector<double> frequencies;
   if (settings.sweepStartHz == settings.sweepStopHz)
   {
      frequencies.push_back(settings.sweepStartHz);
   }
   else if (settings.sweepScale == SweepScale::Logarithmic)
   {
      if (settings.pointsPerDecade < 1)
      {
         result.code = Result::InvalidSweep;
         result.message = Describe(Result::InvalidSweep);
         return result;
      }
      double decades = std::log10(settings.sweepStopHz / settings.sweepStartHz);
      int32_t steps = static_cast<int32_t>(std::ceil(decades * static_cast<double>(settings.pointsPerDecade)));
      if (steps < 1)
      {
         steps = 1;
      }
      for (int32_t step = 0; step <= steps; step += 1)
      {
         double fraction = static_cast<double>(step) / static_cast<double>(steps);
         frequencies.push_back(settings.sweepStartHz * std::pow(10.0, decades * fraction));
      }
   }
   else
   {
      if (settings.linearPointCount < 2)
      {
         result.code = Result::InvalidSweep;
         result.message = Describe(Result::InvalidSweep);
         return result;
      }
      for (int32_t step = 0; step < settings.linearPointCount; step += 1)
      {
         double fraction = static_cast<double>(step) / static_cast<double>(settings.linearPointCount - 1);
         frequencies.push_back(settings.sweepStartHz + (fraction * (settings.sweepStopHz - settings.sweepStartHz)));
      }
   }
   PreparedCircuit prepared = Prepare(netlist, false, SwitchSchedule::OperatingPoint, 0.0);
   if (IsErr(prepared.code))
   {
      result.code = prepared.code;
      result.message = prepared.message;
      return result;
   }
   uint32_t unknownCount = UnknownCountOf(prepared);
   for (double frequency : frequencies)
   {
      if (pCancel != nullptr && pCancel->IsRequested())
      {
         result.code = Result::Error;
         result.message = "Cancelled.";
         return result;
      }
      DenseSystem<ComplexValue> system;
      system.Reset(unknownCount);
      double omega = 2.0 * std::numbers::pi * frequency;
      for (uint32_t net = 1; net < prepared.totalNets; net += 1)
      {
         StampConductance(&system, net, GroundNetIndex, ComplexValue(settings.gminSiemens, 0.0));
      }
      for (size_t index = 0; index < netlist.devices.size(); index += 1)
      {
         const NetlistDevice& device = netlist.devices[index];
         const SolverDevice& slot = prepared.devices[index];
         if (device.kind == ComponentKind::Resistor)
         {
            const ResistorProperties* pResistor = std::get_if<ResistorProperties>(&device.properties);
            if (pResistor == nullptr || pResistor->resistanceOhm <= 0.0)
            {
               result.code = Result::InvalidParameter;
               result.message = device.name + " has an invalid resistance.";
               return result;
            }
            StampConductance(&system, device.nets[0], device.nets[1], ComplexValue(1.0 / pResistor->resistanceOhm, 0.0));
         }
         else if (device.kind == ComponentKind::Capacitor)
         {
            const CapacitorProperties* pCapacitor = std::get_if<CapacitorProperties>(&device.properties);
            if (pCapacitor == nullptr || pCapacitor->capacitanceFarad <= 0.0)
            {
               result.code = Result::InvalidParameter;
               result.message = device.name + " has an invalid capacitance.";
               return result;
            }
            ComplexValue admittance(0.0, omega * pCapacitor->capacitanceFarad);
            if (pCapacitor->esrOhm > 0.0)
            {
               admittance = ComplexValue(1.0, 0.0) / (ComplexValue(pCapacitor->esrOhm, 0.0) + (ComplexValue(1.0, 0.0) / admittance));
            }
            StampConductance(&system, device.nets[0], device.nets[1], admittance);
         }
         else if (device.kind == ComponentKind::Inductor)
         {
            const InductorProperties* pInductor = std::get_if<InductorProperties>(&device.properties);
            if (pInductor == nullptr || pInductor->inductanceHenry <= 0.0)
            {
               result.code = Result::InvalidParameter;
               result.message = device.name + " has an invalid inductance.";
               return result;
            }
            ComplexValue admittance = ComplexValue(1.0, 0.0) / ComplexValue(pInductor->seriesResistanceOhm, omega * pInductor->inductanceHenry);
            StampConductance(&system, device.nets[0], device.nets[1], admittance);
         }
         else if (device.kind == ComponentKind::VoltageSource)
         {
            const VoltageSourceProperties* pSource = std::get_if<VoltageSourceProperties>(&device.properties);
            if (pSource == nullptr || slot.branch == NoBranch)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            double radians = pSource->acPhaseDeg * std::numbers::pi / 180.0;
            ComplexValue phasor = std::polar(pSource->acAmplitudeVolt, radians);
            uint32_t branchUnknown = (prepared.totalNets - 1) + slot.branch;
            StampVoltageSource(&system, device.nets[1], device.nets[0], branchUnknown, phasor);
         }
         else if (device.kind == ComponentKind::CurrentSource)
         {
            const CurrentSourceProperties* pSource = std::get_if<CurrentSourceProperties>(&device.properties);
            if (pSource == nullptr)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            double radians = pSource->acPhaseDeg * std::numbers::pi / 180.0;
            ComplexValue phasor = std::polar(pSource->acAmplitudeAmpere, radians);
            StampInjection(&system, device.nets[0], -phasor);
            StampInjection(&system, device.nets[1], phasor);
         }
         else if (device.kind == ComponentKind::Ammeter && slot.branch != NoBranch)
         {
            uint32_t branchUnknown = (prepared.totalNets - 1) + slot.branch;
            StampVoltageSource(&system, device.nets[1], device.nets[0], branchUnknown, ComplexValue(0.0, 0.0));
         }
         else if (device.kind == ComponentKind::SpdtSwitch)
         {
            Result stamped = StampSpdt(&system, device, slot, prepared.totalNets);
            if (IsErr(stamped))
            {
               result.code = stamped;
               result.message = device.name + " has an invalid contact.";
               return result;
            }
         }
         else if (device.kind == ComponentKind::DipSwitch)
         {
            Result stamped = StampDip(&system, device, slot, prepared.totalNets);
            if (IsErr(stamped))
            {
               result.code = stamped;
               result.message = device.name + " has an invalid contact.";
               return result;
            }
         }
         else if (device.kind == ComponentKind::Timer555)
         {
            uint8_t level = index < result.bias.deviceState.size() ? result.bias.deviceState[index] : TimerOutputLow;
            Result stamped = StampTimerRails(&system, device, level);
            if (IsErr(stamped))
            {
               result.code = stamped;
               result.message = device.name + " has an invalid output resistance.";
               return result;
            }
         }
      }
      Result semiconductors = StampAcSemiconductors(&system, netlist, prepared, result.bias, omega, settings.gminSiemens);
      if (IsErr(semiconductors))
      {
         result.code = semiconductors;
         result.message = Describe(semiconductors);
         return result;
      }
      if (pDrives != nullptr)
      {
         for (const LogicDrive& drive : *pDrives)
         {
            if (std::isfinite(drive.resistanceOhm) == false || drive.resistanceOhm <= 0.0)
            {
               result.code = Result::InvalidParameter;
               result.message = Describe(Result::InvalidParameter);
               return result;
            }
            StampConductance(&system, drive.netIndex, GroundNetIndex, ComplexValue(1.0 / drive.resistanceOhm, 0.0));
         }
      }
      if (unknownCount > 0)
      {
         Result solved = system.FactorizeAndSolve();
         if (IsErr(solved))
         {
            result.code = solved;
            result.message = Describe(solved);
            return result;
         }
      }
      AcPoint point = {};
      point.frequencyHz = frequency;
      point.netVoltageReal.assign(netlist.netCount, 0.0);
      point.netVoltageImag.assign(netlist.netCount, 0.0);
      for (uint32_t net = 1; net < netlist.netCount && unknownCount > 0; net += 1)
      {
         ComplexValue value = system.Solution(net - 1);
         point.netVoltageReal[net] = value.real();
         point.netVoltageImag[net] = value.imag();
      }
      result.points.push_back(point);
   }
   result.code = Result::Ok;
   result.message = Describe(Result::Ok);
   return result;
}

namespace
{

bool HasJunctionDevice(const Netlist& netlist)
{
   for (const NetlistDevice& device : netlist.devices)
   {
      if (device.kind == ComponentKind::Diode || device.kind == ComponentKind::Led
         || device.kind == ComponentKind::Npn || device.kind == ComponentKind::Pnp
         || device.kind == ComponentKind::Nmos || device.kind == ComponentKind::Pmos)
      {
         return true;
      }
   }
   return false;
}

double NodeVoltage(const VoltageList& voltage, uint32_t net)
{
   if (net >= voltage.size())
   {
      return 0.0;
   }
   return voltage[net];
}

void LimitTransientJunctions(const Netlist& netlist, const PreparedCircuit& prepared, const VoltageList& previous, const VoltageList& next, double* pAlpha)
{
   if (pAlpha == nullptr)
   {
      return;
   }
   for (size_t index = 0; index < netlist.devices.size() && index < prepared.devices.size(); index += 1)
   {
      const NetlistDevice& device = netlist.devices[index];
      if (device.kind == ComponentKind::Diode || device.kind == ComponentKind::Led)
      {
         const DiodeProperties* pDiode = std::get_if<DiodeProperties>(&device.properties);
         if (device.kind == ComponentKind::Led)
         {
            const LedProperties* pLed = std::get_if<LedProperties>(&device.properties);
            pDiode = pLed == nullptr ? nullptr : &pLed->diode;
         }
         if (pDiode == nullptr || device.pinCount < 2)
         {
            continue;
         }
         double thermal = pDiode->emissionCoefficient * ThermalVoltage(pDiode->temperatureCelsius);
         LimitJunctionAlpha(previous, next, prepared.devices[index].extra[0], device.nets[1], thermal, pAlpha);
      }
      else if (device.kind == ComponentKind::Npn || device.kind == ComponentKind::Pnp)
      {
         const BjtProperties* pBjt = std::get_if<BjtProperties>(&device.properties);
         if (pBjt == nullptr || device.pinCount < 3)
         {
            continue;
         }
         double thermal = pBjt->emissionCoefficient * ThermalVoltage(pBjt->temperatureCelsius);
         LimitJunctionAlpha(previous, next, device.nets[1], device.nets[2], thermal, pAlpha);
         LimitJunctionAlpha(previous, next, device.nets[1], device.nets[0], thermal, pAlpha);
      }
   }
}

Result StampJunctionDevices(DenseSystem<double>* pSystem, const Netlist& netlist, const PreparedCircuit& prepared, const VoltageList& operating, double gminSiemens, std::string* pMessage)
{
   if (pSystem == nullptr)
   {
      return Result::InvalidParameter;
   }
   for (size_t index = 0; index < netlist.devices.size() && index < prepared.devices.size(); index += 1)
   {
      const NetlistDevice& device = netlist.devices[index];
      const SolverDevice& slot = prepared.devices[index];
      if (device.kind == ComponentKind::Diode || device.kind == ComponentKind::Led)
      {
         const DiodeProperties* pDiode = nullptr;
         DiodeProperties copy = {};
         if (device.kind == ComponentKind::Led)
         {
            const LedProperties* pLed = std::get_if<LedProperties>(&device.properties);
            if (pLed == nullptr)
            {
               if (pMessage != nullptr)
               {
                  *pMessage = device.name + " is missing a junction model.";
               }
               return Result::InvalidParameter;
            }
            copy = pLed->diode;
            pDiode = &copy;
         }
         else
         {
            pDiode = std::get_if<DiodeProperties>(&device.properties);
         }
         if (pDiode == nullptr || device.pinCount < 2)
         {
            if (pMessage != nullptr)
            {
               *pMessage = device.name + " is missing a junction model.";
            }
            return Result::InvalidParameter;
         }
         uint32_t anode = device.nets[0];
         uint32_t junction = slot.extra[0];
         uint32_t cathode = device.nets[1];
         if (pDiode->seriesResistanceOhm > 0.0)
         {
            Result series = StampResistor(pSystem, anode, junction, pDiode->seriesResistanceOhm);
            if (IsErr(series))
            {
               return series;
            }
         }
         double junctionVoltage = VoltageBetween(operating, junction, cathode);
         DiodeLinearization linear = EvaluateDiode(*pDiode, junctionVoltage);
         double equivalent = linear.currentAmpere - (linear.conductanceSiemens * junctionVoltage);
         StampConductance(pSystem, junction, cathode, linear.conductanceSiemens + gminSiemens);
         StampInjection(pSystem, junction, -equivalent);
         StampInjection(pSystem, cathode, equivalent);
      }
      else if (device.kind == ComponentKind::Npn || device.kind == ComponentKind::Pnp)
      {
         const BjtProperties* pBjt = std::get_if<BjtProperties>(&device.properties);
         if (pBjt == nullptr || device.pinCount < 3)
         {
            if (pMessage != nullptr)
            {
               *pMessage = device.name + " is missing a junction model.";
            }
            return Result::InvalidParameter;
         }
         double base = NodeVoltage(operating, device.nets[1]);
         double collector = NodeVoltage(operating, device.nets[0]);
         double emitter = NodeVoltage(operating, device.nets[2]);
         BjtLinearization linear = EvaluateBjt(*pBjt, base - emitter, base - collector);
         double baseEmitter = base - emitter;
         double baseCollector = base - collector;
         double baseEquivalent = linear.baseCurrent - (linear.baseEmitterConductance * baseEmitter) - (linear.baseCollectorConductance * baseCollector);
         double collectorEquivalent = linear.collectorCurrent - (linear.collectorEmitterTransconductance * baseEmitter) - (linear.collectorCollectorConductance * baseCollector);
         StampConductance(pSystem, device.nets[1], device.nets[2], linear.baseEmitterConductance);
         StampConductance(pSystem, device.nets[1], device.nets[0], linear.baseCollectorConductance);
         AddControlled(pSystem, device.nets[0], device.nets[1], device.nets[2], linear.collectorEmitterTransconductance);
         AddControlled(pSystem, device.nets[0], device.nets[1], device.nets[0], linear.collectorCollectorConductance);
         AddControlled(pSystem, device.nets[2], device.nets[1], device.nets[2], -linear.collectorEmitterTransconductance);
         AddControlled(pSystem, device.nets[2], device.nets[1], device.nets[0], -linear.collectorCollectorConductance);
         StampInjection(pSystem, device.nets[1], -baseEquivalent);
         StampInjection(pSystem, device.nets[0], -collectorEquivalent);
         StampInjection(pSystem, device.nets[2], baseEquivalent + collectorEquivalent);
      }
      else if (device.kind == ComponentKind::Nmos || device.kind == ComponentKind::Pmos)
      {
         const MosfetProperties* pMosfet = std::get_if<MosfetProperties>(&device.properties);
         if (pMosfet == nullptr || device.pinCount < 3)
         {
            if (pMessage != nullptr)
            {
               *pMessage = device.name + " is missing a junction model.";
            }
            return Result::InvalidParameter;
         }
         double gate = NodeVoltage(operating, device.nets[1]);
         double drain = NodeVoltage(operating, device.nets[0]);
         double source = NodeVoltage(operating, device.nets[2]);
         MosfetLinearization linear = EvaluateMosfet(device.kind, *pMosfet, gate - source, drain - source);
         double gateSource = gate - source;
         double drainSource = drain - source;
         double equivalent = linear.drainCurrent - (linear.gateSourceTransconductance * gateSource) - (linear.drainSourceConductance * drainSource);
         AddGain(pSystem, device.nets[0], device.nets[1], linear.gateSourceTransconductance);
         AddGain(pSystem, device.nets[0], device.nets[2], -linear.gateSourceTransconductance - linear.drainSourceConductance);
         AddGain(pSystem, device.nets[0], device.nets[0], linear.drainSourceConductance);
         AddGain(pSystem, device.nets[2], device.nets[1], -linear.gateSourceTransconductance);
         AddGain(pSystem, device.nets[2], device.nets[2], linear.gateSourceTransconductance + linear.drainSourceConductance);
         AddGain(pSystem, device.nets[2], device.nets[0], -linear.drainSourceConductance);
         StampInjection(pSystem, device.nets[0], -equivalent);
         StampInjection(pSystem, device.nets[2], equivalent);
      }
   }
   return Result::Ok;
}

Result StampTransientLinear(DenseSystem<double>* pSystem, const Netlist& netlist, const PreparedCircuit& prepared, const AnalysisSettings& settings, const VoltageList& acceptedVoltage, const std::vector<double>& inductorCurrent, const std::vector<uint8_t>& trialLevel, double timeSec, double stepSec, uint32_t acceptedSteps, std::string* pMessage)
{
   if (pSystem == nullptr)
   {
      return Result::InvalidParameter;
   }
   for (size_t index = 0; index < netlist.devices.size() && index < prepared.devices.size(); index += 1)
   {
      const NetlistDevice& device = netlist.devices[index];
      const SolverDevice& slot = prepared.devices[index];
      if (device.kind == ComponentKind::VoltageSource)
      {
         const VoltageSourceProperties* pSource = std::get_if<VoltageSourceProperties>(&device.properties);
         if (pSource == nullptr || slot.branch == NoBranch)
         {
            return Result::InvalidParameter;
         }
         uint32_t branchUnknown = (prepared.totalNets - 1) + slot.branch;
         StampVoltageSource(pSystem, device.nets[1], device.nets[0], branchUnknown, SourceVoltageAt(*pSource, timeSec + stepSec, true));
      }
      else if (device.kind == ComponentKind::CurrentSource)
      {
         const CurrentSourceProperties* pSource = std::get_if<CurrentSourceProperties>(&device.properties);
         if (pSource == nullptr)
         {
            return Result::InvalidParameter;
         }
         double current = SourceCurrentAt(*pSource, timeSec + stepSec, true);
         StampInjection(pSystem, device.nets[0], -current);
         StampInjection(pSystem, device.nets[1], current);
      }
      else if (device.kind == ComponentKind::Resistor)
      {
         const ResistorProperties* pResistor = std::get_if<ResistorProperties>(&device.properties);
         if (pResistor == nullptr)
         {
            return Result::InvalidParameter;
         }
         Result stamped = StampResistor(pSystem, device.nets[0], device.nets[1], pResistor->resistanceOhm);
         if (IsErr(stamped))
         {
            return stamped;
         }
      }
      else if (device.kind == ComponentKind::Capacitor)
      {
         const CapacitorProperties* pCapacitor = std::get_if<CapacitorProperties>(&device.properties);
         if (pCapacitor == nullptr)
         {
            return Result::InvalidParameter;
         }
         uint32_t positiveNet = device.nets[0];
         uint32_t negativeNet = device.nets[1];
         if (pCapacitor->esrOhm > 0.0)
         {
            uint32_t junctionNet = slot.extra[0];
            Result series = StampResistor(pSystem, positiveNet, junctionNet, pCapacitor->esrOhm);
            if (IsErr(series))
            {
               return series;
            }
            positiveNet = junctionNet;
         }
         double previous = VoltageBetween(acceptedVoltage, positiveNet, negativeNet);
         if (acceptedSteps == 0 && settings.initialConditions == InitialConditionMode::ComponentValues)
         {
            previous = pCapacitor->initialVoltageVolt;
         }
         double equivalentConductance = pCapacitor->capacitanceFarad / stepSec;
         double historyCurrent = 0.0;
         if (settings.integration == IntegrationMethod::Trapezoidal)
         {
            equivalentConductance *= 2.0;
            if (index < inductorCurrent.size())
            {
               historyCurrent = inductorCurrent[index];
            }
         }
         StampConductance(pSystem, positiveNet, negativeNet, equivalentConductance);
         StampInjection(pSystem, positiveNet, (equivalentConductance * previous) + historyCurrent);
         StampInjection(pSystem, negativeNet, -((equivalentConductance * previous) + historyCurrent));
      }
      else if (device.kind == ComponentKind::Inductor)
      {
         const InductorProperties* pInductor = std::get_if<InductorProperties>(&device.properties);
         if (pInductor == nullptr)
         {
            return Result::InvalidParameter;
         }
         double equivalentConductance = stepSec / pInductor->inductanceHenry;
         if (settings.integration == IntegrationMethod::Trapezoidal)
         {
            equivalentConductance *= 2.0;
         }
         double historyCurrent = index < inductorCurrent.size() ? inductorCurrent[index] : 0.0;
         StampConductance(pSystem, device.nets[0], device.nets[1], equivalentConductance);
         StampInjection(pSystem, device.nets[0], -historyCurrent);
         StampInjection(pSystem, device.nets[1], historyCurrent);
      }
      else if (device.kind == ComponentKind::SpdtSwitch)
      {
         Result stamped = StampSpdt(pSystem, device, slot, prepared.totalNets);
         if (IsErr(stamped))
         {
            if (pMessage != nullptr)
            {
               *pMessage = device.name + " has an invalid contact.";
            }
            return stamped;
         }
      }
      else if (device.kind == ComponentKind::DipSwitch)
      {
         Result stamped = StampDip(pSystem, device, slot, prepared.totalNets);
         if (IsErr(stamped))
         {
            if (pMessage != nullptr)
            {
               *pMessage = device.name + " has an invalid contact.";
            }
            return stamped;
         }
      }
      else if (device.kind == ComponentKind::SpstSwitch || device.kind == ComponentKind::Pushbutton)
      {
         bool closed = false;
         ResistanceMode closedMode = ResistanceMode::Ideal;
         ResistanceMode openMode = ResistanceMode::Ideal;
         double closedResistance = 0.0;
         double openResistance = 0.0;
         double contactTime = timeSec + stepSec;
         if (device.kind == ComponentKind::SpstSwitch)
         {
            const SpstProperties* pSwitch = std::get_if<SpstProperties>(&device.properties);
            if (pSwitch == nullptr)
            {
               return Result::InvalidParameter;
            }
            closed = SwitchClosedAt(*pSwitch, contactTime, true);
            closedMode = pSwitch->closedMode;
            openMode = pSwitch->openMode;
            closedResistance = pSwitch->closedResistanceOhm;
            openResistance = pSwitch->openResistanceOhm;
         }
         else
         {
            const PushbuttonProperties* pButton = std::get_if<PushbuttonProperties>(&device.properties);
            if (pButton == nullptr)
            {
               return Result::InvalidParameter;
            }
            closed = PushbuttonClosedAt(*pButton, contactTime, true);
            closedMode = pButton->closedMode;
            openMode = pButton->openMode;
            closedResistance = pButton->closedResistanceOhm;
            openResistance = pButton->openResistanceOhm;
         }
         if (closed && closedMode == ResistanceMode::Ideal && slot.branch != NoBranch)
         {
            uint32_t branchUnknown = (prepared.totalNets - 1) + slot.branch;
            StampVoltageSource(pSystem, device.nets[0], device.nets[1], branchUnknown, 0.0);
         }
         else if (closed && closedMode == ResistanceMode::Resistive)
         {
            Result stamped = StampResistor(pSystem, device.nets[0], device.nets[1], closedResistance);
            if (IsErr(stamped))
            {
               return stamped;
            }
         }
         else if (closed == false && openMode == ResistanceMode::Resistive)
         {
            Result stamped = StampResistor(pSystem, device.nets[0], device.nets[1], openResistance);
            if (IsErr(stamped))
            {
               return stamped;
            }
         }
      }
      else if (device.kind == ComponentKind::Timer555)
      {
         uint8_t level = index < trialLevel.size() ? trialLevel[index] : TimerOutputLow;
         Result stamped = StampTimerRails(pSystem, device, level);
         if (IsErr(stamped))
         {
            if (pMessage != nullptr)
            {
               *pMessage = device.name + " has an invalid output resistance.";
            }
            return stamped;
         }
      }
   }
   return Result::Ok;
}

} // namespace

TransientResult SolveTransientAnalog(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel, const LogicDriveList* pDrives, double horizonSec, TransientContinuity* pContinuity)
{
   TransientResult result = {};
   if (settings.stopTimeSec <= 0.0 || settings.maximumStepSec <= 0.0)
   {
      result.code = Result::InvalidTimeStep;
      result.message = Describe(Result::InvalidTimeStep);
      return result;
   }
   double horizon = settings.stopTimeSec;
   if (horizonSec > 0.0 && horizonSec < horizon)
   {
      horizon = horizonSec;
   }
   VoltageList voltage(netlist.netCount, 0.0);
   std::vector<double> inductorCurrent(netlist.devices.size(), 0.0);
   std::vector<uint8_t> timerLevel(netlist.devices.size(), TimerOutputLow);
   std::vector<double> timerChangeAt(netlist.devices.size(), TimerNoSchedule);
   double timeSec = 0.0;
   double stepSec = settings.maximumStepSec;
   uint32_t accepted = 0;
   bool continueSlice = pContinuity != nullptr && pContinuity->ready;
   if (continueSlice)
   {
      timeSec = pContinuity->timeSec;
      stepSec = pContinuity->stepSec > 0.0 ? pContinuity->stepSec : settings.maximumStepSec;
      accepted = pContinuity->acceptedSteps;
      voltage = pContinuity->voltage;
      inductorCurrent = pContinuity->inductorCurrent;
      timerLevel = pContinuity->timerLevel;
      timerChangeAt = pContinuity->timerChangeAt;
      if (voltage.size() < netlist.netCount)
      {
         voltage.resize(netlist.netCount, 0.0);
      }
   }
   else
   {
      AnalysisSettings biasSettings = settings;
      result.initial = SolveDcAnalog(netlist, biasSettings, pCancel, pDrives);
      if (IsErr(result.initial.code))
      {
         result.code = result.initial.code;
         result.message = result.initial.message;
         return result;
      }
      voltage = result.initial.netVoltage;
      if (voltage.size() < netlist.netCount)
      {
         voltage.assign(netlist.netCount, 0.0);
      }
      if (result.initial.deviceState.size() == netlist.devices.size())
      {
         timerLevel = result.initial.deviceState;
      }
      if (settings.initialConditions == InitialConditionMode::ComponentValues)
      {
         for (size_t index = 0; index < netlist.devices.size(); index += 1)
         {
            const NetlistDevice& device = netlist.devices[index];
            if (device.kind == ComponentKind::Inductor)
            {
               const InductorProperties* pInductor = std::get_if<InductorProperties>(&device.properties);
               if (pInductor != nullptr)
               {
                  inductorCurrent[index] = pInductor->initialCurrentAmpere;
               }
            }
         }
      }
      RecordSample(&result, voltage, netlist.netCount, 0.0);
   }
   while (timeSec < horizon && accepted < MaxTransientSteps)
   {
      if (pCancel != nullptr && pCancel->IsRequested())
      {
         result.code = Result::Error;
         result.message = "Cancelled.";
         return result;
      }
      if (timeSec + stepSec > horizon)
      {
         stepSec = horizon - timeSec;
      }
      PreparedCircuit prepared = Prepare(netlist, false, SwitchSchedule::Transient, timeSec + stepSec);
      if (IsErr(prepared.code))
      {
         result.code = prepared.code;
         result.message = prepared.message;
         return result;
      }
      uint32_t unknownCount = UnknownCountOf(prepared);
      std::vector<uint8_t> trialLevel = timerLevel;
      std::vector<double> trialChangeAt = timerChangeAt;
      for (size_t index = 0; index < netlist.devices.size(); index += 1)
      {
         if (netlist.devices[index].kind == ComponentKind::Timer555)
         {
            ScheduleTimer(netlist.devices[index], voltage, timeSec, stepSec, &trialLevel[index], &trialChangeAt[index]);
         }
      }
      bool junctionCircuit = HasJunctionDevice(netlist);
      VoltageList operating(prepared.totalNets, 0.0);
      auto storedNetCount = static_cast<uint32_t>(voltage.size());
      for (uint32_t net = 0; net < prepared.totalNets && net < storedNetCount; net += 1)
      {
         operating[net] = voltage[net];
      }
      bool converged = unknownCount == 0;
      std::string stampMessage;
      for (uint32_t iteration = 0; iteration < MaxNewtonIterations && converged == false; iteration += 1)
      {
         DenseSystem<double> system;
         system.Reset(unknownCount);
         StampGmin(&system, std::max(prepared.totalNets, netlist.netCount), settings.gminSiemens);
         Result linear = StampTransientLinear(&system, netlist, prepared, settings, voltage, inductorCurrent, trialLevel, timeSec, stepSec, accepted, &stampMessage);
         if (IsErr(linear))
         {
            result.code = linear;
            result.message = stampMessage.empty() ? Describe(linear) : stampMessage;
            return result;
         }
         Result junctions = StampJunctionDevices(&system, netlist, prepared, operating, settings.gminSiemens, &stampMessage);
         if (IsErr(junctions))
         {
            result.code = junctions;
            result.message = stampMessage.empty() ? Describe(junctions) : stampMessage;
            return result;
         }
         Result drivesStamped = StampLogicDrives(&system, pDrives, DriveStamp::DcNorton, 1.0);
         if (IsErr(drivesStamped))
         {
            result.code = drivesStamped;
            result.message = Describe(drivesStamped);
            return result;
         }
         Result solved = system.FactorizeAndSolve();
         if (IsErr(solved))
         {
            converged = false;
            break;
         }
         VoltageList proposed(prepared.totalNets, 0.0);
         for (uint32_t net = 1; net < prepared.totalNets; net += 1)
         {
            proposed[net] = system.Solution(net - 1);
         }
         double alpha = 1.0;
         LimitTransientJunctions(netlist, prepared, operating, proposed, &alpha);
         double maxChange = 0.0;
         for (uint32_t net = 1; net < prepared.totalNets; net += 1)
         {
            double updated = operating[net] + (alpha * (proposed[net] - operating[net]));
            maxChange = std::max(maxChange, std::abs(updated - operating[net]));
            operating[net] = updated;
         }
         if (junctionCircuit == false || (alpha > 0.999 && maxChange <= (VoltageAbsoluteTolerance + VoltageRelativeTolerance)))
         {
            converged = true;
         }
      }
      if (converged == false)
      {
         stepSec *= 0.5;
         if (stepSec < settings.maximumStepSec * 1.0e-6)
         {
            result.code = Result::NoConvergence;
            result.message = Describe(Result::NoConvergence);
            return result;
         }
         continue;
      }
      voltage = operating;
      timerLevel = trialLevel;
      timerChangeAt = trialChangeAt;
      for (size_t index = 0; index < netlist.devices.size(); index += 1)
      {
         const NetlistDevice& device = netlist.devices[index];
         if (device.kind == ComponentKind::Inductor)
         {
            const InductorProperties* pInductor = std::get_if<InductorProperties>(&device.properties);
            if (pInductor != nullptr)
            {
               double geq = stepSec / pInductor->inductanceHenry;
               if (settings.integration == IntegrationMethod::Trapezoidal)
               {
                  geq *= 2.0;
               }
               inductorCurrent[index] = inductorCurrent[index] + (geq * VoltageBetween(voltage, device.nets[0], device.nets[1]));
            }
         }
         if (device.kind == ComponentKind::Capacitor && settings.integration == IntegrationMethod::Trapezoidal)
         {
            const CapacitorProperties* pCapacitor = std::get_if<CapacitorProperties>(&device.properties);
            if (pCapacitor != nullptr)
            {
               double geq = 2.0 * pCapacitor->capacitanceFarad / stepSec;
               double previous = result.samples.back().netVoltage[device.nets[0]] - result.samples.back().netVoltage[device.nets[1]];
               double now = VoltageBetween(voltage, device.nets[0], device.nets[1]);
               inductorCurrent[index] = (geq * (now - previous)) - inductorCurrent[index];
            }
         }
      }
      timeSec += stepSec;
      accepted += 1;
      RecordSample(&result, voltage, netlist.netCount, timeSec);
      if (stepSec < settings.maximumStepSec)
      {
         stepSec = std::min(settings.maximumStepSec, stepSec * 1.2);
      }
      if (pContinuity != nullptr)
      {
         pContinuity->timeSec = timeSec;
         pContinuity->stepSec = stepSec;
         pContinuity->voltage = voltage;
         pContinuity->inductorCurrent = inductorCurrent;
         pContinuity->timerLevel = timerLevel;
         pContinuity->timerChangeAt = timerChangeAt;
         pContinuity->acceptedSteps = accepted;
         pContinuity->ready = true;
      }
   }
   result.code = Result::Ok;
   result.message = Describe(Result::Ok);
   return result;
}

} // namespace Analyzer
