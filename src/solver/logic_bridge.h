/*!
 * \file logic_bridge.h
 * \brief DC operating point that couples logic outputs to the analog matrix.
 */

#ifndef LOGIC_BRIDGE_H
#define LOGIC_BRIDGE_H

#include "analysis_result.h"
#include "core/analysis_settings.h"
#include "core/cancel_token.h"
#include "core/netlist.h"
#include "logic_drive.h"

namespace Analyzer
{

/*!
 * \brief Solves analog and digital DC, repeating while mixed nets change.
 *
 * A logic input on an analog net reads Vil and Vih. A logic output on an
 * analog net is a voltage behind outputResistanceOhm. Pure digital nets stay
 * out of the matrix. Pull-up and pull-down parts on a mixed net are not yet
 * stamped as resistors.
 *
 * \param[in] netlist Compiled circuit.
 * \param[in] settings Operating-point settings.
 * \param[in] pCancel Optional cancel flag. May be null.
 */
DcResult SolveOperatingPoint(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel);

/*!
 * \brief Builds the frozen drives described by settled logic levels.
 *
 * \param[in] netlist Compiled circuit.
 * \param[in] levels Settled level of every net.
 * \param[out] pDrives One drive per mixed output net. May be empty.
 */
Result CollectOperatingDrives(const Netlist& netlist, const LogicLevelList& levels, LogicDriveList* pDrives);

/*!
 * \brief True when the netlist contains a gate, clock, or other logic device.
 *
 * \param[in] netlist Compiled circuit.
 */
bool HasLogicDevice(const Netlist& netlist);

/*!
 * \brief Integrates a circuit that has both analog parts and logic.
 *
 * The analog step ends at the next logic event. Drives are then rebuilt.
 * With no capacitor, inductor, or time-varying source, the solver jumps to
 * that event and solves a new operating point.
 *
 * \param[in] netlist Compiled circuit.
 * \param[in] settings Transient settings.
 * \param[in] pCancel Optional cancel flag. May be null.
 */
TransientResult SolveMixedTransient(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel);

} // namespace Analyzer

#endif // LOGIC_BRIDGE_H
