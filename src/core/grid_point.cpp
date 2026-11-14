/*!
 * \file grid_point.cpp
 * \brief Rotation of schematic grid offsets.
 */

#include "grid_point.h"

namespace Analyzer
{

namespace
{

constexpr int32_t FullCircleDegrees = 360;
constexpr int32_t QuarterTurnDegrees = 90;

} // namespace

bool IsQuarterTurn(int32_t rotationDegrees)
{
   int32_t wrapped = rotationDegrees % FullCircleDegrees;
   if (wrapped < 0)
   {
      wrapped += FullCircleDegrees;
   }
   return wrapped == 0
      || wrapped == QuarterTurnDegrees
      || wrapped == (QuarterTurnDegrees * 2)
      || wrapped == (QuarterTurnDegrees * 3);
}

GridPoint RotateOffset(GridPoint offset, int32_t rotationDegrees)
{
   int32_t wrapped = rotationDegrees % FullCircleDegrees;
   if (wrapped < 0)
   {
      wrapped += FullCircleDegrees;
   }
   if (wrapped == QuarterTurnDegrees)
   {
      return GridPoint{-offset.positionY, offset.positionX};
   }
   if (wrapped == (QuarterTurnDegrees * 2))
   {
      return GridPoint{-offset.positionX, -offset.positionY};
   }
   if (wrapped == (QuarterTurnDegrees * 3))
   {
      return GridPoint{offset.positionY, -offset.positionX};
   }
   return offset;
}

} // namespace Analyzer
