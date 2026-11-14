/*!
 * \file cancel_token.h
 * \brief Cooperative cancellation flag for long analyses.
 */

#ifndef CANCEL_TOKEN_H
#define CANCEL_TOKEN_H

#include <atomic>

namespace Analyzer
{

/*!
 * \brief Set from the interface and polled by the solvers.
 */
class CancelToken
{
public:
   /*!
    * \brief Creates a token that is not cancelled.
    */
   CancelToken(void);

   /*!
    * \brief Clears a previous cancel request.
    */
   void Reset(void);

   /*!
    * \brief Asks the running analysis to stop.
    */
   void Request(void);

   /*!
    * \brief True after Request until Reset.
    */
   bool IsRequested(void) const;

private:
   std::atomic<bool> _requested;
};

} // namespace Analyzer

#endif // CANCEL_TOKEN_H
