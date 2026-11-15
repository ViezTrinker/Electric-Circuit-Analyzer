/*!
 * \file analog_solver.h
 * \brief DC, AC, and transient modified nodal analysis.
 */

#ifndef ANALOG_SOLVER_H
#define ANALOG_SOLVER_H

#include "analysis_result.h"
#include "core/analysis_settings.h"
#include "core/cancel_token.h"
#include "core/netlist.h"
#include "logic_drive.h"

namespace Analyzer
{

/*!
 * \brief Solves the DC operating point of the analog devices in a netlist.
 *
 * \param[in] netlist Compiled circuit.
 * \param[in] settings Analysis settings, including gmin.
 * \param[in] pCancel Optional cancel flag. May be null.
 * \param[in] pDrives Optional logic Thevenin drives. May be null.
 */
DcResult SolveDcAnalog(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel, const LogicDriveList* pDrives = nullptr);

/*!
 * \brief Solves a small-signal frequency sweep around a DC bias.
 *
 * \param[in] netlist Compiled circuit.
 * \param[in] settings Sweep settings.
 * \param[in] pCancel Optional cancel flag. May be null.
 * \param[in] pDrives Optional frozen logic drives. May be null. The sweep stamps only their resistance.
 * \param[in] pBias Optional operating point to reuse. May be null.
 */
AcResult SolveAcAnalog(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel, const LogicDriveList* pDrives = nullptr, const DcResult* pBias = nullptr);

/*!
 * \brief Capacitor, inductor, and timer state carried from one transient slice to the next.
 */
struct TransientContinuity
{
   double timeSec;
   double stepSec;
   VoltageList voltage;
   std::vector<double> inductorCurrent;
   std::vector<uint8_t> timerLevel;
   std::vector<double> timerChangeAt;
   uint32_t acceptedSteps;
   bool ready;
};

/*!
 * \brief Integrates the analog circuit in time.
 *
 * Each step Newton-linearizes diodes, BJTs, and MOSFETs. Junction capacitance
 * is not stamped. A negative horizon uses the stop time in the settings. A ready continuity
 * record continues a previous slice instead of solving a new operating point.
 * Logic drives stay constant for the whole call.
 *
 * \param[in] netlist Compiled circuit.
 * \param[in] settings Transient settings.
 * \param[in] pCancel Optional cancel flag. May be null.
 * \param[in] pDrives Optional logic Thevenin drives. May be null.
 * \param[in] horizonSec Last time included in this slice. Negative means the settings stop time.
 * \param[in,out] pContinuity Optional state carried across slices. May be null.
 */
TransientResult SolveTransientAnalog(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel, const LogicDriveList* pDrives = nullptr, double horizonSec = -1.0, TransientContinuity* pContinuity = nullptr);

} // namespace Analyzer

#endif // ANALOG_SOLVER_H
