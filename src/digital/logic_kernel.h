/*!
 * \file logic_kernel.h
 * \brief Event-driven logic for gates, latches, flip-flops, and clocks.
 */

#ifndef LOGIC_KERNEL_H
#define LOGIC_KERNEL_H

#include "core/analysis_settings.h"
#include "core/cancel_token.h"
#include "core/netlist.h"
#include "solver/analysis_result.h"

namespace Analyzer
{

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
