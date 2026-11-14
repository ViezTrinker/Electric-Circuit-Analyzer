/*!
 * \file logic_drive.h
 * \brief Thevenin drive of a logic output on an analog net.
 */

#ifndef LOGIC_DRIVE_H
#define LOGIC_DRIVE_H

#include <cstdint>
#include <vector>

namespace Analyzer
{

/*!
 * \brief Voltage source behind a resistance from one net to ground.
 *
 * Used for a push-pull or open-drain-low logic output that shares a net with
 * an analog part. An AC stamp keeps the resistance and drops the voltage,
 * because the frozen operating-point source is a small-signal short.
 */
struct LogicDrive
{
   uint32_t netIndex;
   double voltageVolt;
   double resistanceOhm;
};

using LogicDriveList = std::vector<LogicDrive>;

} // namespace Analyzer

#endif // LOGIC_DRIVE_H
