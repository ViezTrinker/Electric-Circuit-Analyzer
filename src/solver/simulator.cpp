/*!
 * \file simulator.cpp
 * \brief Compiles a schematic and dispatches analog and digital analysis.
 */

#include "simulator.h"

#include "analog_solver.h"
#include "core/netlist_builder.h"
#include "digital/logic_kernel.h"

namespace Analyzer
{

namespace
{

DcResult FailedDc(Result code, const WarningList& warnings)
{
   DcResult result = {};
   result.code = code;
   result.message = Describe(code);
   result.warnings = warnings;
   return result;
}

} // namespace

DcResult SolveDc(const Circuit& circuit, CancelToken* pCancel)
{
   Netlist netlist;
   Result built = BuildNetlist(circuit, &netlist);
   if (IsErr(built))
   {
      return FailedDc(built, netlist.warnings);
   }
   DcResult result = SolveDcAnalog(netlist, circuit.Settings(), pCancel);
   if (IsErr(result.code))
   {
      return result;
   }
   Result digital = SolveDigitalDc(netlist, &result);
   if (IsErr(digital))
   {
      result.code = digital;
      result.message = Describe(digital);
   }
   return result;
}

AcResult SolveAc(const Circuit& circuit, CancelToken* pCancel)
{
   Netlist netlist;
   Result built = BuildNetlist(circuit, &netlist);
   if (IsErr(built))
   {
      AcResult result = {};
      result.code = built;
      result.message = Describe(built);
      result.warnings = netlist.warnings;
      return result;
   }
   return SolveAcAnalog(netlist, circuit.Settings(), pCancel);
}

TransientResult SolveTransient(const Circuit& circuit, CancelToken* pCancel)
{
   Netlist netlist;
   Result built = BuildNetlist(circuit, &netlist);
   if (IsErr(built))
   {
      TransientResult result = {};
      result.code = built;
      result.message = Describe(built);
      result.warnings = netlist.warnings;
      return result;
   }
   if (netlist.hasAnalog == false)
   {
      TransientResult result = {};
      Result digital = SolveDigitalTransient(netlist, circuit.Settings(), pCancel, &result);
      if (IsErr(digital) && result.code == Result::Ok)
      {
         result.code = digital;
         result.message = Describe(digital);
      }
      return result;
   }
   return SolveTransientAnalog(netlist, circuit.Settings(), pCancel);
}

} // namespace Analyzer
