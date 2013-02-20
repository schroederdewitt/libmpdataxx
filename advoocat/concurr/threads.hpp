/** @file
 * @copyright University of Warsaw
 * @section LICENSE
 * GPLv3+ (see the COPYING file or http://www.gnu.org/licenses/)
 */

#pragma once

#ifdef _OPENMP
#  include "openmp.hpp"
#else
#  include "boost_thread.hpp"
#endif

namespace advoocat
{
  namespace concurr
  {
    template <
      class solver_t,
      bcond::bcond_e bcx,
      bcond::bcond_e bcy = bcond::null,
      bcond::bcond_e bcz = bcond::null
    > using threads = 
#if defined(_OPENMP)
    openmp<solver_t, bcx, bcy, bcz>;
#else
    boost_thread<solver_t, bcx, bcy, bcz>;
#endif
  }; // namespace concurr
}; // namespace advoocat
