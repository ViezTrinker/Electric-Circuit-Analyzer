/*!
 * \file component_access.cpp
 * \brief Reads and writes the numeric and enumeration fields of a component.
 */

#include "component.h"

#include <cmath>

namespace Analyzer
{

namespace
{

constexpr double FullInputCountMin = 2.0;
constexpr double FullInputCountMax = 8.0;
constexpr uint8_t CounterValueMax = 15;

bool Belongs(double value)
{
   return std::isfinite(value);
}

Result RequirePositive(double value)
{
   if (Belongs(value) == false || value <= 0.0)
   {
      return Result::InvalidParameter;
   }
   return Result::Ok;
}

Result RequireNonNegative(double value)
{
   if (Belongs(value) == false || value < 0.0)
   {
      return Result::InvalidParameter;
   }
   return Result::Ok;
}

LogicProperties* MutableLogic(Component* pComponent)
{
   if (LogicProperties* pLogic = std::get_if<LogicProperties>(&pComponent->properties))
   {
      return pLogic;
   }
   if (SequentialProperties* pSequential = std::get_if<SequentialProperties>(&pComponent->properties))
   {
      return &pSequential->logic;
   }
   if (ClockProperties* pClock = std::get_if<ClockProperties>(&pComponent->properties))
   {
      return &pClock->logic;
   }
   if (MuxProperties* pMux = std::get_if<MuxProperties>(&pComponent->properties))
   {
      return &pMux->logic;
   }
   if (DecoderProperties* pDecoder = std::get_if<DecoderProperties>(&pComponent->properties))
   {
      return &pDecoder->logic;
   }
   if (CounterProperties* pCounter = std::get_if<CounterProperties>(&pComponent->properties))
   {
      return &pCounter->logic;
   }
   if (ShiftRegisterProperties* pShift = std::get_if<ShiftRegisterProperties>(&pComponent->properties))
   {
      return &pShift->logic;
   }
   if (PullProperties* pPull = std::get_if<PullProperties>(&pComponent->properties))
   {
      return &pPull->logic;
   }
   if (AdcProperties* pAdc = std::get_if<AdcProperties>(&pComponent->properties))
   {
      return &pAdc->logic;
   }
   if (DacProperties* pDac = std::get_if<DacProperties>(&pComponent->properties))
   {
      return &pDac->logic;
   }
   return nullptr;
}

const LogicProperties* ViewLogic(const Component& component)
{
   return MutableLogic(const_cast<Component*>(&component));
}

} // namespace

Result ReadNumeric(const Component& component, NumericProperty property, double* pValue)
{
   if (pValue == nullptr)
   {
      return Result::InvalidParameter;
   }
   if (const ResistorProperties* pResistor = std::get_if<ResistorProperties>(&component.properties))
   {
      if (property == NumericProperty::ResistanceOhm)
      {
         *pValue = pResistor->resistanceOhm;
         return Result::Ok;
      }
   }
   if (const CapacitorProperties* pCapacitor = std::get_if<CapacitorProperties>(&component.properties))
   {
      if (property == NumericProperty::CapacitanceFarad) { *pValue = pCapacitor->capacitanceFarad; return Result::Ok; }
      if (property == NumericProperty::EsrOhm) { *pValue = pCapacitor->esrOhm; return Result::Ok; }
      if (property == NumericProperty::InitialVoltageVolt) { *pValue = pCapacitor->initialVoltageVolt; return Result::Ok; }
   }
   if (const InductorProperties* pInductor = std::get_if<InductorProperties>(&component.properties))
   {
      if (property == NumericProperty::InductanceHenry) { *pValue = pInductor->inductanceHenry; return Result::Ok; }
      if (property == NumericProperty::SeriesResistanceOhm) { *pValue = pInductor->seriesResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::InitialCurrentAmpere) { *pValue = pInductor->initialCurrentAmpere; return Result::Ok; }
   }
   if (const PotentiometerProperties* pPotentiometer = std::get_if<PotentiometerProperties>(&component.properties))
   {
      if (property == NumericProperty::ResistanceOhm) { *pValue = pPotentiometer->resistanceOhm; return Result::Ok; }
      if (property == NumericProperty::WiperPosition) { *pValue = pPotentiometer->wiperPosition; return Result::Ok; }
   }
   if (const VoltageSourceProperties* pSource = std::get_if<VoltageSourceProperties>(&component.properties))
   {
      if (property == NumericProperty::DcVoltageVolt) { *pValue = pSource->dcVoltageVolt; return Result::Ok; }
      if (property == NumericProperty::AcAmplitudeVolt) { *pValue = pSource->acAmplitudeVolt; return Result::Ok; }
      if (property == NumericProperty::AcPhaseDeg) { *pValue = pSource->acPhaseDeg; return Result::Ok; }
      if (property == NumericProperty::SineOffset) { *pValue = pSource->sineOffsetVolt; return Result::Ok; }
      if (property == NumericProperty::SineAmplitude) { *pValue = pSource->sineAmplitudeVolt; return Result::Ok; }
      if (property == NumericProperty::SineFrequencyHz) { *pValue = pSource->sineFrequencyHz; return Result::Ok; }
      if (property == NumericProperty::SinePhaseDeg) { *pValue = pSource->sinePhaseDeg; return Result::Ok; }
      if (property == NumericProperty::SineDelaySec) { *pValue = pSource->sineDelaySec; return Result::Ok; }
      if (property == NumericProperty::PulseInitial) { *pValue = pSource->pulseInitialVolt; return Result::Ok; }
      if (property == NumericProperty::PulseLevel) { *pValue = pSource->pulseLevelVolt; return Result::Ok; }
      if (property == NumericProperty::PulseDelaySec) { *pValue = pSource->pulseDelaySec; return Result::Ok; }
      if (property == NumericProperty::PulseRiseSec) { *pValue = pSource->pulseRiseSec; return Result::Ok; }
      if (property == NumericProperty::PulseFallSec) { *pValue = pSource->pulseFallSec; return Result::Ok; }
      if (property == NumericProperty::PulseWidthSec) { *pValue = pSource->pulseWidthSec; return Result::Ok; }
      if (property == NumericProperty::PulsePeriodSec) { *pValue = pSource->pulsePeriodSec; return Result::Ok; }
   }
   if (const CurrentSourceProperties* pSource = std::get_if<CurrentSourceProperties>(&component.properties))
   {
      if (property == NumericProperty::DcCurrentAmpere) { *pValue = pSource->dcCurrentAmpere; return Result::Ok; }
      if (property == NumericProperty::AcAmplitudeAmpere) { *pValue = pSource->acAmplitudeAmpere; return Result::Ok; }
      if (property == NumericProperty::AcPhaseDeg) { *pValue = pSource->acPhaseDeg; return Result::Ok; }
      if (property == NumericProperty::SineOffset) { *pValue = pSource->sineOffsetAmpere; return Result::Ok; }
      if (property == NumericProperty::SineAmplitude) { *pValue = pSource->sineAmplitudeAmpere; return Result::Ok; }
      if (property == NumericProperty::SineFrequencyHz) { *pValue = pSource->sineFrequencyHz; return Result::Ok; }
      if (property == NumericProperty::SinePhaseDeg) { *pValue = pSource->sinePhaseDeg; return Result::Ok; }
      if (property == NumericProperty::SineDelaySec) { *pValue = pSource->sineDelaySec; return Result::Ok; }
      if (property == NumericProperty::PulseInitial) { *pValue = pSource->pulseInitialAmpere; return Result::Ok; }
      if (property == NumericProperty::PulseLevel) { *pValue = pSource->pulseLevelAmpere; return Result::Ok; }
      if (property == NumericProperty::PulseDelaySec) { *pValue = pSource->pulseDelaySec; return Result::Ok; }
      if (property == NumericProperty::PulseRiseSec) { *pValue = pSource->pulseRiseSec; return Result::Ok; }
      if (property == NumericProperty::PulseFallSec) { *pValue = pSource->pulseFallSec; return Result::Ok; }
      if (property == NumericProperty::PulseWidthSec) { *pValue = pSource->pulseWidthSec; return Result::Ok; }
      if (property == NumericProperty::PulsePeriodSec) { *pValue = pSource->pulsePeriodSec; return Result::Ok; }
   }
   if (const DiodeProperties* pDiode = std::get_if<DiodeProperties>(&component.properties))
   {
      if (property == NumericProperty::ForwardVoltageVolt) { *pValue = pDiode->forwardVoltageVolt; return Result::Ok; }
      if (property == NumericProperty::ForwardCurrentAmpere) { *pValue = pDiode->forwardCurrentAmpere; return Result::Ok; }
      if (property == NumericProperty::EmissionCoefficient) { *pValue = pDiode->emissionCoefficient; return Result::Ok; }
      if (property == NumericProperty::SeriesResistanceOhm) { *pValue = pDiode->seriesResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::TemperatureCelsius) { *pValue = pDiode->temperatureCelsius; return Result::Ok; }
      if (property == NumericProperty::JunctionCapacitanceFarad) { *pValue = pDiode->junctionCapacitanceFarad; return Result::Ok; }
      if (property == NumericProperty::JunctionPotentialVolt) { *pValue = pDiode->junctionPotentialVolt; return Result::Ok; }
      if (property == NumericProperty::GradingCoefficient) { *pValue = pDiode->gradingCoefficient; return Result::Ok; }
      if (property == NumericProperty::TransitTimeSec) { *pValue = pDiode->transitTimeSec; return Result::Ok; }
   }
   if (const LedProperties* pLed = std::get_if<LedProperties>(&component.properties))
   {
      Component diodeView = component;
      diodeView.properties = pLed->diode;
      return ReadNumeric(diodeView, property, pValue);
   }
   if (const BjtProperties* pBjt = std::get_if<BjtProperties>(&component.properties))
   {
      if (property == NumericProperty::ForwardVoltageVolt) { *pValue = pBjt->forwardVoltageVolt; return Result::Ok; }
      if (property == NumericProperty::ForwardCurrentAmpere) { *pValue = pBjt->forwardCurrentAmpere; return Result::Ok; }
      if (property == NumericProperty::EmissionCoefficient) { *pValue = pBjt->emissionCoefficient; return Result::Ok; }
      if (property == NumericProperty::TemperatureCelsius) { *pValue = pBjt->temperatureCelsius; return Result::Ok; }
      if (property == NumericProperty::ForwardBeta) { *pValue = pBjt->forwardBeta; return Result::Ok; }
      if (property == NumericProperty::ReverseBeta) { *pValue = pBjt->reverseBeta; return Result::Ok; }
      if (property == NumericProperty::EarlyVoltageVolt) { *pValue = pBjt->earlyVoltageVolt; return Result::Ok; }
      if (property == NumericProperty::BaseResistanceOhm) { *pValue = pBjt->baseResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::CollectorResistanceOhm) { *pValue = pBjt->collectorResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::EmitterResistanceOhm) { *pValue = pBjt->emitterResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::CjeFarad) { *pValue = pBjt->cjeFarad; return Result::Ok; }
      if (property == NumericProperty::CjcFarad) { *pValue = pBjt->cjcFarad; return Result::Ok; }
      if (property == NumericProperty::ForwardTransitTimeSec) { *pValue = pBjt->forwardTransitTimeSec; return Result::Ok; }
      if (property == NumericProperty::ReverseTransitTimeSec) { *pValue = pBjt->reverseTransitTimeSec; return Result::Ok; }
   }
   if (const MosfetProperties* pMosfet = std::get_if<MosfetProperties>(&component.properties))
   {
      if (property == NumericProperty::ThresholdVoltageVolt) { *pValue = pMosfet->thresholdVoltageVolt; return Result::Ok; }
      if (property == NumericProperty::Kp) { *pValue = pMosfet->kpAmperePerVoltSquared; return Result::Ok; }
      if (property == NumericProperty::ChannelWidthMeter) { *pValue = pMosfet->channelWidthMeter; return Result::Ok; }
      if (property == NumericProperty::ChannelLengthMeter) { *pValue = pMosfet->channelLengthMeter; return Result::Ok; }
      if (property == NumericProperty::LambdaPerVolt) { *pValue = pMosfet->lambdaPerVolt; return Result::Ok; }
      if (property == NumericProperty::Cgso) { *pValue = pMosfet->cgsoFaradPerMeter; return Result::Ok; }
      if (property == NumericProperty::Cgdo) { *pValue = pMosfet->cgdoFaradPerMeter; return Result::Ok; }
   }
   if (const MeterProperties* pMeter = std::get_if<MeterProperties>(&component.properties))
   {
      if (property == NumericProperty::BurdenResistanceOhm) { *pValue = pMeter->burdenResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::BurdenInductanceHenry) { *pValue = pMeter->burdenInductanceHenry; return Result::Ok; }
      if (property == NumericProperty::InputResistanceOhm) { *pValue = pMeter->inputResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::InputCapacitanceFarad) { *pValue = pMeter->inputCapacitanceFarad; return Result::Ok; }
   }
   if (const LogicProperties* pLogic = ViewLogic(component))
   {
      if (property == NumericProperty::SupplyVoltageVolt) { *pValue = pLogic->supplyVoltageVolt; return Result::Ok; }
      if (property == NumericProperty::VohVolt) { *pValue = pLogic->vohVolt; return Result::Ok; }
      if (property == NumericProperty::VolVolt) { *pValue = pLogic->volVolt; return Result::Ok; }
      if (property == NumericProperty::VihVolt) { *pValue = pLogic->vihVolt; return Result::Ok; }
      if (property == NumericProperty::VilVolt) { *pValue = pLogic->vilVolt; return Result::Ok; }
      if (property == NumericProperty::PropagationDelaySec) { *pValue = pLogic->propagationDelaySec; return Result::Ok; }
      if (property == NumericProperty::RiseTimeSec) { *pValue = pLogic->riseTimeSec; return Result::Ok; }
      if (property == NumericProperty::FallTimeSec) { *pValue = pLogic->fallTimeSec; return Result::Ok; }
      if (property == NumericProperty::OutputResistanceOhm) { *pValue = pLogic->outputResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::InputCount) { *pValue = pLogic->inputCount; return Result::Ok; }
      if (property == NumericProperty::LowThresholdVolt) { *pValue = pLogic->lowThresholdVolt; return Result::Ok; }
      if (property == NumericProperty::HighThresholdVolt) { *pValue = pLogic->highThresholdVolt; return Result::Ok; }
   }
   if (const SequentialProperties* pSequential = std::get_if<SequentialProperties>(&component.properties))
   {
      if (property == NumericProperty::SetupTimeSec) { *pValue = pSequential->setupTimeSec; return Result::Ok; }
      if (property == NumericProperty::HoldTimeSec) { *pValue = pSequential->holdTimeSec; return Result::Ok; }
   }
   if (const ClockProperties* pClock = std::get_if<ClockProperties>(&component.properties))
   {
      if (property == NumericProperty::FrequencyHz) { *pValue = pClock->frequencyHz; return Result::Ok; }
      if (property == NumericProperty::DutyCycle) { *pValue = pClock->dutyCycle; return Result::Ok; }
      if (property == NumericProperty::DelaySec) { *pValue = pClock->delaySec; return Result::Ok; }
   }
   if (const SevenSegmentProperties* pDisplay = std::get_if<SevenSegmentProperties>(&component.properties))
   {
      if (property == NumericProperty::ForwardVoltageVolt) { *pValue = pDisplay->forwardVoltageVolt; return Result::Ok; }
      if (property == NumericProperty::ForwardCurrentAmpere) { *pValue = pDisplay->forwardCurrentAmpere; return Result::Ok; }
      if (property == NumericProperty::SegmentResistanceOhm) { *pValue = pDisplay->seriesResistanceOhm; return Result::Ok; }
   }
   if (const PullProperties* pPull = std::get_if<PullProperties>(&component.properties))
   {
      if (property == NumericProperty::ResistanceOhm) { *pValue = pPull->resistanceOhm; return Result::Ok; }
   }
   if (const SpstProperties* pSwitch = std::get_if<SpstProperties>(&component.properties))
   {
      if (property == NumericProperty::ClosedResistanceOhm) { *pValue = pSwitch->closedResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::OpenResistanceOhm) { *pValue = pSwitch->openResistanceOhm; return Result::Ok; }
   }
   if (const PushbuttonProperties* pButton = std::get_if<PushbuttonProperties>(&component.properties))
   {
      if (property == NumericProperty::ClosedResistanceOhm) { *pValue = pButton->closedResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::OpenResistanceOhm) { *pValue = pButton->openResistanceOhm; return Result::Ok; }
   }
   if (const SpdtProperties* pSpdt = std::get_if<SpdtProperties>(&component.properties))
   {
      if (property == NumericProperty::ClosedResistanceOhm) { *pValue = pSpdt->closedResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::OpenResistanceOhm) { *pValue = pSpdt->openResistanceOhm; return Result::Ok; }
   }
   if (const DipSwitchProperties* pDip = std::get_if<DipSwitchProperties>(&component.properties))
   {
      if (property == NumericProperty::ClosedResistanceOhm) { *pValue = pDip->closedResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::OpenResistanceOhm) { *pValue = pDip->openResistanceOhm; return Result::Ok; }
   }
   if (const Timer555Properties* pTimer = std::get_if<Timer555Properties>(&component.properties))
   {
      if (property == NumericProperty::OutputResistanceOhm) { *pValue = pTimer->outputResistanceOhm; return Result::Ok; }
      if (property == NumericProperty::PropagationDelaySec) { *pValue = pTimer->propagationDelaySec; return Result::Ok; }
   }
   if (const CounterProperties* pCounter = std::get_if<CounterProperties>(&component.properties))
   {
      if (property == NumericProperty::InitialValue) { *pValue = pCounter->initialValue; return Result::Ok; }
   }
   return Result::InvalidParameter;
}

Result WriteNumeric(Component* pComponent, NumericProperty property, double value)
{
   if (pComponent == nullptr || Belongs(value) == false)
   {
      return Result::InvalidParameter;
   }
   if (ResistorProperties* pResistor = std::get_if<ResistorProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::ResistanceOhm)
      {
         if (IsErr(RequirePositive(value))) return Result::InvalidParameter;
         pResistor->resistanceOhm = value;
         return Result::Ok;
      }
   }
   if (CapacitorProperties* pCapacitor = std::get_if<CapacitorProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::CapacitanceFarad)
      {
         if (IsErr(RequirePositive(value))) return Result::InvalidParameter;
         pCapacitor->capacitanceFarad = value;
         return Result::Ok;
      }
      if (property == NumericProperty::EsrOhm)
      {
         if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter;
         pCapacitor->esrOhm = value;
         return Result::Ok;
      }
      if (property == NumericProperty::InitialVoltageVolt)
      {
         pCapacitor->initialVoltageVolt = value;
         return Result::Ok;
      }
   }
   if (InductorProperties* pInductor = std::get_if<InductorProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::InductanceHenry)
      {
         if (IsErr(RequirePositive(value))) return Result::InvalidParameter;
         pInductor->inductanceHenry = value;
         return Result::Ok;
      }
      if (property == NumericProperty::SeriesResistanceOhm)
      {
         if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter;
         pInductor->seriesResistanceOhm = value;
         return Result::Ok;
      }
      if (property == NumericProperty::InitialCurrentAmpere)
      {
         pInductor->initialCurrentAmpere = value;
         return Result::Ok;
      }
   }
   if (PotentiometerProperties* pPotentiometer = std::get_if<PotentiometerProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::ResistanceOhm)
      {
         if (IsErr(RequirePositive(value))) return Result::InvalidParameter;
         pPotentiometer->resistanceOhm = value;
         return Result::Ok;
      }
      if (property == NumericProperty::WiperPosition)
      {
         if (value < 0.0 || value > 1.0) return Result::InvalidParameter;
         pPotentiometer->wiperPosition = value;
         return Result::Ok;
      }
   }
   if (VoltageSourceProperties* pSource = std::get_if<VoltageSourceProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::DcVoltageVolt) { pSource->dcVoltageVolt = value; return Result::Ok; }
      if (property == NumericProperty::AcAmplitudeVolt) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->acAmplitudeVolt = value; return Result::Ok; }
      if (property == NumericProperty::AcPhaseDeg) { pSource->acPhaseDeg = value; return Result::Ok; }
      if (property == NumericProperty::SineOffset) { pSource->sineOffsetVolt = value; return Result::Ok; }
      if (property == NumericProperty::SineAmplitude) { pSource->sineAmplitudeVolt = value; return Result::Ok; }
      if (property == NumericProperty::SineFrequencyHz) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pSource->sineFrequencyHz = value; return Result::Ok; }
      if (property == NumericProperty::SinePhaseDeg) { pSource->sinePhaseDeg = value; return Result::Ok; }
      if (property == NumericProperty::SineDelaySec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->sineDelaySec = value; return Result::Ok; }
      if (property == NumericProperty::PulseInitial) { pSource->pulseInitialVolt = value; return Result::Ok; }
      if (property == NumericProperty::PulseLevel) { pSource->pulseLevelVolt = value; return Result::Ok; }
      if (property == NumericProperty::PulseDelaySec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->pulseDelaySec = value; return Result::Ok; }
      if (property == NumericProperty::PulseRiseSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->pulseRiseSec = value; return Result::Ok; }
      if (property == NumericProperty::PulseFallSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->pulseFallSec = value; return Result::Ok; }
      if (property == NumericProperty::PulseWidthSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->pulseWidthSec = value; return Result::Ok; }
      if (property == NumericProperty::PulsePeriodSec) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pSource->pulsePeriodSec = value; return Result::Ok; }
   }
   if (CurrentSourceProperties* pSource = std::get_if<CurrentSourceProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::DcCurrentAmpere) { pSource->dcCurrentAmpere = value; return Result::Ok; }
      if (property == NumericProperty::AcAmplitudeAmpere) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->acAmplitudeAmpere = value; return Result::Ok; }
      if (property == NumericProperty::AcPhaseDeg) { pSource->acPhaseDeg = value; return Result::Ok; }
      if (property == NumericProperty::SineFrequencyHz) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pSource->sineFrequencyHz = value; return Result::Ok; }
      if (property == NumericProperty::SineAmplitude) { pSource->sineAmplitudeAmpere = value; return Result::Ok; }
      if (property == NumericProperty::SineOffset) { pSource->sineOffsetAmpere = value; return Result::Ok; }
      if (property == NumericProperty::SinePhaseDeg) { pSource->sinePhaseDeg = value; return Result::Ok; }
      if (property == NumericProperty::SineDelaySec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->sineDelaySec = value; return Result::Ok; }
      if (property == NumericProperty::PulseInitial) { pSource->pulseInitialAmpere = value; return Result::Ok; }
      if (property == NumericProperty::PulseLevel) { pSource->pulseLevelAmpere = value; return Result::Ok; }
      if (property == NumericProperty::PulseDelaySec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->pulseDelaySec = value; return Result::Ok; }
      if (property == NumericProperty::PulseRiseSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->pulseRiseSec = value; return Result::Ok; }
      if (property == NumericProperty::PulseFallSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->pulseFallSec = value; return Result::Ok; }
      if (property == NumericProperty::PulseWidthSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSource->pulseWidthSec = value; return Result::Ok; }
      if (property == NumericProperty::PulsePeriodSec) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pSource->pulsePeriodSec = value; return Result::Ok; }
   }
   if (DiodeProperties* pDiode = std::get_if<DiodeProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::ForwardVoltageVolt) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pDiode->forwardVoltageVolt = value; return Result::Ok; }
      if (property == NumericProperty::ForwardCurrentAmpere) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pDiode->forwardCurrentAmpere = value; return Result::Ok; }
      if (property == NumericProperty::EmissionCoefficient) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pDiode->emissionCoefficient = value; return Result::Ok; }
      if (property == NumericProperty::SeriesResistanceOhm) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pDiode->seriesResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::TemperatureCelsius) { if (value <= -ZeroCelsiusInKelvin) return Result::InvalidParameter; pDiode->temperatureCelsius = value; return Result::Ok; }
      if (property == NumericProperty::JunctionCapacitanceFarad) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pDiode->junctionCapacitanceFarad = value; return Result::Ok; }
      if (property == NumericProperty::JunctionPotentialVolt) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pDiode->junctionPotentialVolt = value; return Result::Ok; }
      if (property == NumericProperty::GradingCoefficient) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pDiode->gradingCoefficient = value; return Result::Ok; }
      if (property == NumericProperty::TransitTimeSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pDiode->transitTimeSec = value; return Result::Ok; }
   }
   if (LedProperties* pLed = std::get_if<LedProperties>(&pComponent->properties))
   {
      Component diodeEdit = *pComponent;
      diodeEdit.properties = pLed->diode;
      Result wrote = WriteNumeric(&diodeEdit, property, value);
      if (IsOk(wrote))
      {
         pLed->diode = std::get<DiodeProperties>(diodeEdit.properties);
      }
      return wrote;
   }
   if (BjtProperties* pBjt = std::get_if<BjtProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::ForwardBeta || property == NumericProperty::ReverseBeta)
      {
         if (IsErr(RequirePositive(value))) return Result::InvalidParameter;
         if (property == NumericProperty::ForwardBeta) pBjt->forwardBeta = value;
         else pBjt->reverseBeta = value;
         return Result::Ok;
      }
      if (property == NumericProperty::ForwardVoltageVolt) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pBjt->forwardVoltageVolt = value; return Result::Ok; }
      if (property == NumericProperty::ForwardCurrentAmpere) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pBjt->forwardCurrentAmpere = value; return Result::Ok; }
      if (property == NumericProperty::EmissionCoefficient) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pBjt->emissionCoefficient = value; return Result::Ok; }
      if (property == NumericProperty::TemperatureCelsius) { if (value <= -ZeroCelsiusInKelvin) return Result::InvalidParameter; pBjt->temperatureCelsius = value; return Result::Ok; }
      if (property == NumericProperty::EarlyVoltageVolt) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pBjt->earlyVoltageVolt = value; return Result::Ok; }
      if (property == NumericProperty::BaseResistanceOhm) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pBjt->baseResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::CollectorResistanceOhm) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pBjt->collectorResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::EmitterResistanceOhm) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pBjt->emitterResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::CjeFarad) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pBjt->cjeFarad = value; return Result::Ok; }
      if (property == NumericProperty::CjcFarad) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pBjt->cjcFarad = value; return Result::Ok; }
      if (property == NumericProperty::ForwardTransitTimeSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pBjt->forwardTransitTimeSec = value; return Result::Ok; }
      if (property == NumericProperty::ReverseTransitTimeSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pBjt->reverseTransitTimeSec = value; return Result::Ok; }
   }
   if (MosfetProperties* pMosfet = std::get_if<MosfetProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::ThresholdVoltageVolt) { pMosfet->thresholdVoltageVolt = value; return Result::Ok; }
      if (property == NumericProperty::Kp) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pMosfet->kpAmperePerVoltSquared = value; return Result::Ok; }
      if (property == NumericProperty::ChannelWidthMeter) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pMosfet->channelWidthMeter = value; return Result::Ok; }
      if (property == NumericProperty::ChannelLengthMeter) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pMosfet->channelLengthMeter = value; return Result::Ok; }
      if (property == NumericProperty::LambdaPerVolt) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pMosfet->lambdaPerVolt = value; return Result::Ok; }
      if (property == NumericProperty::Cgso) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pMosfet->cgsoFaradPerMeter = value; return Result::Ok; }
      if (property == NumericProperty::Cgdo) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pMosfet->cgdoFaradPerMeter = value; return Result::Ok; }
   }
   if (LogicProperties* pLogic = MutableLogic(pComponent))
   {
      if (property == NumericProperty::SupplyVoltageVolt)
      {
         if (IsErr(RequirePositive(value))) return Result::InvalidParameter;
         pLogic->supplyVoltageVolt = value;
         ApplyLogicFamily(pLogic, FamilySupplyReset::Keep);
         return Result::Ok;
      }
      if (property == NumericProperty::PropagationDelaySec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pLogic->propagationDelaySec = value; return Result::Ok; }
      if (property == NumericProperty::OutputResistanceOhm) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pLogic->outputResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::RiseTimeSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pLogic->riseTimeSec = value; return Result::Ok; }
      if (property == NumericProperty::FallTimeSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pLogic->fallTimeSec = value; return Result::Ok; }
      if (property == NumericProperty::VohVolt) { pLogic->vohVolt = value; return Result::Ok; }
      if (property == NumericProperty::VolVolt) { pLogic->volVolt = value; return Result::Ok; }
      if (property == NumericProperty::VihVolt) { pLogic->vihVolt = value; return Result::Ok; }
      if (property == NumericProperty::VilVolt) { pLogic->vilVolt = value; return Result::Ok; }
      if (property == NumericProperty::LowThresholdVolt) { pLogic->lowThresholdVolt = value; return Result::Ok; }
      if (property == NumericProperty::HighThresholdVolt) { pLogic->highThresholdVolt = value; return Result::Ok; }
      if (property == NumericProperty::InputCount)
      {
         if (value < FullInputCountMin || value > FullInputCountMax) return Result::InvalidParameter;
         pLogic->inputCount = static_cast<uint8_t>(value);
         return Result::Ok;
      }
   }
   if (SequentialProperties* pSequential = std::get_if<SequentialProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::SetupTimeSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSequential->setupTimeSec = value; return Result::Ok; }
      if (property == NumericProperty::HoldTimeSec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pSequential->holdTimeSec = value; return Result::Ok; }
   }
   if (ClockProperties* pClock = std::get_if<ClockProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::FrequencyHz) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pClock->frequencyHz = value; return Result::Ok; }
      if (property == NumericProperty::DutyCycle) { if (value <= 0.0 || value >= 1.0) return Result::InvalidParameter; pClock->dutyCycle = value; return Result::Ok; }
      if (property == NumericProperty::DelaySec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pClock->delaySec = value; return Result::Ok; }
   }
   if (MeterProperties* pMeter = std::get_if<MeterProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::BurdenResistanceOhm) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pMeter->burdenResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::BurdenInductanceHenry) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pMeter->burdenInductanceHenry = value; return Result::Ok; }
      if (property == NumericProperty::InputResistanceOhm) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pMeter->inputResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::InputCapacitanceFarad) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pMeter->inputCapacitanceFarad = value; return Result::Ok; }
   }
   if (SevenSegmentProperties* pDisplay = std::get_if<SevenSegmentProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::ForwardVoltageVolt) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pDisplay->forwardVoltageVolt = value; return Result::Ok; }
      if (property == NumericProperty::ForwardCurrentAmpere) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pDisplay->forwardCurrentAmpere = value; return Result::Ok; }
      if (property == NumericProperty::SegmentResistanceOhm) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pDisplay->seriesResistanceOhm = value; return Result::Ok; }
   }
   if (PullProperties* pPull = std::get_if<PullProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::ResistanceOhm) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pPull->resistanceOhm = value; return Result::Ok; }
   }
   if (SpstProperties* pSwitch = std::get_if<SpstProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::ClosedResistanceOhm) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pSwitch->closedResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::OpenResistanceOhm) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pSwitch->openResistanceOhm = value; return Result::Ok; }
   }
   if (PushbuttonProperties* pButton = std::get_if<PushbuttonProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::ClosedResistanceOhm) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pButton->closedResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::OpenResistanceOhm) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pButton->openResistanceOhm = value; return Result::Ok; }
   }
   if (SpdtProperties* pSpdt = std::get_if<SpdtProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::ClosedResistanceOhm) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pSpdt->closedResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::OpenResistanceOhm) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pSpdt->openResistanceOhm = value; return Result::Ok; }
   }
   if (DipSwitchProperties* pDip = std::get_if<DipSwitchProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::ClosedResistanceOhm) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pDip->closedResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::OpenResistanceOhm) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pDip->openResistanceOhm = value; return Result::Ok; }
   }
   if (Timer555Properties* pTimer = std::get_if<Timer555Properties>(&pComponent->properties))
   {
      if (property == NumericProperty::OutputResistanceOhm) { if (IsErr(RequirePositive(value))) return Result::InvalidParameter; pTimer->outputResistanceOhm = value; return Result::Ok; }
      if (property == NumericProperty::PropagationDelaySec) { if (IsErr(RequireNonNegative(value))) return Result::InvalidParameter; pTimer->propagationDelaySec = value; return Result::Ok; }
   }
   if (CounterProperties* pCounter = std::get_if<CounterProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::InitialValue)
      {
         if (value < 0.0 || value > CounterValueMax) return Result::InvalidParameter;
         pCounter->initialValue = static_cast<uint8_t>(value);
         return Result::Ok;
      }
   }
   if (ShiftRegisterProperties* pShift = std::get_if<ShiftRegisterProperties>(&pComponent->properties))
   {
      if (property == NumericProperty::InitialValue)
      {
         if (value < 0.0 || value > CounterValueMax) return Result::InvalidParameter;
         pShift->initialValue = static_cast<uint8_t>(value);
         return Result::Ok;
      }
   }
   return Result::InvalidParameter;
}

int32_t EnumCodeCount(EnumProperty property)
{
   switch (property)
   {
      case EnumProperty::TransientWaveform: return 3;
      case EnumProperty::LogicFamily: return 4;
      case EnumProperty::OutputStage: return 2;
      case EnumProperty::InitialState: return 4;
      case EnumProperty::ClockEdge: return 2;
      case EnumProperty::PresetPolarity:
      case EnumProperty::ClearPolarity:
      case EnumProperty::SrPolarity:
      case EnumProperty::EnablePolarity:
      case EnumProperty::OutputPolarity: return 2;
      case EnumProperty::StartLevel: return 2;
      case EnumProperty::MeterMode: return 2;
      case EnumProperty::ClosedMode:
      case EnumProperty::OpenMode: return 2;
      case EnumProperty::RestState:
      case EnumProperty::MechanicalState: return 2;
      case EnumProperty::RestThrow: return 2;
      case EnumProperty::ContactForm: return 2;
      case EnumProperty::LedColor: return 5;
      case EnumProperty::SegmentCommon: return 2;
      case EnumProperty::AdcConversion: return 2;
      case EnumProperty::MuxWidth: return 2;
      default: break;
   }
   return 0;
}

std::string EnumCodeLabel(EnumProperty property, int32_t code)
{
   if (property == EnumProperty::TransientWaveform)
   {
      if (code == 0) return "DC";
      if (code == 1) return "Sine";
      if (code == 2) return "Pulse";
   }
   if (property == EnumProperty::LogicFamily)
   {
      if (code == 0) return "TTL";
      if (code == 1) return "CMOS 5 V";
      if (code == 2) return "CMOS 3.3 V";
      if (code == 3) return "Custom";
   }
   if (property == EnumProperty::OutputStage)
   {
      if (code == 0) return "Push-pull";
      if (code == 1) return "Open-drain";
   }
   if (property == EnumProperty::InitialState || property == EnumProperty::StartLevel)
   {
      if (code == 0) return "Low";
      if (code == 1) return "High";
      if (code == 2) return "High-Z";
      if (code == 3) return "Conflict";
   }
   if (property == EnumProperty::ClockEdge)
   {
      if (code == 0) return "Rising";
      if (code == 1) return "Falling";
   }
   if (property == EnumProperty::MeterMode || property == EnumProperty::ClosedMode || property == EnumProperty::OpenMode)
   {
      if (code == 0) return "Ideal";
      if (code == 1) return "Non-ideal";
   }
   if (property == EnumProperty::RestState || property == EnumProperty::MechanicalState)
   {
      if (code == 0) return "Open";
      if (code == 1) return "Closed";
   }
   if (property == EnumProperty::RestThrow)
   {
      if (code == 0) return "A";
      if (code == 1) return "B";
   }
   if (property == EnumProperty::ContactForm)
   {
      if (code == 0) return "Normally open";
      if (code == 1) return "Normally closed";
   }
   if (property == EnumProperty::LedColor)
   {
      if (code == 0) return "Red";
      if (code == 1) return "Green";
      if (code == 2) return "Yellow";
      if (code == 3) return "Blue";
      if (code == 4) return "White";
   }
   if (property == EnumProperty::SegmentCommon)
   {
      if (code == 0) return "Common cathode";
      if (code == 1) return "Common anode";
   }
   if (property == EnumProperty::AdcConversion)
   {
      if (code == 0) return "Clocked";
      if (code == 1) return "Continuous";
   }
   if (property == EnumProperty::MuxWidth)
   {
      if (code == 0) return "2:1";
      if (code == 1) return "4:1";
   }
   if (code == 0) return "Active high";
   if (code == 1) return "Active low";
   return "Unknown";
}

Result ReadEnumCode(const Component& component, EnumProperty property, int32_t* pCode)
{
   if (pCode == nullptr)
   {
      return Result::InvalidParameter;
   }
   if (const VoltageSourceProperties* pSource = std::get_if<VoltageSourceProperties>(&component.properties))
   {
      if (property == EnumProperty::TransientWaveform)
      {
         *pCode = static_cast<int32_t>(pSource->transientWaveform);
         return Result::Ok;
      }
   }
   if (const CurrentSourceProperties* pSource = std::get_if<CurrentSourceProperties>(&component.properties))
   {
      if (property == EnumProperty::TransientWaveform)
      {
         *pCode = static_cast<int32_t>(pSource->transientWaveform);
         return Result::Ok;
      }
   }
   if (const LogicProperties* pLogic = ViewLogic(component))
   {
      if (property == EnumProperty::LogicFamily) { *pCode = static_cast<int32_t>(pLogic->family); return Result::Ok; }
      if (property == EnumProperty::OutputStage) { *pCode = static_cast<int32_t>(pLogic->outputStage); return Result::Ok; }
      if (property == EnumProperty::EnablePolarity) { *pCode = static_cast<int32_t>(pLogic->enablePolarity); return Result::Ok; }
   }
   if (const SequentialProperties* pSequential = std::get_if<SequentialProperties>(&component.properties))
   {
      if (property == EnumProperty::InitialState) { *pCode = static_cast<int32_t>(pSequential->initialState); return Result::Ok; }
      if (property == EnumProperty::ClockEdge) { *pCode = static_cast<int32_t>(pSequential->clockEdge); return Result::Ok; }
      if (property == EnumProperty::PresetPolarity) { *pCode = static_cast<int32_t>(pSequential->presetPolarity); return Result::Ok; }
      if (property == EnumProperty::ClearPolarity) { *pCode = static_cast<int32_t>(pSequential->clearPolarity); return Result::Ok; }
      if (property == EnumProperty::SrPolarity) { *pCode = static_cast<int32_t>(pSequential->srPolarity); return Result::Ok; }
      if (property == EnumProperty::EnablePolarity) { *pCode = static_cast<int32_t>(pSequential->enablePolarity); return Result::Ok; }
   }
   if (const MeterProperties* pMeter = std::get_if<MeterProperties>(&component.properties))
   {
      if (property == EnumProperty::MeterMode) { *pCode = static_cast<int32_t>(pMeter->mode); return Result::Ok; }
   }
   if (const SpstProperties* pSwitch = std::get_if<SpstProperties>(&component.properties))
   {
      if (property == EnumProperty::RestState) { *pCode = static_cast<int32_t>(pSwitch->restState); return Result::Ok; }
      if (property == EnumProperty::ClosedMode) { *pCode = static_cast<int32_t>(pSwitch->closedMode); return Result::Ok; }
      if (property == EnumProperty::OpenMode) { *pCode = static_cast<int32_t>(pSwitch->openMode); return Result::Ok; }
   }
   if (const SpdtProperties* pSpdt = std::get_if<SpdtProperties>(&component.properties))
   {
      if (property == EnumProperty::RestThrow) { *pCode = static_cast<int32_t>(pSpdt->restPosition); return Result::Ok; }
      if (property == EnumProperty::ClosedMode) { *pCode = static_cast<int32_t>(pSpdt->closedMode); return Result::Ok; }
      if (property == EnumProperty::OpenMode) { *pCode = static_cast<int32_t>(pSpdt->openMode); return Result::Ok; }
   }
   if (const DipSwitchProperties* pDip = std::get_if<DipSwitchProperties>(&component.properties))
   {
      if (property == EnumProperty::ClosedMode) { *pCode = static_cast<int32_t>(pDip->closedMode); return Result::Ok; }
      if (property == EnumProperty::OpenMode) { *pCode = static_cast<int32_t>(pDip->openMode); return Result::Ok; }
   }
   if (const ClockProperties* pClock = std::get_if<ClockProperties>(&component.properties))
   {
      if (property == EnumProperty::StartLevel) { *pCode = static_cast<int32_t>(pClock->startLevel); return Result::Ok; }
   }
   if (const LedProperties* pLed = std::get_if<LedProperties>(&component.properties))
   {
      if (property == EnumProperty::LedColor) { *pCode = static_cast<int32_t>(pLed->color); return Result::Ok; }
   }
   if (const SevenSegmentProperties* pDisplay = std::get_if<SevenSegmentProperties>(&component.properties))
   {
      if (property == EnumProperty::SegmentCommon) { *pCode = static_cast<int32_t>(pDisplay->common); return Result::Ok; }
      if (property == EnumProperty::LedColor) { *pCode = static_cast<int32_t>(pDisplay->color); return Result::Ok; }
   }
   if (const MuxProperties* pMux = std::get_if<MuxProperties>(&component.properties))
   {
      if (property == EnumProperty::MuxWidth) { *pCode = pMux->width == MuxWidth::Four ? 1 : 0; return Result::Ok; }
   }
   return Result::InvalidParameter;
}

Result WriteEnumCode(Component* pComponent, EnumProperty property, int32_t code)
{
   if (pComponent == nullptr || code < 0 || code >= EnumCodeCount(property))
   {
      return Result::InvalidParameter;
   }
   if (VoltageSourceProperties* pSource = std::get_if<VoltageSourceProperties>(&pComponent->properties))
   {
      if (property == EnumProperty::TransientWaveform)
      {
         pSource->transientWaveform = static_cast<SourceWaveform>(code);
         return Result::Ok;
      }
   }
   if (CurrentSourceProperties* pSource = std::get_if<CurrentSourceProperties>(&pComponent->properties))
   {
      if (property == EnumProperty::TransientWaveform)
      {
         pSource->transientWaveform = static_cast<SourceWaveform>(code);
         return Result::Ok;
      }
   }
   if (LogicProperties* pLogic = MutableLogic(pComponent))
   {
      if (property == EnumProperty::LogicFamily)
      {
         pLogic->family = static_cast<LogicFamily>(code);
         ApplyLogicFamily(pLogic, FamilySupplyReset::FamilyDefault);
         return Result::Ok;
      }
      if (property == EnumProperty::OutputStage)
      {
         pLogic->outputStage = static_cast<OutputStage>(code);
         return Result::Ok;
      }
      if (property == EnumProperty::EnablePolarity)
      {
         pLogic->enablePolarity = static_cast<Polarity>(code);
         return Result::Ok;
      }
   }
   if (SequentialProperties* pSequential = std::get_if<SequentialProperties>(&pComponent->properties))
   {
      if (property == EnumProperty::InitialState) { pSequential->initialState = static_cast<LogicLevel>(code); return Result::Ok; }
      if (property == EnumProperty::ClockEdge) { pSequential->clockEdge = static_cast<ClockEdge>(code); return Result::Ok; }
      if (property == EnumProperty::PresetPolarity) { pSequential->presetPolarity = static_cast<Polarity>(code); return Result::Ok; }
      if (property == EnumProperty::ClearPolarity) { pSequential->clearPolarity = static_cast<Polarity>(code); return Result::Ok; }
      if (property == EnumProperty::SrPolarity) { pSequential->srPolarity = static_cast<Polarity>(code); return Result::Ok; }
      if (property == EnumProperty::EnablePolarity) { pSequential->enablePolarity = static_cast<Polarity>(code); return Result::Ok; }
   }
   if (MeterProperties* pMeter = std::get_if<MeterProperties>(&pComponent->properties))
   {
      if (property == EnumProperty::MeterMode) { pMeter->mode = static_cast<MeterMode>(code); return Result::Ok; }
   }
   if (SpstProperties* pSwitch = std::get_if<SpstProperties>(&pComponent->properties))
   {
      if (property == EnumProperty::RestState) { pSwitch->restState = static_cast<SwitchState>(code); return Result::Ok; }
      if (property == EnumProperty::ClosedMode) { pSwitch->closedMode = static_cast<ResistanceMode>(code); return Result::Ok; }
      if (property == EnumProperty::OpenMode) { pSwitch->openMode = static_cast<ResistanceMode>(code); return Result::Ok; }
   }
   if (SpdtProperties* pSpdt = std::get_if<SpdtProperties>(&pComponent->properties))
   {
      if (property == EnumProperty::RestThrow) { pSpdt->restPosition = static_cast<ThrowPosition>(code); return Result::Ok; }
      if (property == EnumProperty::ClosedMode) { pSpdt->closedMode = static_cast<ResistanceMode>(code); return Result::Ok; }
      if (property == EnumProperty::OpenMode) { pSpdt->openMode = static_cast<ResistanceMode>(code); return Result::Ok; }
   }
   if (DipSwitchProperties* pDip = std::get_if<DipSwitchProperties>(&pComponent->properties))
   {
      if (property == EnumProperty::ClosedMode) { pDip->closedMode = static_cast<ResistanceMode>(code); return Result::Ok; }
      if (property == EnumProperty::OpenMode) { pDip->openMode = static_cast<ResistanceMode>(code); return Result::Ok; }
   }
   if (ClockProperties* pClock = std::get_if<ClockProperties>(&pComponent->properties))
   {
      if (property == EnumProperty::StartLevel)
      {
         pClock->startLevel = code == 0 ? LogicLevel::Low : LogicLevel::High;
         return Result::Ok;
      }
   }
   if (LedProperties* pLed = std::get_if<LedProperties>(&pComponent->properties))
   {
      if (property == EnumProperty::LedColor) { pLed->color = static_cast<LedColor>(code); return Result::Ok; }
   }
   if (SevenSegmentProperties* pDisplay = std::get_if<SevenSegmentProperties>(&pComponent->properties))
   {
      if (property == EnumProperty::LedColor) { pDisplay->color = static_cast<LedColor>(code); return Result::Ok; }
      if (property == EnumProperty::SegmentCommon) { pDisplay->common = static_cast<SegmentCommon>(code); return Result::Ok; }
   }
   if (MuxProperties* pMux = std::get_if<MuxProperties>(&pComponent->properties))
   {
      if (property == EnumProperty::MuxWidth)
      {
         pMux->width = code == 1 ? MuxWidth::Four : MuxWidth::Two;
         return Result::Ok;
      }
   }
   return Result::InvalidParameter;
}

} // namespace Analyzer
