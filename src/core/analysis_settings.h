/*!
 * \file analysis_settings.h
 * \brief User settings for the DC, AC, and transient analyses.
 */

#ifndef ANALYSIS_SETTINGS_H
#define ANALYSIS_SETTINGS_H

#include <cstdint>

namespace Analyzer
{

/*!
 * \brief Which analysis the Run action starts.
 */
enum class AnalysisKind : int8_t
{
   DcOperatingPoint = 0,
   AcSweep = 1,
   Transient = 2
};

/*!
 * \brief Spacing of an AC sweep.
 */
enum class SweepScale : int8_t
{
   Logarithmic = 0,
   Linear = 1
};

/*!
 * \brief Integration method for the transient analysis.
 */
enum class IntegrationMethod : int8_t
{
   BackwardEuler = 0,
   Trapezoidal = 1
};

/*!
 * \brief How a transient run gets its initial capacitor and inductor state.
 */
enum class InitialConditionMode : int8_t
{
   OperatingPoint = 0,
   ComponentValues = 1
};

/*!
 * \brief Whether an edit recomputes the DC operating point.
 */
enum class UpdateDcOnEdit : int8_t
{
   No = 0,
   Yes = 1
};

/*!
 * \brief Saved with the circuit and edited in the analysis panel.
 */
struct AnalysisSettings
{
   AnalysisKind kind;
   double gminSiemens;
   UpdateDcOnEdit updateDcOnEdit;

   SweepScale sweepScale;
   double sweepStartHz;
   double sweepStopHz;
   int32_t pointsPerDecade;
   int32_t linearPointCount;

   double stopTimeSec;
   double maximumStepSec;
   IntegrationMethod integration;
   InitialConditionMode initialConditions;
};

/*!
 * \brief Settings used for a new circuit.
 */
AnalysisSettings DefaultAnalysisSettings(void);

} // namespace Analyzer

#endif // ANALYSIS_SETTINGS_H
