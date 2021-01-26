//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2020 QMCPACK developers.
//
// File developed by: Peter Doak, doakpw@ornl.gov, Oak Ridge National Laboratory
//                    Ken Esler, kpesler@gmail.com, University of Illinois at Urbana-Champaign
//                    Jeremy McMinnis, jmcminis@gmail.com, University of Illinois at Urbana-Champaign
//                    Jeongnim Kim, jeongnim.kim@gmail.com, University of Illinois at Urbana-Champaign
//                    Ye Luo, yeluo@anl.gov, Argonne National Laboratory
//                    Mark A. Berrill, berrillma@ornl.gov, Oak Ridge National Laboratory
//
// File created by: Jeongnim Kim, jeongnim.kim@gmail.com, University of Illinois at Urbana-Champaign
//////////////////////////////////////////////////////////////////////////////////////


#ifndef QMCPLUSPLUS_WALKER_CONTROL_BASE_H
#define QMCPLUSPLUS_WALKER_CONTROL_BASE_H

#include "Configuration.h"
#include "Particle/MCWalkerConfiguration.h"
#include "QMCDrivers/WalkerElementsRef.h"
#include "QMCDrivers/MCPopulation.h"
#include "Message/MPIObjectBase.h"
#include "Message/CommOperators.h"
// #include "QMCDrivers/ForwardWalking/ForwardWalkingStructure.h"

//#include <boost/archive/binary_oarchive.hpp>

namespace qmcplusplus
{
/** Class for controlling the walkers for DMC simulations.
 * w and w/o MPI. Fixed and dynamic population in one place.
 */
class WalkerControl : public MPIObjectBase
{
public:
  ///typedef of Walker_t
  typedef MCWalkerConfiguration::Walker_t Walker_t;
  /// distinct type for "new" walker, currently same as Walker_t
  using MCPWalker = MCPopulation::MCPWalker;
  ///typedef of FullPrecRealType
  using FullPrecRealType = QMCTraits::FullPrecRealType;
  ///typedef of IndexType
  typedef QMCTraits::IndexType IndexType;

  /** default constructor
   *
   * Set the SwapMode to zero so that instantiation can be done
   */
  WalkerControl(Communicate* c, RandomGenerator_t& rng, bool use_fixed_pop = false);

  /** empty destructor to clean up the derived classes */
  ~WalkerControl();

  /** start a block */
  void start();

  /** take averages and writes to a file */
  void measureProperties(int iter);

  /** set the trial energy
   */
  inline void setTrialEnergy(FullPrecRealType et) { trial_energy_ = et; }

  /** reset to accumulate data */
  virtual void reset();

  /** unified: perform branch and swap walkers as required 
   *
   *  \return global population
   */
  virtual FullPrecRealType branch(int iter, MCPopulation& pop, bool do_not_branch);

  virtual FullPrecRealType getFeedBackParameter(int ngen, FullPrecRealType tau)
  {
    return 1.0 / (static_cast<FullPrecRealType>(ngen) * tau);
  }

  bool put(xmlNodePtr cur);

  void setMinMax(int nw_in, int nmax_in);

  int get_n_max() const { return n_max_; }
  int get_n_min() const { return n_min_; }
  FullPrecRealType get_target_sigma() const { return target_sigma_; }
  MCDataType<FullPrecRealType>& get_ensemble_property() { return ensemble_property_; }
  void set_ensemble_property(MCDataType<FullPrecRealType>& ensemble_property)
  {
    ensemble_property_ = ensemble_property;
  }
  IndexType get_num_contexts() const { return num_ranks_; }

private:
  /// kill dead walkers in the population
  static void killDeadWalkersOnRank(MCPopulation& pop);

  static std::vector<IndexType> syncFutureWalkersPerRank(Communicate* comm, IndexType n_walkers);

  void Write2XYZ(MCWalkerConfiguration& W);

  /// compute curData
  void computeCurData(const UPtrVector<MCPWalker>& walkers);

  /** creates the distribution plan
   *
   *  populates the minus and plus vectors they contain 1 copy of a partition index 
   *  for each adjustment in population to the context.
   *  \param[in] num_per_node as if all walkers were copied out to multiplicity
   *  \param[out] fair_offset running population count at each partition boundary
   *  \param[out] minus list of partition indexes one occurance for each walker removed
   *  \param[out] plus list of partition indexes one occurance for each walker added
   */
  static void determineNewWalkerPopulation(const std::vector<int>& num_per_node,
                                           std::vector<int>& fair_offset,
                                           std::vector<int>& minus,
                                           std::vector<int>& plus);

#if defined(HAVE_MPI)
  /** swap Walkers with Recv/Send or Irecv/Isend
   *
   * The algorithm ensures that the load per node can differ only by one walker.
   * Each MPI rank can only send or receive or be silent.
   * The communication is one-dimensional and very local.
   * If multiple copies of a walker need to be sent to the target rank, only send one.
   * The number of copies is communicated ahead via blocking send/recv.
   * Then the walkers are transferred via blocking or non-blocking send/recv.
   * The blocking send/recv may become serialized and worsen load imbalance.
   * Non blocking send/recv algorithm avoids serialization completely.
   */
  void swapWalkersSimple(MCPopulation& pop);
#endif

  /** An enum to access curData for reduction
   *
   * curData is larger than this //LE_MAX + n_node * T
   */
  enum
  {
    ENERGY_INDEX = 0,
    ENERGY_SQ_INDEX,
    WALKERSIZE_INDEX,
    WEIGHT_INDEX,
    R2ACCEPTED_INDEX,
    R2PROPOSED_INDEX,
    FNSIZE_INDEX,
    SENTWALKERS_INDEX,
    LE_MAX
  };

  ///random number generator
  RandomGenerator_t& rng_;
  ///if true, use fixed population
  bool use_fixed_pop_;
  ///minimum number of walkers
  IndexType n_min_;
  ///maximum number of walkers
  IndexType n_max_;
  ///maximum copy per walker
  IndexType max_copy_;
  ///trial energy energy
  FullPrecRealType trial_energy_;
  ///target sigma to limit fluctuations of the trial energy
  FullPrecRealType target_sigma_;
  ///number of walkers on each node after branching before load balancing
  std::vector<int> num_per_node_;
  ///offset of the particle index for a fair distribution
  std::vector<int> fair_offset_;
  ///filename for dmc.dat
  std::string dmcFname;
  ///file to save energy histogram
  std::ofstream* dmcStream;
  ///context id
  IndexType rank_num_;
  ///number of contexts
  IndexType num_ranks_;
  ///0 is default
  IndexType SwapMode;
  ///any temporary data includes many ridiculous conversions of integral types to and from fp
  std::vector<FullPrecRealType> curData;
  ///Use non-blocking isend/irecv
  bool use_nonblocking_;
  ///ensemble properties
  MCDataType<FullPrecRealType> ensemble_property_;
  ///timers
  TimerList_t myTimers;
  ///Number of walkers sent during the exchange
  IndexType saved_num_walkers_sent_;
};

} // namespace qmcplusplus
#endif