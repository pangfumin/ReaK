/**
 * \file invariant_kalman_filter.hpp
 * 
 * This library provides a number of functions and classes to do state estimation 
 * using the Invariant Kalman Filter. This filtering technique applies to a 
 * gaussian belief state and an invariant state-space system. The Kalman filter is 
 * an optimal filter for a linear 
 * state-space system where all sources of noise or disturbances are Gaussian 
 * (normally distributed). This Kalman filter implementation requires
 * that the system be a linearized system which has an invariant frame which 
 * can map the state-space into a frame in which the non-linearities have less or 
 * no effect on the covariance transitions during the prediction and measurement 
 * update.
 * 
 * \author Sven Mikael Persson <mikael.s.persson@gmail.com>
 * \date May 2011
 */

/*
 *    Copyright 2011 Sven Mikael Persson
 *
 *    THIS SOFTWARE IS DISTRIBUTED UNDER THE TERMS OF THE GNU GENERAL PUBLIC LICENSE v3 (GPLv3).
 *
 *    This file is part of ReaK.
 *
 *    ReaK is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    ReaK is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with ReaK (as LICENSE in the root folder).  
 *    If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef REAK_INVARIANT_KALMAN_FILTER_HPP
#define REAK_INVARIANT_KALMAN_FILTER_HPP

#include <ReaK/core/lin_alg/vect_concepts.hpp>
#include <ReaK/core/lin_alg/mat_alg.hpp>
#include <ReaK/core/lin_alg/mat_cholesky.hpp>

#include <ReaK/ctrl/topologies/metric_space_concept.hpp>

#include "belief_state_concept.hpp"
#include "discrete_linear_sss_concept.hpp"
#include "invariant_system_concept.hpp"
#include "covariance_concept.hpp"
#include "gaussian_belief_state.hpp"
#include "covariance_matrix.hpp"
#include "kalman_filter.hpp"

#include <boost/utility/enable_if.hpp>
#include <boost/static_assert.hpp>
#include <boost/mpl/and.hpp>

namespace ReaK {

namespace ctrl {



/**
 * This function template performs one prediction step using the Invariant Kalman Filter method.
 * \tparam InvariantSystem An invariant discrete-time state-space system modeling the 
 *         InvariantDiscreteSystemConcept.
 * \tparam StateSpaceType A topology type on which the state-vectors can reside, should model
 *         the pp::TopologyConcept.
 * \tparam BeliefState A belief state type modeling the ContinuousBeliefStateConcept with
 *         a unimodular gaussian representation.
 * \tparam InputBelief A belief state type modeling the ContinuousBeliefStateConcept with
 *         a unimodular gaussian representation.
 * \param sys The invariant discrete-time state-space system used in the state estimation.
 * \param state_space The state-space topology on which the state representations lie.
 * \param b_x As input, it stores the belief-state before the prediction step. As output, it stores
 *        the belief-state after the prediction step.
 * \param b_u The input belief to apply to the state-space system to make the transition of the 
 *        mean-state, i.e., the current input vector and its covariance.
 * \param t The current time (before the prediction).
 * 
 */
template <typename InvariantSystem, 
          typename StateSpaceType,
          typename BeliefState, 
          typename InputBelief>
typename boost::enable_if< 
  boost::mpl::and_< 
    is_invariant_system<InvariantSystem>,
    is_continuous_belief_state<BeliefState>,
    boost::mpl::bool_< (belief_state_traits<BeliefState>::representation == belief_representation::gaussian) >,
    boost::mpl::bool_< (belief_state_traits<BeliefState>::distribution == belief_distribution::unimodal) >
  >,
void >::type invariant_kalman_predict(const InvariantSystem& sys,
                                      const StateSpaceType& state_space,
                                      BeliefState& b_x,
                                      const InputBelief& b_u,
                                      typename discrete_sss_traits<InvariantSystem>::time_type t = 0) {
  //here the requirement is that the system models a linear system which is at worse a linearized system
  // - if the system is LTI or LTV, then this will result in a basic Kalman Filter (KF) update
  // - if the system is linearized, then this will result in an Extended Kalman Filter (EKF) update
  BOOST_CONCEPT_ASSERT((pp::TopologyConcept< StateSpaceType >));
  BOOST_CONCEPT_ASSERT((InvariantDiscreteSystemConcept<InvariantSystem, StateSpaceType>));
  BOOST_CONCEPT_ASSERT((ContinuousBeliefStateConcept<BeliefState>));
  BOOST_CONCEPT_ASSERT((ContinuousBeliefStateConcept<InputBelief>));
  
  typedef typename discrete_sss_traits<InvariantSystem>::point_type StateType;
  typedef typename continuous_belief_state_traits<BeliefState>::covariance_type CovType;
  typedef typename covariance_mat_traits< CovType >::matrix_type MatType;
  typedef typename invariant_system_traits<InvariantSystem>::invariant_frame_type InvarFrame;
  
  typename discrete_linear_sss_traits<InvariantSystem>::matrixA_type A;
  typename discrete_linear_sss_traits<InvariantSystem>::matrixB_type B;
  
  StateType x = b_x.get_mean_state();
  MatType P = b_x.get_covariance().get_matrix();
  
  StateType x_prior = sys.get_next_state(state_space, x, b_u.get_mean_state(), t);
  sys.get_state_transition_blocks(A, B, state_space, t, t + sys.get_time_step(), x, x_prior, b_u.get_mean_state(), b_u.get_mean_state());
  InvarFrame W = sys.get_invariant_prior_frame(state_space, x, x_prior, b_u.get_mean_state(), t + sys.get_time_step());
  P = W * (( A * P * transpose_view(A)) + B * b_u.get_covariance().get_matrix() * transpose_view(B)) * transpose_view(W);
  b_x.set_mean_state( x_prior );
  b_x.set_covariance( CovType( P ) );
};


template <typename InvariantSystem, 
          typename StateSpaceType,
          typename BeliefState, 
          typename InputBelief>
typename boost::disable_if< is_invariant_system<InvariantSystem>,
void >::type invariant_kalman_predict(const InvariantSystem& sys,
                                      const StateSpaceType& state_space,
                                      BeliefState& b_x,
                                      const InputBelief& b_u,
                                      typename discrete_sss_traits<InvariantSystem>::time_type t = 0) {
  kalman_predict(sys, state_space, b_x, b_u, t);
};




/**
 * This function template performs one measurement update step using the Invariant Kalman Filter method.
 * \tparam InvariantSystem An invariant discrete-time state-space system modeling the 
 *         InvariantDiscreteSystemConcept.
 * \tparam StateSpaceType A topology type on which the state-vectors can reside, should model
 *         the pp::TopologyConcept.
 * \tparam BeliefState A belief state type modeling the ContinuousBeliefStateConcept with
 *         a unimodular gaussian representation.
 * \tparam InputBelief A belief state type modeling the ContinuousBeliefStateConcept with
 *         a unimodular gaussian representation.
 * \tparam MeasurementBelief A belief state type modeling the ContinuousBeliefStateConcept with
 *         a unimodular gaussian representation.
 * \param sys The invariant discrete-time state-space system used in the state estimation.
 * \param state_space The state-space topology on which the state representations lie.
 * \param b_x As input, it stores the belief-state before the update step. As output, it stores
 *        the belief-state after the update step.
 * \param b_u The input vector to apply to the state-space system to make the transition of the 
 *        mean-state, i.e., the current input vector and its covariance.
 * \param b_z The output belief that was measured, i.e. the measurement vector and its covariance.
 * \param t The current time.
 * 
 */
template <typename InvariantSystem,  
          typename StateSpaceType,
          typename BeliefState, 
          typename InputBelief, 
          typename MeasurementBelief>
typename boost::enable_if< 
  boost::mpl::and_< 
    is_invariant_system<InvariantSystem>,
    is_continuous_belief_state<BeliefState>,
    boost::mpl::bool_< (belief_state_traits<BeliefState>::representation == belief_representation::gaussian) >,
    boost::mpl::bool_< (belief_state_traits<BeliefState>::distribution == belief_distribution::unimodal) >
  >,
void >::type invariant_kalman_update(const InvariantSystem& sys,
                                     const StateSpaceType& state_space,
                                     BeliefState& b_x,
                                     const InputBelief& b_u,
                                     const MeasurementBelief& b_z,
                                     typename discrete_sss_traits<InvariantSystem>::time_type t = 0) {
  //here the requirement is that the system models a linear system which is at worse a linearized system
  // - if the system is LTI or LTV, then this will result in a basic Kalman Filter (KF) update
  // - if the system is linearized, then this will result in an Extended Kalman Filter (EKF) update
  BOOST_CONCEPT_ASSERT((pp::TopologyConcept< StateSpaceType >));
  BOOST_CONCEPT_ASSERT((InvariantDiscreteSystemConcept<InvariantSystem, StateSpaceType>));
  BOOST_CONCEPT_ASSERT((ContinuousBeliefStateConcept<BeliefState>));
  BOOST_CONCEPT_ASSERT((ContinuousBeliefStateConcept<InputBelief>));
  BOOST_CONCEPT_ASSERT((ContinuousBeliefStateConcept<MeasurementBelief>));
  
  typedef typename discrete_sss_traits<InvariantSystem>::point_type StateType;
  typedef typename continuous_belief_state_traits<BeliefState>::covariance_type CovType;
  typedef typename covariance_mat_traits< CovType >::matrix_type MatType;
  typedef typename mat_traits<MatType>::value_type ValueType;
  typedef typename invariant_system_traits<InvariantSystem>::invariant_frame_type InvarFrame;
  typedef typename invariant_system_traits<InvariantSystem>::invariant_correction_type InvarCorr;
  
  typename discrete_linear_sss_traits<InvariantSystem>::matrixC_type C;
  typename discrete_linear_sss_traits<InvariantSystem>::matrixD_type D;
  
  StateType x = b_x.get_mean_state();
  MatType P = b_x.get_covariance().get_matrix();
  sys.get_output_function_blocks(C, D, state_space, t, x, b_u.get_mean_state());
  
  vect_n<ValueType> e = 
    to_vect<ValueType>(sys.get_invariant_error(state_space, x, b_u.get_mean_state(), b_z.get_mean_state(), t + sys.get_time_step()));
  
  mat< ValueType, mat_structure::rectangular, mat_alignment::column_major > CP = C * P;
  mat< ValueType, mat_structure::symmetric > S(CP * transpose_view(C) + b_z.get_covariance().get_matrix());
  linsolve_Cholesky(S,CP);
  mat< ValueType, mat_structure::rectangular, mat_alignment::row_major > K(transpose_view(CP));
   
  b_x.set_mean_state( sys.apply_correction(state_space, x, from_vect<InvarCorr>(K * e), b_u.get_mean_state(), t + sys.get_time_step()) );
  InvarFrame W = sys.get_invariant_posterior_frame(state_space, x, b_x.get_mean_state(), b_u.get_mean_state(), t + sys.get_time_step());
  b_x.set_covariance( CovType( MatType( W * ((mat< ValueType, mat_structure::identity>(K.get_row_count()) - K * C) * P) * transpose_view(W) ) ) );
};



template <typename InvariantSystem,  
          typename StateSpaceType,
          typename BeliefState, 
          typename InputBelief, 
          typename MeasurementBelief>
typename boost::disable_if< is_invariant_system<InvariantSystem>,
void >::type invariant_kalman_update(const InvariantSystem& sys,
                                     const StateSpaceType& state_space,
                                     BeliefState& b_x,
                                     const InputBelief& b_u,
                                     const MeasurementBelief& b_z,
                                     typename discrete_sss_traits<InvariantSystem>::time_type t = 0) {
  kalman_update(sys, state_space, b_x, b_u, b_z, t);
};



/**
 * This function template performs one complete estimation step using the (Extended) Kalman 
 * Filter method, which includes a prediction and measurement update step. This function is, 
 * in general, more efficient than applying the prediction and update separately.
 * \tparam InvariantSystem An invariant discrete-time state-space system modeling the 
 *         InvariantDiscreteSystemConcept.
 * \tparam StateSpaceType A topology type on which the state-vectors can reside, should model
 *         the pp::TopologyConcept.
 * \tparam BeliefState A belief state type modeling the ContinuousBeliefStateConcept with
 *         a unimodular gaussian representation.
 * \tparam InputBelief A belief state type modeling the ContinuousBeliefStateConcept with
 *         a unimodular gaussian representation.
 * \tparam MeasurementBelief A belief state type modeling the ContinuousBeliefStateConcept with
 *         a unimodular gaussian representation.
 * \param sys The invariant discrete-time state-space system used in the state estimation.
 * \param state_space The state-space topology on which the state representations lie.
 * \param b_x As input, it stores the belief-state before the estimation step. As output, it stores
 *        the belief-state after the estimation step.
 * \param b_u The input vector to apply to the state-space system to make the transition of the 
 *        mean-state, i.e., the current input vector and its covariance.
 * \param b_z The output belief that was measured, i.e. the measurement vector and its covariance.
 * \param t The current time (before the prediction).
 * 
 */
template <typename InvariantSystem, 
          typename StateSpaceType,
          typename BeliefState, 
          typename InputBelief, 
          typename MeasurementBelief>
typename boost::enable_if< 
  boost::mpl::and_< 
    is_invariant_system<InvariantSystem>,
    is_continuous_belief_state<BeliefState>,
    boost::mpl::bool_< (belief_state_traits<BeliefState>::representation == belief_representation::gaussian) >,
    boost::mpl::bool_< (belief_state_traits<BeliefState>::distribution == belief_distribution::unimodal) >
  >,
void >::type invariant_kalman_filter_step(const InvariantSystem& sys,
                                          const StateSpaceType& state_space,
                                          BeliefState& b_x,
                                          const InputBelief& b_u,
                                          const MeasurementBelief& b_z,
                                          typename discrete_sss_traits<InvariantSystem>::time_type t = 0) {
  //here the requirement is that the system models a linear system which is at worse a linearized system
  // - if the system is LTI or LTV, then this will result in a basic Kalman Filter (KF) update
  // - if the system is linearized, then this will result in an Extended Kalman Filter (EKF) update
  BOOST_CONCEPT_ASSERT((pp::TopologyConcept< StateSpaceType >));
  BOOST_CONCEPT_ASSERT((InvariantDiscreteSystemConcept<InvariantSystem, StateSpaceType>));
  BOOST_CONCEPT_ASSERT((ContinuousBeliefStateConcept<BeliefState>));
  BOOST_CONCEPT_ASSERT((ContinuousBeliefStateConcept<InputBelief>));
  BOOST_CONCEPT_ASSERT((ContinuousBeliefStateConcept<MeasurementBelief>));

  typedef typename discrete_sss_traits<InvariantSystem>::point_type StateType;
  typedef typename continuous_belief_state_traits<BeliefState>::covariance_type CovType;
  typedef typename covariance_mat_traits< CovType >::matrix_type MatType;
  typedef typename mat_traits<MatType>::value_type ValueType;
  typedef typename invariant_system_traits<InvariantSystem>::invariant_frame_type InvarFrame;
  typedef typename invariant_system_traits<InvariantSystem>::invariant_correction_type InvarCorr;
  
  typename discrete_linear_sss_traits<InvariantSystem>::matrixA_type A;
  typename discrete_linear_sss_traits<InvariantSystem>::matrixB_type B;
  typename discrete_linear_sss_traits<InvariantSystem>::matrixC_type C;
  typename discrete_linear_sss_traits<InvariantSystem>::matrixD_type D;
  
  StateType x = b_x.get_mean_state(); 
  MatType P = b_x.get_covariance().get_matrix(); 
  
  StateType x_prior = sys.get_next_state(state_space, x, b_u.get_mean_state(), t); 
  sys.get_state_transition_blocks(A, B, state_space, t, t + sys.get_time_step(), x, x_prior, b_u.get_mean_state(), b_u.get_mean_state()); 
  InvarFrame W = sys.get_invariant_prior_frame(state_space, x, x_prior, b_u.get_mean_state(), t + sys.get_time_step()); 
  P = W * (( A * P * transpose_view(A)) + B * b_u.get_covariance().get_matrix() * transpose_view(B)) * transpose_view(W); 
  
  sys.get_output_function_blocks(C, D, state_space, t + sys.get_time_step(), x_prior, b_u.get_mean_state()); 
  vect_n<ValueType> e = to_vect<ValueType>(sys.get_invariant_error(state_space, x_prior, b_u.get_mean_state(), b_z.get_mean_state(), t + sys.get_time_step())); 
  
  mat< ValueType, mat_structure::rectangular, mat_alignment::column_major > CP = C * P; 
  mat< ValueType, mat_structure::symmetric > S(CP * transpose_view(C) + b_z.get_covariance().get_matrix()); 
  linsolve_Cholesky(S,CP); 
  mat< ValueType, mat_structure::rectangular, mat_alignment::row_major > K(transpose_view(CP)); 
  
  b_x.set_mean_state( sys.apply_correction(state_space, x_prior, from_vect<InvarCorr>(K * e), b_u.get_mean_state(), t + sys.get_time_step()) ); 
  W = sys.get_invariant_posterior_frame(state_space, x_prior, b_x.get_mean_state(), b_u.get_mean_state(), t + sys.get_time_step()); 
  b_x.set_covariance( CovType( MatType( W * ((mat< ValueType, mat_structure::identity>(K.get_row_count()) - K * C) * P) * transpose_view(W) ) ) ); 
};

template <typename InvariantSystem, 
          typename StateSpaceType,
          typename BeliefState, 
          typename InputBelief, 
          typename MeasurementBelief>
typename boost::disable_if< is_invariant_system<InvariantSystem>,
void >::type invariant_kalman_filter_step(const InvariantSystem& sys,
                                          const StateSpaceType& state_space,
                                          BeliefState& b_x,
                                          const InputBelief& b_u,
                                          const MeasurementBelief& b_z,
                                          typename discrete_sss_traits<InvariantSystem>::time_type t = 0) {
  kalman_filter_step(sys, state_space, b_x, b_u, b_z, t);
};




/**
 * This class template can be used as a belief-state predictor (and transfer) that uses the 
 * Invariant (Extended) Kalman Filter method. This class template models the BeliefTransferConcept and 
 * the BeliefPredictorConcept.
 * \tparam IKFTransferFactory The factory type which can create this kalman predictor.
 */
template <typename IKFTransferFactory>
struct IKF_belief_transfer {
  typedef IKF_belief_transfer<IKFTransferFactory> self;
  typedef typename IKFTransferFactory::state_space_system state_space_system;
  typedef shared_ptr< state_space_system > state_space_system_ptr;
  typedef typename discrete_sss_traits< state_space_system >::time_type time_type;
  typedef typename discrete_sss_traits< state_space_system >::time_difference_type time_difference_type;
  
  typedef covariance_matrix< vect_n<double> > covariance_type;
  
  typedef typename discrete_sss_traits< state_space_system >::input_type input_type;
  typedef typename discrete_sss_traits< state_space_system >::output_type output_type;
  
  typedef gaussian_belief_state<input_type,  covariance_type> input_belief_type;
  typedef gaussian_belief_state<output_type, covariance_type> output_belief_type;
  
  const IKFTransferFactory* factory;
  
  /**
   * Parametrized constructor.
   * \param aFactory A pointer to the factory object that is creating this object.
   */
  explicit IKF_belief_transfer(const IKFTransferFactory* aFactory = NULL) : factory(aFactory) { };
  
  /**
   * Returns the time-step of the predictor.
   * \return The time-step of the predictor.
   */
  time_difference_type get_time_step() const { return factory->get_time_step(); };

  /**
   * Returns a reference to the underlying state-space system.
   * \return A reference to the underlying state-space system.
   */
  const state_space_system_ptr& get_ss_system() const { return factory->get_state_space_system(); };

  /**
   * Returns the belief-state at the next time instant.
   * \tparam BeliefSpace The belief-space type on which to operate.
   * \param b_space The belief-space on which the belief-states lie.
   * \param b The current belief-state.
   * \param t The current time.
   * \param u The current input given to the system.
   * \param y The output that was measured at the next time instant.
   * \return the belief-state at the next time instant.
   */
  template <typename BeliefSpace>
  typename pp::topology_traits<BeliefSpace>::point_type
      get_next_belief(const BeliefSpace& b_space, typename pp::topology_traits<BeliefSpace>::point_type b, 
                      const time_type& t, const input_type& u, const output_type& y) const {
    invariant_kalman_filter_step(*(factory->get_state_space_system()), 
                                 b_space.get_state_topology(), b, 
                                 input_belief_type( u, covariance_type(factory->get_input_disturbance_cov())), 
                                 output_belief_type(y, covariance_type(factory->get_measurement_noise_cov())), 
                                 t);
    return b;
  };
  
  /**
   * Returns the prediction belief-state at the next time instant.
   * \tparam BeliefSpace The belief-space type on which to operate.
   * \param b_space The belief-space on which the belief-states lie.
   * \param b The current belief-state.
   * \param t The current time.
   * \param u The current input given to the system.
   * \return the belief-state at the next time instant, predicted by the filter.
   */
  template <typename BeliefSpace>
  typename pp::topology_traits<BeliefSpace>::point_type 
      predict_belief(const BeliefSpace& b_space, typename pp::topology_traits<BeliefSpace>::point_type b, 
                     const time_type& t, const input_type& u) const {
    invariant_kalman_predict(*(factory->get_state_space_system()), 
                             b_space.get_state_topology(), b, 
                             input_belief_type( u, covariance_type(factory->get_input_disturbance_cov())), 
                             t);
    return b;
  };
  
  /**
   * Converts a prediction belief-state into an updated belief-state which assumes the most likely measurement.
   * \tparam BeliefSpace The belief-space type on which to operate.
   * \param b_space The belief-space on which the belief-states lie.
   * \param b The current prediction's belief-state.
   * \param t The current time.
   * \param u The current input given to the system.
   * \return the updated belief-state when assuming the most likely measurement.
   */
  template <typename BeliefSpace>
  typename pp::topology_traits<BeliefSpace>::point_type 
      prediction_to_ML_belief(const BeliefSpace& b_space, typename pp::topology_traits<BeliefSpace>::point_type b, 
                              const time_type& t, const input_type& u) const {
    invariant_kalman_update(*(factory->get_state_space_system()), 
                            b_space.get_state_topology(), b, 
                            input_belief_type( u, covariance_type(factory->get_input_disturbance_cov())), 
                            output_belief_type(factory->get_state_space_system()->get_output(b_space.get_state_topology(),b.get_mean_state(),u,t),covariance_type(factory->get_measurement_noise_cov())), 
                            t);
    return b;
  };
  
  /**
   * Returns the prediction belief-state at the next time instant, assuming the upcoming measurement to be the most likely one.
   * \tparam BeliefSpace The belief-space type on which to operate.
   * \param b_space The belief-space on which the belief-states lie.
   * \param b The current belief-state.
   * \param t The current time.
   * \param u The current input given to the system.
   * \return the belief-state at the next time instant, predicted by the filter.
   */
  template <typename BeliefSpace>
  typename pp::topology_traits<BeliefSpace>::point_type 
      predict_ML_belief(const BeliefSpace& b_space, typename pp::topology_traits<BeliefSpace>::point_type b, 
                        const time_type& t, const input_type& u) const {
    input_belief_type b_u(u, covariance_type(factory->get_input_disturbance_cov()));
    invariant_kalman_predict(*(factory->get_state_space_system()), 
                             b_space.get_state_topology(), b, b_u, t);
    invariant_kalman_update(*(factory->get_state_space_system()), 
                            b_space.get_state_topology(), b, b_u, 
                            output_belief_type(factory->get_state_space_system()->get_output(b_space.get_state_topology(),b.get_mean_state(),u,t),covariance_type(factory->get_measurement_noise_cov())), 
                            t + factory->get_state_space_system()->get_time_step());
    return b;
  };
  
};





/**
 * This class is a factory class for invariant Kalman filtering predictors on a belief-space.
 * \tparam InvariantSystem An invariant discrete-time state-space system modeling the InvariantDiscreteSystemConcept.
 */
template <typename InvariantSystem>
class IKF_belief_transfer_factory : public serialization::serializable {
  public:
    typedef IKF_belief_transfer_factory<InvariantSystem> self;
    typedef IKF_belief_transfer<self> predictor_type;
    
    typedef InvariantSystem state_space_system;
    typedef shared_ptr< state_space_system > state_space_system_ptr;
    typedef covariance_matrix< vect_n<double> > covariance_type;
    typedef covariance_mat_traits< covariance_type >::matrix_type matrix_type;
    
    typedef typename discrete_sss_traits< state_space_system >::time_type time_type;
    typedef typename discrete_sss_traits< state_space_system >::time_difference_type time_difference_type;
    
    typedef typename discrete_sss_traits< state_space_system >::input_type input_type;
    
    template <typename BeliefSpace>
    struct predictor {
      typedef predictor_type type;
    };
    
  private:
    state_space_system_ptr sys; ///< Holds the reference to the system used for the filter.
    matrix_type Q; ///< Holds the system's input noise covariance matrix.
    matrix_type R; ///< Holds the system's output measurement's covariance matrix.
  public:
    
    
    /**
     * Parametrized constructor.
     * \param aSys The reference to the system used for the filter.
     * \param aQ The system's input noise covariance matrix.
     * \param aR The system's output measurement's covariance matrix.
     */
    IKF_belief_transfer_factory(const state_space_system_ptr& aSys = state_space_system_ptr(), 
                                const matrix_type& aQ = matrix_type(),
                                const matrix_type& aR = matrix_type()) : 
                                sys(aSys), Q(aQ), R(aR) { };
    
    /**
     * Returns the time-step of the discrete-time system.
     * \return The time-step of the discrete-time system.
     */
    time_difference_type get_time_step() const { return sys->get_time_step(); };
    
    /**
     * Sets the state-space system used by this kalman filter transfer factory.
     * \param aSys The new state-space system, by shared-pointer.
     */
    void set_state_space_system(const state_space_system_ptr& aSys) { sys = aSys; };
    /**
     * Gets the state-space system used by this kalman filter transfer factory.
     * \param aSys The new state-space system, by shared-pointer.
     */
    const state_space_system_ptr& get_state_space_system() const { return sys; };
    
    /**
     * Sets the system input disturbance covariance matrix used by this kalman filter transfer factory.
     * \param aQ The new system input disturbance covariance matrix.
     */
    void set_input_disturbance_cov(const matrix_type& aQ) { Q = aQ; };
    /**
     * Returns the system input disturbance covariance matrix used by this kalman filter transfer factory.
     * \return The system input disturbance covariance matrix.
     */
    const matrix_type& get_input_disturbance_cov() const { return Q; };
    
    /**
     * Sets the system measurement noise covariance matrix used by this kalman filter transfer factory.
     * \param aR The new system measurement noise covariance matrix.
     */
    void set_measurement_noise_cov(const matrix_type& aR) { R = aR; };
    /**
     * Returns the system measurement noise covariance matrix used by this kalman filter transfer factory.
     * \return The system measurement noise covariance matrix.
     */
    const matrix_type& get_measurement_noise_cov() const { return R; };
    
    template <typename BeliefSpace>
    predictor_type create_predictor(const BeliefSpace&, 
                                    const typename pp::topology_traits<BeliefSpace>::point_type*, 
                                    const time_type&, const input_type&) const {
      return predictor_type(this);
    };
    
    
/*******************************************************************************
                   ReaK's RTTI and Serialization interfaces
*******************************************************************************/
    
    virtual void RK_CALL save(serialization::oarchive& A, unsigned int) const {
      A & RK_SERIAL_SAVE_WITH_NAME(sys)
        & RK_SERIAL_SAVE_WITH_NAME(Q)
        & RK_SERIAL_SAVE_WITH_NAME(R);
    };
    
    virtual void RK_CALL load(serialization::iarchive& A, unsigned int) {
      A & RK_SERIAL_LOAD_WITH_NAME(sys)
        & RK_SERIAL_LOAD_WITH_NAME(Q)
        & RK_SERIAL_LOAD_WITH_NAME(R);
    };
    
    RK_RTTI_MAKE_ABSTRACT_1BASE(self,0xC2320002,1,"IKF_belief_transfer_factory",serialization::serializable)
};




};

};


#endif










