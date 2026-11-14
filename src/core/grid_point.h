/*!
 * \file grid_point.h
 * \brief Integer schematic coordinates and orthogonal rotation.
 */

#ifndef GRID_POINT_H
#define GRID_POINT_H

#include <cstdint>
#include <vector>

namespace Analyzer
{

/*!
 * \brief A point on the schematic grid.
 */
struct GridPoint
{
   int32_t positionX;
   int32_t positionY;
};

using GridPointList = std::vector<GridPoint>;

/*!
 * \brief Ordering used by point maps.
 *
 * \param[in] left First point.
 * \param[in] right Second point.
 */
inline bool operator<(GridPoint left, GridPoint right)
{
   if (left.positionX != right.positionX)
   {
      return left.positionX < right.positionX;
   }
   return left.positionY < right.positionY;
}

/*!
 * \brief True when both coordinates match.
 *
 * \param[in] left First point.
 * \param[in] right Second point.
 */
inline bool operator==(GridPoint left, GridPoint right)
{
   return left.positionX == right.positionX && left.positionY == right.positionY;
}

/*!
 * \brief True when the coordinates differ.
 *
 * \param[in] left First point.
 * \param[in] right Second point.
 */
inline bool operator!=(GridPoint left, GridPoint right)
{
   return (left == right) == false;
}

/*!
 * \brief Rotates an offset by a multiple of 90 degrees.
 *
 * Positive Y points down, matching the schematic view. A 90 degree turn is
 * clockwise, so (x, y) becomes (-y, x).
 *
 * \param[in] offset Offset before rotation.
 * \param[in] rotationDegrees Clockwise rotation. Only 0, 90, 180, and 270 are valid.
 */
GridPoint RotateOffset(GridPoint offset, int32_t rotationDegrees);

/*!
 * \brief True for 0, 90, 180, and 270.
 *
 * \param[in] rotationDegrees Rotation to test.
 */
bool IsQuarterTurn(int32_t rotationDegrees);

} // namespace Analyzer

#endif // GRID_POINT_H
