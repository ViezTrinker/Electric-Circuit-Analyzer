/*!
 * \file simulator.h
 * \brief Runs a saved circuit through DC, AC, or transient analysis.
 */

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "analysis_result.h"
#include "core/cancel_token.h"
#include "core/circuit.h"

namespace Analyzer
{

/*!
 * \brief DC operating point of a schematic.
 *
 * \param[in] circuit Schematic to solve.
 * \param[in] pCancel Optional cancel flag. May be null.
 */
DcResult SolveDc(const Circuit& circuit, CancelToken* pCancel);

/*!
 * \brief AC sweep of a schematic.
 *
 * \param[in] circuit Schematic to solve.
 * \param[in] pCancel Optional cancel flag. May be null.
 */
AcResult SolveAc(const Circuit& circuit, CancelToken* pCancel);

/*!
 * \brief Transient run of a schematic.
 *
 * \param[in] circuit Schematic to solve.
 * \param[in] pCancel Optional cancel flag. May be null.
 */
TransientResult SolveTransient(const Circuit& circuit, CancelToken* pCancel);

} // namespace Analyzer

#endif // SIMULATOR_H
