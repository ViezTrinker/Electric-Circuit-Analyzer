/*!
 * \file simulator.cpp
 * \brief Compiles a schematic and dispatches analog and digital analysis.
 */

#include "simulator.h"

#include "analog_solver.h"
#include "core/netlist_builder.h"
#include "digital/logic_kernel.h"
#include "logic_bridge.h"

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
   return SolveOperatingPoint(netlist, circuit.Settings(), pCancel);
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
   DcResult bias = SolveOperatingPoint(netlist, circuit.Settings(), pCancel);
   if (IsErr(bias.code))
   {
      AcResult result = {};
      result.code = bias.code;
      result.message = bias.message;
      result.warnings = bias.warnings;
      result.bias = bias;
      return result;
   }
   LogicDriveList drives;
   Result collected = CollectOperatingDrives(netlist, bias.netLevel, &drives);
   if (IsErr(collected))
   {
      AcResult result = {};
      result.code = collected;
      result.message = Describe(collected);
      result.bias = bias;
      return result;
   }
   return SolveAcAnalog(netlist, circuit.Settings(), pCancel, &drives, &bias);
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
   if (HasLogicDevice(netlist))
   {
      return SolveMixedTransient(netlist, circuit.Settings(), pCancel);
   }
   return SolveTransientAnalog(netlist, circuit.Settings(), pCancel);
}

} // namespace Analyzer
