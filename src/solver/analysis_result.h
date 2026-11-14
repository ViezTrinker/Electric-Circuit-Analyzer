/*!
 * \file analysis_result.h
 * \brief DC, AC, and transient results shown by the interface.
 */

#ifndef ANALYSIS_RESULT_H
#define ANALYSIS_RESULT_H

#include "core/component.h"
#include "core/result.h"

#include <complex>
#include <cstdint>
#include <string>
#include <vector>

namespace Analyzer
{

struct NodeValue
{
   uint32_t netIndex;
   std::string name;
   double value;
   double phaseDeg;
   LogicLevel level;
};

struct BranchValue
{
   uint64_t componentId;
   std::string name;
   double currentAmpere;
   double voltageVolt;
   double powerWatt;
   LogicLevel level;
};

using NodeValueList = std::vector<NodeValue>;
using BranchValueList = std::vector<BranchValue>;
using VoltageList = std::vector<double>;
using LogicLevelList = std::vector<LogicLevel>;

struct DcResult
{
   Result code;
   std::string message;
   WarningList warnings;
   NodeValueList nodes;
   BranchValueList branches;
   VoltageList netVoltage;
   LogicLevelList netLevel;
};

struct AcPoint
{
   double frequencyHz;
   VoltageList netVoltageReal;
   VoltageList netVoltageImag;
};

using AcPointList = std::vector<AcPoint>;

struct AcResult
{
   Result code;
   std::string message;
   WarningList warnings;
   AcPointList points;
   DcResult bias;
};

struct TransientSample
{
   double timeSec;
   VoltageList netVoltage;
   LogicLevelList netLevel;
};

using TransientSampleList = std::vector<TransientSample>;

struct TransientResult
{
   Result code;
   std::string message;
   WarningList warnings;
   TransientSampleList samples;
   DcResult initial;
};

} // namespace Analyzer

#endif // ANALYSIS_RESULT_H
