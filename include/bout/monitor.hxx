#ifndef __MONITOR_H__
#define __MONITOR_H__

#include "bout_types.hxx"
#include "bout/assert.hxx"

#include <cmath>

class Solver;

/// Return true if either \p a is a multiple of \p b or vice-versa
///
/// Assumes both arguments are greater than zero
inline bool isMultiple(BoutReal a, BoutReal b) {
  ASSERT2(a > 0);
  ASSERT2(b > 0);
  return (std::fmod(a, b) == 0) || (std::fmod(b, a) == 0);
}

/// Monitor baseclass for the Solver
///
/// Can be called ether with a specified frequency, or with the
/// frequency of the BOUT++ output monitor.
class Monitor{
  friend class Solver; ///< needs access to timestep and freq
public:
  /// A \p timestep_ of -1 defaults to the the frequency of the BOUT++
  /// output monitor
  Monitor(BoutReal timestep_ = -1) : timestep(timestep_){};

  virtual ~Monitor(){};

  /// Callback function for the solver, called after timestep_ has passed
  ///
  /// @param[in] solver The solver calling this monitor
  /// @param[in] time   The current simulation time
  /// @param[in] iter   The current simulation iteration
  /// @param[in] nout   The total number of iterations for this simulation
  ///
  /// @returns non-zero if simulation should be stopped
  virtual int call(Solver *solver, BoutReal time, int iter, int nout) = 0;

  /// Callback function for when a clean shutdown is initiated
  virtual void cleanup(){};

private:
  BoutReal timestep;
  int freq;
};

#endif // __MONITOR_H__
