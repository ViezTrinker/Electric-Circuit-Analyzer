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
 * \brief Integrates the analog circuit in time.
 *
 * \param[in] netlist Compiled circuit.
 * \param[in] settings Transient settings.
 * \param[in] pCancel Optional cancel flag. May be null.
 */
TransientResult SolveTransientAnalog(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel);

} // namespace Analyzer

#endif // ANALOG_SOLVER_H
