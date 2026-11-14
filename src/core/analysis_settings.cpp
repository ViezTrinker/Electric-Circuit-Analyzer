/*!
 * \file analysis_settings.cpp
 * \brief Default analysis settings.
 */

#include "analysis_settings.h"

namespace Analyzer
{

namespace
{

constexpr double DefaultGminSiemens = 1.0e-12;
constexpr double DefaultSweepStartHz = 10.0;
constexpr double DefaultSweepStopHz = 1000000.0;
constexpr int32_t DefaultPointsPerDecade = 20;
constexpr int32_t DefaultLinearPointCount = 50;
constexpr double DefaultStopTimeSec = 0.001;
constexpr double DefaultMaximumStepSec = 1.0e-6;

} // namespace

AnalysisSettings DefaultAnalysisSettings(void)
{
   AnalysisSettings settings = {};
   settings.kind = AnalysisKind::DcOperatingPoint;
   settings.gminSiemens = DefaultGminSiemens;
   settings.updateDcOnEdit = UpdateDcOnEdit::Yes;
   settings.sweepScale = SweepScale::Logarithmic;
   settings.sweepStartHz = DefaultSweepStartHz;
   settings.sweepStopHz = DefaultSweepStopHz;
   settings.pointsPerDecade = DefaultPointsPerDecade;
   settings.linearPointCount = DefaultLinearPointCount;
   settings.stopTimeSec = DefaultStopTimeSec;
   settings.maximumStepSec = DefaultMaximumStepSec;
   settings.integration = IntegrationMethod::BackwardEuler;
   settings.initialConditions = InitialConditionMode::OperatingPoint;
   return settings;
}

} // namespace Analyzer
