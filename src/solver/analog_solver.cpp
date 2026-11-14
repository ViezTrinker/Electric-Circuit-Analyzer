/*!
 * \file analog_solver.cpp
 * \brief Modified nodal analysis for DC, AC, and transient circuits.
 */

#include "analog_solver.h"

#include "dense_system.h"
#include "device_model.h"

#include <algorithm>
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
constexpr uint32_t MaxTransientSteps = 200000;
constexpr double VoltageAbsoluteTolerance = 1.0e-6;
constexpr double VoltageRelativeTolerance = 1.0e-5;
constexpr double JunctionStepFactor = 2.0;

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

PreparedCircuit Prepare(const Netlist& netlist, bool directCurrent)
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
         if (pSwitch != nullptr && SwitchClosedAt(*pSwitch, 0.0, false) && pSwitch->closedMode == ResistanceMode::Ideal)
         {
            idealShort = true;
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

void AddGain(DenseSystem<double>* pSystem, uint32_t rowNet, uint32_t columnNet, double gain)
{
   NodeRef<double> row = MapNode<double>(rowNet);
   NodeRef<double> column = MapNode<double>(columnNet);
   if (row.mapped && column.mapped)
   {
      pSystem->Add(row.index, column.index, gain);
   }
}

void AddControlled(DenseSystem<double>* pSystem, uint32_t intoNet, uint32_t plusNet, uint32_t minusNet, double gain)
{
   AddGain(pSystem, intoNet, plusNet, gain);
   AddGain(pSystem, intoNet, minusNet, -gain);
}

void RecordSample(TransientResult* pResult, const VoltageList& voltage, uint32_t netCount, double timeSec)
{
   TransientSample sample = {};
   sample.timeSec = timeSec;
   sample.netVoltage = voltage;
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

} // namespace

DcResult SolveDcAnalog(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel)
{
   DcResult result = {};
   result.code = Result::Ok;
   result.netVoltage.assign(netlist.netCount, 0.0);
   result.netLevel.assign(netlist.netCount, LogicLevel::HighZ);
   if (netlist.hasAnalog == false)
   {
      return result;
   }
   PreparedCircuit prepared = Prepare(netlist, true);
   if (IsErr(prepared.code))
   {
      result.code = prepared.code;
      result.message = prepared.message.empty() ? Describe(prepared.code) : prepared.message;
      return result;
   }
   uint32_t unknownCount = UnknownCountOf(prepared);
   VoltageList voltage(prepared.totalNets, 0.0);
   VoltageList proposed(prepared.totalNets, 0.0);
   std::vector<double> branchCurrents(prepared.branchCount, 0.0);
   bool converged = false;
   for (uint32_t iteration = 0; iteration < MaxNewtonIterations; iteration += 1)
   {
      if (pCancel != nullptr && pCancel->IsRequested())
      {
         result.code = Result::Error;
         result.message = "Cancelled.";
         return result;
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
            StampVoltageSource(&system, device.nets[1], device.nets[0], branchUnknown, pSource->dcVoltageVolt);
         }
         else if (device.kind == ComponentKind::CurrentSource)
         {
            const CurrentSourceProperties* pSource = std::get_if<CurrentSourceProperties>(&device.properties);
            if (pSource == nullptr)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            StampInjection(&system, device.nets[0], -pSource->dcCurrentAmpere);
            StampInjection(&system, device.nets[1], pSource->dcCurrentAmpere);
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
      }
      if (unknownCount == 0)
      {
         converged = true;
         break;
      }
      Result solved = system.FactorizeAndSolve();
      if (IsErr(solved))
      {
         result.code = solved;
         result.message = Describe(solved);
         return result;
      }
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
         if (device.kind != ComponentKind::Diode && device.kind != ComponentKind::Led)
         {
            continue;
         }
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
         double previousJunction = VoltageBetween(voltage, junction, device.nets[1]);
         double nextJunction = VoltageBetween(proposed, junction, device.nets[1]);
         double thermal = pDiode->emissionCoefficient * ThermalVoltage(pDiode->temperatureCelsius);
         double limit = JunctionStepFactor * thermal;
         double step = std::abs(nextJunction - previousJunction);
         if (limit > 0.0 && step > limit)
         {
            alpha = std::min(alpha, limit / step);
         }
      }
      double maxChange = 0.0;
      for (uint32_t net = 1; net < prepared.totalNets; net += 1)
      {
         double updated = voltage[net] + (alpha * (proposed[net] - voltage[net]));
         maxChange = std::max(maxChange, std::abs(updated - voltage[net]));
         voltage[net] = updated;
      }
      if (alpha > 0.999 && maxChange <= (VoltageAbsoluteTolerance + VoltageRelativeTolerance))
      {
         converged = true;
         break;
      }
   }
   if (converged == false)
   {
      result.code = Result::NoConvergence;
      result.message = Describe(Result::NoConvergence);
      return result;
   }
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
   result.warnings = netlist.warnings;
   result.message = Describe(Result::Ok);
   return result;
}

AcResult SolveAcAnalog(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel)
{
   AcResult result = {};
   result.bias = SolveDcAnalog(netlist, settings, pCancel);
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
   PreparedCircuit prepared = Prepare(netlist, false);
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

TransientResult SolveTransientAnalog(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel)
{
   TransientResult result = {};
   if (settings.stopTimeSec <= 0.0 || settings.maximumStepSec <= 0.0)
   {
      result.code = Result::InvalidTimeStep;
      result.message = Describe(Result::InvalidTimeStep);
      return result;
   }
   AnalysisSettings biasSettings = settings;
   result.initial = SolveDcAnalog(netlist, biasSettings, pCancel);
   if (IsErr(result.initial.code))
   {
      result.code = result.initial.code;
      result.message = result.initial.message;
      return result;
   }
   VoltageList voltage = result.initial.netVoltage;
   if (voltage.size() < netlist.netCount)
   {
      voltage.assign(netlist.netCount, 0.0);
   }
   std::vector<double> inductorCurrent(netlist.devices.size(), 0.0);
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
   double timeSec = 0.0;
   double stepSec = settings.maximumStepSec;
   uint32_t accepted = 0;
   while (timeSec < settings.stopTimeSec && accepted < MaxTransientSteps)
   {
      if (pCancel != nullptr && pCancel->IsRequested())
      {
         result.code = Result::Error;
         result.message = "Cancelled.";
         return result;
      }
      if (timeSec + stepSec > settings.stopTimeSec)
      {
         stepSec = settings.stopTimeSec - timeSec;
      }
      PreparedCircuit prepared = Prepare(netlist, false);
      if (IsErr(prepared.code))
      {
         result.code = prepared.code;
         result.message = prepared.message;
         return result;
      }
      uint32_t unknownCount = UnknownCountOf(prepared);
      DenseSystem<double> system;
      system.Reset(unknownCount);
      StampGmin(&system, std::max(prepared.totalNets, netlist.netCount), settings.gminSiemens);
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
               return result;
            }
            uint32_t branchUnknown = (prepared.totalNets - 1) + slot.branch;
            StampVoltageSource(&system, device.nets[1], device.nets[0], branchUnknown, SourceVoltageAt(*pSource, timeSec + stepSec, true));
         }
         else if (device.kind == ComponentKind::CurrentSource)
         {
            const CurrentSourceProperties* pSource = std::get_if<CurrentSourceProperties>(&device.properties);
            if (pSource == nullptr)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            double current = SourceCurrentAt(*pSource, timeSec + stepSec, true);
            StampInjection(&system, device.nets[0], -current);
            StampInjection(&system, device.nets[1], current);
         }
         else if (device.kind == ComponentKind::Resistor)
         {
            const ResistorProperties* pResistor = std::get_if<ResistorProperties>(&device.properties);
            if (pResistor == nullptr)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            Result stamped = StampResistor(&system, device.nets[0], device.nets[1], pResistor->resistanceOhm);
            if (IsErr(stamped))
            {
               result.code = stamped;
               return result;
            }
         }
         else if (device.kind == ComponentKind::Capacitor)
         {
            const CapacitorProperties* pCapacitor = std::get_if<CapacitorProperties>(&device.properties);
            if (pCapacitor == nullptr)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            double previous = VoltageBetween(voltage, device.nets[0], device.nets[1]);
            if (accepted == 0 && settings.initialConditions == InitialConditionMode::ComponentValues)
            {
               previous = pCapacitor->initialVoltageVolt;
            }
            double geq = pCapacitor->capacitanceFarad / stepSec;
            double historyCurrent = 0.0;
            if (settings.integration == IntegrationMethod::Trapezoidal)
            {
               geq *= 2.0;
               historyCurrent = inductorCurrent[index];
            }
            StampConductance(&system, device.nets[0], device.nets[1], geq);
            StampInjection(&system, device.nets[0], (geq * previous) + historyCurrent);
            StampInjection(&system, device.nets[1], -((geq * previous) + historyCurrent));
         }
         else if (device.kind == ComponentKind::Inductor)
         {
            const InductorProperties* pInductor = std::get_if<InductorProperties>(&device.properties);
            if (pInductor == nullptr)
            {
               result.code = Result::InvalidParameter;
               return result;
            }
            double geq = stepSec / pInductor->inductanceHenry;
            if (settings.integration == IntegrationMethod::Trapezoidal)
            {
               geq *= 2.0;
            }
            StampConductance(&system, device.nets[0], device.nets[1], geq);
            StampInjection(&system, device.nets[0], -inductorCurrent[index]);
            StampInjection(&system, device.nets[1], inductorCurrent[index]);
         }
      }
      if (unknownCount > 0)
      {
         Result solved = system.FactorizeAndSolve();
         if (IsErr(solved))
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
         for (uint32_t net = 1; net < netlist.netCount; net += 1)
         {
            voltage[net] = system.Solution(net - 1);
         }
      }
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
   }
   result.code = Result::Ok;
   result.message = Describe(Result::Ok);
   return result;
}

} // namespace Analyzer
