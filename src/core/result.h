/*!
 * \file result.h
 * \brief Result codes for analyzer operations and the IsErr, IsOk, and IsMsg checks.
 */

#ifndef RESULT_H
#define RESULT_H

#include <cstdint>
#include <string>
#include <vector>

namespace Analyzer
{

/*!
 * \brief Outcome of an operation.
 *
 * Negative values are errors, zero is success, and positive values are messages.
 */
enum class Result : int8_t
{
   Oscillation = -12,
   NoConvergence = -11,
   InvalidSweep = -10,
   InvalidTimeStep = -9,
   InvalidParameter = -8,
   SingularMatrix = -7,
   VoltageSourceLoop = -6,
   MissingGround = -5,
   UnknownKind = -4,
   UnknownVersion = -3,
   IoError = -2,
   Error = -1,
   Ok = 0,
   Warning = 1
};

using WarningList = std::vector<std::string>;

/*!
 * \brief Result plus the warnings collected along a successful path.
 */
struct Status
{
   Result code;
   WarningList warnings;
};

/*!
 * \brief True when the result is an error.
 *
 * \param[in] result Result to test.
 */
inline bool IsErr(Result result)
{
   return static_cast<int8_t>(result) < 0;
}

/*!
 * \brief True when the result is success.
 *
 * \param[in] result Result to test.
 */
inline bool IsOk(Result result)
{
   return result == Result::Ok;
}

/*!
 * \brief True when the result is a non-error message.
 *
 * \param[in] result Result to test.
 */
inline bool IsMsg(Result result)
{
   return static_cast<int8_t>(result) > 0;
}

/*!
 * \brief Short English sentence for a result code.
 *
 * \param[in] result Result to describe.
 */
std::string Describe(Result result);

} // namespace Analyzer

#endif // RESULT_H
