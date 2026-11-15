/*!
 * \file device_model.cpp
 * \brief Diode, BJT, and MOSFET evaluations used by Newton stamps.
 */

#include "device_model.h"

#include <algorithm>
#include <cmath>

namespace Analyzer
{

namespace
{

constexpr double MaximumExponent = 140.0;
constexpr double MaximumJunctionConductance = 1.0e9;
constexpr double SmoothWindowVolt = 0.001;
constexpr double ForwardCapacitanceFraction = 0.5;

double SafeExp(double argument)
{
   if (argument > MaximumExponent)
   {
      return std::exp(MaximumExponent);
   }
   if (argument < -MaximumExponent)
   {
      return std::exp(-MaximumExponent);
   }
   return std::exp(argument);
}

} // namespace

double JunctionCapacitance(double zeroBiasFarad, double potentialVolt, double gradingCoefficient, double junctionVoltageVolt)
{
   if (zeroBiasFarad <= 0.0 || potentialVolt <= 0.0 || gradingCoefficient <= 0.0 || std::isfinite(junctionVoltageVolt) == false)
   {
      return 0.0;
   }
   double barrierVolt = ForwardCapacitanceFraction * potentialVolt;
   if (junctionVoltageVolt < barrierVolt)
   {
      double ratio = 1.0 - (junctionVoltageVolt / potentialVolt);
      if (ratio <= 0.0)
      {
         ratio = 1.0 - ForwardCapacitanceFraction;
      }
      return zeroBiasFarad / std::pow(ratio, gradingCoefficient);
   }
   double remaining = 1.0 - ForwardCapacitanceFraction;
   double continued = zeroBiasFarad / std::pow(remaining, gradingCoefficient);
   double slope = gradingCoefficient / (potentialVolt * remaining);
   return continued * (1.0 + (slope * (junctionVoltageVolt - barrierVolt)));
}

double LimitJunctionStep(double proposedVolt, double previousVolt, double thermalVolt)
{
   double limit = thermalVolt + thermalVolt;
   if (limit <= 0.0)
   {
      return proposedVolt;
   }
   double step = proposedVolt - previousVolt;
   if (step > limit)
   {
      return previousVolt + limit;
   }
   if (step < -limit)
   {
      return previousVolt - limit;
   }
   return proposedVolt;
}

DiodeLinearization EvaluateDiode(const DiodeProperties& properties, double junctionVoltageVolt)
{
   DiodeLinearization linearization = {};
   double saturation = SaturationCurrent(properties.forwardVoltageVolt, properties.forwardCurrentAmpere, properties.emissionCoefficient, properties.temperatureCelsius);
   double thermal = properties.emissionCoefficient * ThermalVoltage(properties.temperatureCelsius);
   if (saturation <= 0.0 || thermal <= 0.0)
   {
      return linearization;
   }
   double argument = junctionVoltageVolt / thermal;
   double criticalArgument = std::log(thermal / (std::sqrt(2.0) * saturation));
   if (argument > criticalArgument)
   {
      argument = criticalArgument;
   }
   double exponential = SafeExp(argument);
   linearization.currentAmpere = saturation * (exponential - 1.0);
   linearization.conductanceSiemens = saturation * exponential / thermal;
   if (linearization.conductanceSiemens > MaximumJunctionConductance)
   {
      linearization.conductanceSiemens = MaximumJunctionConductance;
   }
   return linearization;
}

BjtLinearization EvaluateBjt(const BjtProperties& properties, double baseEmitterVolt, double baseCollectorVolt)
{
   BjtLinearization linearization = {};
   double saturation = SaturationCurrent(properties.forwardVoltageVolt, properties.forwardCurrentAmpere, properties.emissionCoefficient, properties.temperatureCelsius);
   double thermal = properties.emissionCoefficient * ThermalVoltage(properties.temperatureCelsius);
   if (saturation <= 0.0 || thermal <= 0.0 || properties.forwardBeta <= 0.0 || properties.reverseBeta <= 0.0)
   {
      return linearization;
   }
   double forwardExponent = SafeExp(baseEmitterVolt / thermal);
   double reverseExponent = SafeExp(baseCollectorVolt / thermal);
   double forwardCurrent = saturation * (forwardExponent - 1.0);
   double reverseCurrent = saturation * (reverseExponent - 1.0);
   double forwardSlope = saturation * forwardExponent / thermal;
   double reverseSlope = saturation * reverseExponent / thermal;
   if (properties.earlyVoltageVolt > 0.0)
   {
      double collectorEmitter = baseEmitterVolt - baseCollectorVolt;
      double early = 1.0 + (collectorEmitter / properties.earlyVoltageVolt);
      forwardCurrent *= early;
      forwardSlope *= early;
   }
   linearization.baseCurrent = (forwardCurrent / properties.forwardBeta) + (reverseCurrent / properties.reverseBeta);
   linearization.collectorCurrent = forwardCurrent - (reverseCurrent * (1.0 + (1.0 / properties.reverseBeta)));
   linearization.baseEmitterConductance = forwardSlope / properties.forwardBeta;
   linearization.baseCollectorConductance = reverseSlope / properties.reverseBeta;
   linearization.collectorEmitterTransconductance = forwardSlope;
   linearization.collectorCollectorConductance = -reverseSlope * (1.0 + (1.0 / properties.reverseBeta));
   return linearization;
}

MosfetLinearization EvaluateMosfet(ComponentKind channelKind, const MosfetProperties& properties, double gateSourceVolt, double drainSourceVolt)
{
   MosfetLinearization linearization = {};
   if (properties.channelLengthMeter <= 0.0)
   {
      return linearization;
   }
   double polarity = channelKind == ComponentKind::Pmos ? -1.0 : 1.0;
   double modelGate = polarity * gateSourceVolt;
   double modelDrain = polarity * drainSourceVolt;
   double modelThreshold = polarity * properties.thresholdVoltageVolt;
   double process = properties.kpAmperePerVoltSquared * properties.channelWidthMeter / properties.channelLengthMeter;
   double overdrive = modelGate - modelThreshold;
   double modelCurrent = 0.0;
   double modelGm = 0.0;
   double modelGds = 0.0;
   if (overdrive > 0.0)
   {
      double boundary = overdrive;
      bool saturation = modelDrain >= boundary;
      bool triode = modelDrain < boundary;
      if (std::abs(modelDrain - boundary) < SmoothWindowVolt)
      {
         saturation = false;
         triode = false;
         double blend = (modelDrain - (boundary - SmoothWindowVolt)) / (SmoothWindowVolt + SmoothWindowVolt);
         blend = std::clamp(blend, 0.0, 1.0);
         double triodeCurrent = process * ((overdrive * modelDrain) - (0.5 * modelDrain * modelDrain));
         double saturationCurrent = 0.5 * process * overdrive * overdrive;
         modelCurrent = ((1.0 - blend) * triodeCurrent) + (blend * saturationCurrent);
         double triodeGm = process * modelDrain;
         double triodeGds = process * (overdrive - modelDrain);
         double saturationGm = process * overdrive;
         modelGm = ((1.0 - blend) * triodeGm) + (blend * saturationGm);
         modelGds = (1.0 - blend) * triodeGds;
      }
      else if (triode)
      {
         modelCurrent = process * ((overdrive * modelDrain) - (0.5 * modelDrain * modelDrain));
         modelGm = process * modelDrain;
         modelGds = process * (overdrive - modelDrain);
      }
      else if (saturation)
      {
         modelCurrent = 0.5 * process * overdrive * overdrive;
         modelGm = process * overdrive;
         modelGds = 0.0;
      }
      if (properties.lambdaPerVolt != 0.0)
      {
         double early = 1.0 + (properties.lambdaPerVolt * modelDrain);
         modelCurrent *= early;
         modelGm *= early;
         modelGds = (modelGds * early) + (modelCurrent * properties.lambdaPerVolt / early);
      }
   }
   linearization.drainCurrent = polarity * modelCurrent;
   linearization.gateSourceTransconductance = modelGm;
   linearization.drainSourceConductance = modelGds;
   return linearization;
}

} // namespace Analyzer
