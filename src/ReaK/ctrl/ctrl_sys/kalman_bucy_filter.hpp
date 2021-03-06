
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

#ifndef REAK_KALMAN_BUCY_FILTER_HPP
#define REAK_KALMAN_BUCY_FILTER_HPP

#include <ReaK/core/lin_alg/vect_concepts.hpp>
#include <ReaK/core/lin_alg/mat_alg.hpp>
#include <ReaK/core/lin_alg/mat_cholesky.hpp>
#include <ReaK/core/integrators/integrator.hpp>

#include "belief_state_concept.hpp"
#include "linear_ss_system_concept.hpp"
#include "covariance_concept.hpp"

#include <boost/utility/enable_if.hpp>
#include <boost/static_assert.hpp>


namespace ReaK {

namespace ctrl {


namespace detail {
  
  
  template <typename T,
            typename LinearSystem,
            typename StateSpaceType,
            typename SystemNoiseCovariance,
            typename MeasurementNoiseCovariance>
  struct kalman_bucy_system : public state_rate_function< T > {
    
    typedef T value_type;
    typedef std::size_t size_type;
    typedef typename ss_system_traits<LinearSystem>::point_type state_type;
    typedef typename ss_system_traits<LinearSystem>::input_type input_type;
    typedef typename ss_system_traits<LinearSystem>::output_type output_type;
  
    typedef typename linear_ss_system_traits<LinearSystem>::matrixA_type matrixA_type;
    typedef typename linear_ss_system_traits<LinearSystem>::matrixB_type matrixB_type;
    typedef typename linear_ss_system_traits<LinearSystem>::matrixC_type matrixC_type;
    typedef typename linear_ss_system_traits<LinearSystem>::matrixD_type matrixD_type;
    
    BOOST_CONCEPT_ASSERT((LinearSSSystemConcept< LinearSystem, StateSpaceType, LinearizedSystemType >));
    BOOST_CONCEPT_ASSERT((CovarianceMatrixConcept<SystemNoiseCovariance,input_type>));
    BOOST_CONCEPT_ASSERT((CovarianceMatrixConcept<MeasurementNoiseCovariance,output_type>));
    
    const LinearSystem& sys;
    const StateSpaceType& state_space;
    const input_type& u;
    const output_type& z;
    
    mat<value_type, mat_structure::symmetric> Q;
    mat<value_type, mat_structure::symmetric> R_inv;
    mat<value_type, mat_structure::rectangular> Kt;
    mat<value_type, mat_structure::square> P;
    
    matrixA_type A;
    matrixB_type B;
    matrixC_type C;
    matrixD_type D;
    
    kalman_bucy_system(const LinearSystem& aSys, const StateSpaceType& aStateSpace,
                       const input_type& aU, const output_type& aZ, 
                       const SystemNoiseCovariance& aQ, const MeasurementNoiseCovariance& aR) :
                       sys(aSys), state_space(aStateSpace), u(aU), z(aZ), Q(aQ.get_matrix()) {
      invert_Cholesky(aR.get_matrix(),R_inv);
      Kt.set_row_count(R_inv.get_row_count());
      Kt.set_col_count(Q.get_row_count());
      P.set_row_count(Q.get_row_count());
    };
    
    virtual void RK_CALL computeStateRate(double aTime,const ReaK::vect_n<value_type>& aState, ReaK::vect_n<value_type>& aStateRate) {
      vect_n<value_type> x;
      x.resize(Q.get_row_count());
      for(size_type i = 0; i < x.size(); ++i) 
        x[i] = aState[i];
      
      sys.get_linear_blocks(A, B, C, D, state_space, aTime, from_vect<state_type>(x), u);
      
      for(size_type j = 0; j < x.size(); ++j)
        for(size_type i = 0; i < x.size(); ++i)
          P(i,j) = aState[x.size() * (j + 1) + i];
      
      Kt = R_inv * C * P;
      x = A * x + B * to_vect<value_type>(u) + (to_vect<value_type>(z) - C * x - D * to_vect<value_type>(u)) * Kt;
      P = A * P + B * Q + Q * transpose_view(B) + P * (transpose_view(A) - transpose_view(C) * Kt);
      
      for(size_type i = 0; i < x.size(); ++i) 
        aStateRate[i] = x[i];

      for(size_type j = 0; j < x.size(); ++j)
        for(size_type i = 0; i < x.size(); ++i)
          aStateRate[x.size() * (j + 1) + i] = 0.5 * (P(i,j) + P(j,i));
    };
  };
  
  
};





template <typename LinearSystem, 
          typename StateSpaceType,
          typename BeliefState, 
          typename InputBelief, 
          typename MeasurementBelief>
typename boost::enable_if_c< is_continuous_belief_state<BeliefState>::value &&
                             (belief_state_traits<BeliefState>::representation == belief_representation::gaussian) &&
                             (belief_state_traits<BeliefState>::distribution == belief_distribution::unimodal),
void >::type kalman_bucy_filter_step(const LinearSystem& sys,
                                     integrator< typename mat_traits< typename covariance_mat_traits< typename continuous_belief_state_traits<BeliefState>::covariance_type >::matrix_type >::value_type >& integ,
                                     const StateSpaceType& state_space,
                                     BeliefState& b_x,
                                     const InputBelief& b_u,
                                     const MeasurementBelief& b_z,
                                     typename ss_system_traits<LinearSystem>::time_difference_type dt,
                                     typename ss_system_traits<LinearSystem>::time_type t = 0) {
  //here the requirement is that the system models a linear system which is at worse a linearized system
  // - if the system is LTI or LTV, then this will result in a basic Kalman Filter (KF) prediction
  // - if the system is linearized, then this will result in an Extended Kalman Filter (EKF) prediction
  BOOST_CONCEPT_ASSERT((LinearSSSystemConcept< LinearSystem, StateSpaceType, LinearizedSystemType >));
  BOOST_CONCEPT_ASSERT((ContinuousBeliefStateConcept<BeliefState>));
  BOOST_CONCEPT_ASSERT((ContinuousBeliefStateConcept<InputBelief>));
  BOOST_CONCEPT_ASSERT((ContinuousBeliefStateConcept<MeasurementBelief>));
  
  typedef typename ss_system_traits<LinearSystem>::point_type StateType;
  typedef typename continuous_belief_state_traits<BeliefState>::covariance_type CovType;
  typedef typename covariance_mat_traits< CovType >::matrix_type MatType;
  typedef typename mat_traits< MatType >::value_type ValueType;
  typedef typename mat_traits< MatType >::size_type SizeType;
  
  typedef typename continuous_belief_state_traits<InputBelief>::covariance_type InputCovType;
  typedef typename continuous_belief_state_traits<MeasurementBelief>::covariance_type OutputCovType;
  
  integ.setTime(t);
  integ.clearStateVector();
  vect_n<ValueType> x = to_vect<ValueType>(b_x.get_mean_state());
  integ.addStateElements(x);
  mat<ValueType, mat_structure::square> P = mat<ValueType, mat_structure::square>(b_x.get_covariance().get_matrix());
  
  for(SizeType j = 0; j < P.get_col_count(); ++j) 
    for(SizeType i = 0; i < P.get_row_count(); ++i)
      integ.addStateElement(P(i,j));
  
  boost::shared_ptr< state_rate_function<ValueType> > integ_sys =
    boost::shared_ptr< state_rate_function<ValueType> >( 
      new detail::kalman_bucy_system<ValueType, 
                                     LinearSystem, 
                                     StateSpaceType,
                                     InputCovType, 
                                     OutputCovType>( sys, state_space, 
                                                     b_u.get_mean_state(), 
                                                     b_z.get_mean_state(), 
                                                     b_u.get_covariance(), 
                                                     b_z.get_covariance() ) );
    
  integ.setStateRateFunc( integ_sys );
  
  integ.integrate(t + dt);
  
  typename std::vector<ValueType>::const_iterator it = integ.getStateBegin();
  for(SizeType i = 0; i < x.size(); ++it, ++i)
    x[i] = *it;
  
  for(SizeType j = 0; j < P.get_col_count(); ++j) 
    for(SizeType i = 0; i < P.get_row_count(); ++i, ++it)
      P(i,j) = *it;
  
  b_x.set_mean_state( from_vect<StateType>(x) );
  b_x.set_covariance( CovType( MatType(P) ) );
  
  integ.setStateRateFunc( boost::shared_ptr< state_rate_function<ValueType> >() );
};




};

};

#endif










