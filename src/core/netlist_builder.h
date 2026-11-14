/*!
 * \file netlist_builder.h
 * \brief Builds nets from component pins, wires, and labels.
 */

#ifndef NETLIST_BUILDER_H
#define NETLIST_BUILDER_H

#include "circuit.h"
#include "netlist.h"

namespace Analyzer
{

/*!
 * \brief Connects pins that share a grid point or an orthogonal wire.
 *
 * \param[in] circuit Schematic to compile.
 * \param[out] pNetlist Compiled nets and devices.
 */
Result BuildNetlist(const Circuit& circuit, Netlist* pNetlist);

} // namespace Analyzer

#endif // NETLIST_BUILDER_H
