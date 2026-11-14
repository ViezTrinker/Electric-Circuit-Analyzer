/*!
 * \file cancel_token.cpp
 * \brief Cooperative cancellation flag.
 */

#include "cancel_token.h"

namespace Analyzer
{

CancelToken::CancelToken(void)
   : _requested(false)
{
}

void CancelToken::Reset(void)
{
   _requested.store(false);
}

void CancelToken::Request(void)
{
   _requested.store(true);
}

bool CancelToken::IsRequested(void) const
{
   return _requested.load();
}

} // namespace Analyzer
