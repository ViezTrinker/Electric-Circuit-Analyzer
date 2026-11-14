/*!
 * \file logic_kernel.h
 * \brief Event-driven logic for gates, latches, flip-flops, and clocks.
 */

#ifndef LOGIC_KERNEL_H
#define LOGIC_KERNEL_H

#include <vector>

#include "core/analysis_settings.h"
#include "core/cancel_token.h"
#include "core/netlist.h"
#include "solver/analysis_result.h"

namespace Analyzer
{

using HoldMask = std::vector<uint8_t>;

/*!
 * \brief Settles combinational logic and sequential initial state.
 *
 * Clock outputs sit at their time-zero level. Flip-flops do not see an edge.
 * A combinational loop that is still changing after the delta limit returns
 * Oscillation. Setup and hold are not checked here.
 *
 * \param[in] netlist Compiled circuit.
 * \param[out] pResult DC result whose logic levels are filled. Voltages already
 * stored in the result are left in place.
 */
Result SolveDigitalDc(const Netlist& netlist, DcResult* pResult);

/*!
 * \brief Settles logic while selected nets keep a seeded level.
 *
 * A held net is an analog net whose level comes from its voltage. Resolve
 * does not replace that level with a digital driver.
 *
 * \param[in] netlist Compiled circuit.
 * \param[in] seededLevels Level to keep on each held net.
 * \param[in] holdMask Nonzero entries are held. Sized to the net count.
 * \param[out] pResult DC result whose logic levels are filled.
 */
Result SolveDigitalDcHeld(const Netlist& netlist, const LogicLevelList& seededLevels, const HoldMask& holdMask, DcResult* pResult, const VoltageList* pAnalogVoltage = nullptr);

/*!
 * \brief Settles logic and returns the net levels without publishing branches.
 *
 * \param[in] netlist Compiled circuit.
 * \param[in] seededLevels Level to keep on each held net.
 * \param[in] holdMask Nonzero entries are held.
 * \param[out] pLevels Settled level of every net.
 */
Result ReadSettledLevels(const Netlist& netlist, const LogicLevelList& seededLevels, const HoldMask& holdMask, LogicLevelList* pLevels, const VoltageList* pAnalogVoltage = nullptr);

/*!
 * \brief Runs a pure logic transient from the settled operating point.
 *
 * Outputs change at the propagation delay, and a delay of zero takes one
 * delta cycle. The run stops at the next digital event boundary only; analog
 * integration is not part of this function.
 *
 * \param[in] netlist Compiled circuit.
 * \param[in] settings Stop time comes from the transient settings.
 * \param[in] pCancel Optional cancel flag. May be null.
 * \param[out] pResult Samples and the initial operating point.
 */
Result SolveDigitalTransient(const Netlist& netlist, const AnalysisSettings& settings, CancelToken* pCancel, TransientResult* pResult);

} // namespace Analyzer

#endif // LOGIC_KERNEL_H
