/*!
 * \file dense_system.h
 * \brief Dense real or complex linear system with partial pivoting.
 */

#ifndef DENSE_SYSTEM_H
#define DENSE_SYSTEM_H

#include "core/result.h"

#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace Analyzer
{

inline constexpr double PivotAbsoluteEpsilon = 1.0e-18;

/*!
 * \brief Row-major dense matrix Gx = b.
 */
template <typename ScalarType>
class DenseSystem
{
public:
   /*!
    * \brief Creates an empty system.
    */
   DenseSystem(void)
      : _unknownCount(0)
   {
   }

   /*!
    * \brief Clears the matrix and the right-hand side.
    *
    * \param[in] unknownCount Number of unknowns.
    */
   void Reset(uint32_t unknownCount)
   {
      _unknownCount = unknownCount;
      uint32_t cellCount = unknownCount * unknownCount;
      _matrix.assign(cellCount, ScalarType{});
      _rhs.assign(unknownCount, ScalarType{});
      _solution.assign(unknownCount, ScalarType{});
   }

   /*!
    * \brief Number of unknowns.
    */
   uint32_t UnknownCount(void) const
   {
      return _unknownCount;
   }

   /*!
    * \brief Adds a value into one matrix entry.
    *
    * \param[in] row Row index.
    * \param[in] column Column index.
    * \param[in] value Value to add.
    */
   void Add(uint32_t row, uint32_t column, ScalarType value)
   {
      if (row >= _unknownCount || column >= _unknownCount)
      {
         return;
      }
      _matrix[(row * _unknownCount) + column] += value;
   }

   /*!
    * \brief Adds a value into the right-hand side.
    *
    * \param[in] row Row index.
    * \param[in] value Value to add.
    */
   void AddRhs(uint32_t row, ScalarType value)
   {
      if (row >= _unknownCount)
      {
         return;
      }
      _rhs[row] += value;
   }

   /*!
    * \brief Solves the system and stores the solution.
    */
   Result FactorizeAndSolve(void)
   {
      for (uint32_t column = 0; column < _unknownCount; column += 1)
      {
         uint32_t pivotRow = column;
         double bestMagnitude = std::abs(_matrix[(column * _unknownCount) + column]);
         for (uint32_t row = column + 1; row < _unknownCount; row += 1)
         {
            double magnitude = std::abs(_matrix[(row * _unknownCount) + column]);
            if (magnitude > bestMagnitude)
            {
               bestMagnitude = magnitude;
               pivotRow = row;
            }
         }
         if (bestMagnitude < PivotAbsoluteEpsilon)
         {
            return Result::SingularMatrix;
         }
         if (pivotRow != column)
         {
            for (uint32_t swapColumn = column; swapColumn < _unknownCount; swapColumn += 1)
            {
               std::swap(_matrix[(pivotRow * _unknownCount) + swapColumn], _matrix[(column * _unknownCount) + swapColumn]);
            }
            std::swap(_rhs[pivotRow], _rhs[column]);
         }
         ScalarType pivot = _matrix[(column * _unknownCount) + column];
         for (uint32_t row = column + 1; row < _unknownCount; row += 1)
         {
            ScalarType factor = _matrix[(row * _unknownCount) + column] / pivot;
            _matrix[(row * _unknownCount) + column] = ScalarType{};
            for (uint32_t updateColumn = column + 1; updateColumn < _unknownCount; updateColumn += 1)
            {
               _matrix[(row * _unknownCount) + updateColumn] -= factor * _matrix[(column * _unknownCount) + updateColumn];
            }
            _rhs[row] -= factor * _rhs[column];
         }
      }
      for (int32_t row = static_cast<int32_t>(_unknownCount) - 1; row >= 0; row -= 1)
      {
         auto rowIndex = static_cast<uint32_t>(row);
         ScalarType sum = _rhs[rowIndex];
         for (uint32_t column = rowIndex + 1; column < _unknownCount; column += 1)
         {
            sum -= _matrix[(rowIndex * _unknownCount) + column] * _solution[column];
         }
         _solution[rowIndex] = sum / _matrix[(rowIndex * _unknownCount) + rowIndex];
      }
      return Result::Ok;
   }

   /*!
    * \brief One solved unknown.
    *
    * \param[in] index Unknown index.
    */
   ScalarType Solution(uint32_t index) const
   {
      if (index >= _unknownCount)
      {
         return ScalarType{};
      }
      return _solution[index];
   }

private:
   uint32_t _unknownCount;
   std::vector<ScalarType> _matrix;
   std::vector<ScalarType> _rhs;
   std::vector<ScalarType> _solution;
};

} // namespace Analyzer

#endif // DENSE_SYSTEM_H
