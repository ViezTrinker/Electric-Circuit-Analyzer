/*!
 * \file netlist.h
 * \brief Compiled nets and devices used by every analysis.
 */

#ifndef NETLIST_H
#define NETLIST_H

#include "component.h"
#include "result.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Analyzer
{

inline constexpr uint32_t GroundNetIndex = 0;

using PinNetArray = std::array<uint32_t, MaxPinCount>;

/*!
 * \brief One placed device after pins have been mapped onto nets.
 */
struct NetlistDevice
{
   uint64_t componentId;
   std::string name;
   ComponentKind kind;
   uint8_t pinCount;
   PinNetArray nets;
   ComponentProperties properties;
};

using NetlistDeviceList = std::vector<NetlistDevice>;
using NetNameList = std::vector<std::string>;

/*!
 * \brief Connectivity shared by DC, AC, and transient.
 */
struct Netlist
{
   uint32_t netCount;
   bool hasGround;
   bool hasAnalog;
   NetNameList netNames;
   NetlistDeviceList devices;
   WarningList warnings;
};

} // namespace Analyzer

#endif // NETLIST_H
