/*!
 * \file example_circuits.h
 * \brief Built-in schematics shipped as examples/*.eca.json.
 */

#ifndef EXAMPLE_CIRCUITS_H
#define EXAMPLE_CIRCUITS_H

#include "circuit.h"

#include <cstdint>
#include <string_view>

namespace Analyzer
{

/*!
 * \brief One schematic in the examples directory.
 */
enum class ExampleCircuit : int8_t
{
   VoltageDivider = 0,
   RcLowpass,
   RlcStep,
   Rectifier,
   NpnBias,
   NmosSwitch,
   NandSrLatch,
   BinaryCounter,
   SevenSegmentCount,
   LedAndSwitch,
   Timer555Astable,
   Count
};

/*!
 * \brief File name inside the examples directory.
 *
 * \param[in] example Schematic to name.
 */
std::string_view ExampleFileName(ExampleCircuit example);

/*!
 * \brief Menu title for a schematic.
 *
 * \param[in] example Schematic to title.
 */
std::string_view ExampleTitle(ExampleCircuit example);

/*!
 * \brief Builds one example schematic.
 *
 * \param[in] example Schematic to build.
 * \param[out] pCircuit Replaced on success. Required.
 */
Result BuildExample(ExampleCircuit example, Circuit* pCircuit);

} // namespace Analyzer

#endif // EXAMPLE_CIRCUITS_H
