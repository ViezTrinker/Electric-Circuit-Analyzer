/*!
 * \file result.cpp
 * \brief English descriptions for analyzer result codes.
 */

#include "result.h"

namespace Analyzer
{

std::string Describe(Result result)
{
   switch (result)
   {
      case Result::Oscillation:
      {
         return "The digital circuit oscillates or contains a combinational loop.";
      }
      case Result::NoConvergence:
      {
         return "The nonlinear solver did not converge.";
      }
      case Result::InvalidSweep:
      {
         return "The frequency sweep settings are invalid.";
      }
      case Result::InvalidTimeStep:
      {
         return "The transient time settings are invalid.";
      }
      case Result::InvalidParameter:
      {
         return "A component parameter is invalid.";
      }
      case Result::SingularMatrix:
      {
         return "The circuit matrix is singular.";
      }
      case Result::VoltageSourceLoop:
      {
         return "Voltage sources or ideal shorts form a loop.";
      }
      case Result::MissingGround:
      {
         return "The analog circuit has no ground.";
      }
      case Result::UnknownKind:
      {
         return "The file contains an unknown component kind.";
      }
      case Result::UnknownVersion:
      {
         return "The file version is not supported.";
      }
      case Result::IoError:
      {
         return "The file could not be read or written.";
      }
      case Result::Error:
      {
         return "The operation failed.";
      }
      case Result::Ok:
      {
         return "Ok.";
      }
      case Result::Warning:
      {
         return "The solution completed with warnings.";
      }
      default:
      {
         break;
      }
   }
   return "Unknown result.";
}

} // namespace Analyzer
