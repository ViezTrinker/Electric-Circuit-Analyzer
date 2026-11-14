/*!
 * \file si_parser.h
 * \brief Parses and formats numbers with SI prefixes.
 */

#ifndef SI_PARSER_H
#define SI_PARSER_H

#include "result.h"

#include <string>
#include <string_view>

namespace Analyzer
{

/*!
 * \brief Parses a number such as 4.7k, 10m, 10u, or 1Meg.
 *
 * \param[in] text User text.
 * \param[out] pValue Parsed value in base units.
 */
Result ParseSiNumber(std::string_view text, double* pValue);

/*!
 * \brief Formats a number with an SI prefix.
 *
 * \param[in] value Value in base units.
 */
std::string FormatSiNumber(double value);

} // namespace Analyzer

#endif // SI_PARSER_H
