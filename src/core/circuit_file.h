/*!
 * \file circuit_file.h
 * \brief Reads and writes one circuit as UTF-8 .eca.json text.
 */

#ifndef CIRCUIT_FILE_H
#define CIRCUIT_FILE_H

#include "circuit.h"

#include <string>
#include <string_view>

namespace Analyzer
{

/*!
 * \brief Writes a circuit document as JSON text.
 *
 * The text uses format "electric-circuit-analyzer" and version 1. It includes
 * the analysis settings, parts, wires, and net labels.
 *
 * \param[in] circuit Document to write.
 * \param[out] pText UTF-8 JSON. Required.
 */
Result WriteCircuitText(const Circuit& circuit, std::string* pText);

/*!
 * \brief Reads a version-1 circuit document from JSON text.
 *
 * An unknown format version or component kind rejects the whole text. On
 * failure the destination circuit is left unchanged.
 *
 * \param[in] text UTF-8 JSON.
 * \param[out] pCircuit Document replaced on success. Required.
 */
Result ReadCircuitText(std::string_view text, Circuit* pCircuit);

} // namespace Analyzer

#endif // CIRCUIT_FILE_H
