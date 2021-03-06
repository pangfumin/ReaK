/**
 * \file proxy_traj_applicator.hpp
 * 
 * This library defines a class for applying a trajectory to a given static model applicator.
 * This class can be used for updating a proximity-query model with time such that the model 
 * is always synchronized, in its configuration, with a given time (e.g., current planning time).
 * 
 * \author Sven Mikael Persson <mikael.s.persson@gmail.com>
 * \date November 2013
 */

/*
 *    Copyright 2013 Sven Mikael Persson
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

#ifndef REAK_PROXY_TRAJ_APPLICATOR_HPP
#define REAK_PROXY_TRAJ_APPLICATOR_HPP

#include <ReaK/core/base/defs.hpp>

#include "proxy_model_updater.hpp"

#include "metric_space_concept.hpp"
#include <ReaK/ctrl/interpolation/spatial_trajectory_concept.hpp>  // for SpatialTrajectoryConcept


namespace ReaK {

namespace pp {




/**
 * This class is for applying a trajectory to a given static model applicator.
 * This class can be used for updating a proximity-query model with time such that the model 
 * is always synchronized, in its configuration, with a given time (e.g., current planning time).
 */
template <typename JointTrajectory>
class proxy_traj_applicator : public proxy_model_updater {
  public:
    
    typedef proxy_traj_applicator< JointTrajectory > self;
    typedef typename spatial_trajectory_traits< JointTrajectory >::topology temporal_space_type;
    typedef typename temporal_space_traits< temporal_space_type >::space_topology joint_space_type;
    typedef typename topology_traits< temporal_space_type >::point_type point_type;
    typedef typename spatial_trajectory_traits< JointTrajectory >::const_waypoint_descriptor wp_desc_type;
    
    BOOST_CONCEPT_ASSERT((SpatialTrajectoryConcept<JointTrajectory, temporal_space_type>));
    
    /** This data member points to a manipulator kinematics model to use for the mappings performed. */
    shared_ptr< proxy_model_applicator<joint_space_type> > static_applicator;
  private:
    shared_ptr< JointTrajectory > traj;
    mutable std::pair<wp_desc_type, point_type> last_wp;
  public:
    
    void set_trajectory(const shared_ptr< JointTrajectory >& aTraj) {
      traj = aTraj;
      if(traj)
        last_wp = traj->get_waypoint_at_time(traj->get_start_time());
    };
    
    /**
     * Parametric / default constructor.
     * \param aStaticApplicator The static applicator for the proximity-query model.
     * \param aTraj The joint-space trajectory of the proximity-query model, i.e., tracks its state over time.
     */
    proxy_traj_applicator(const shared_ptr< proxy_model_applicator<joint_space_type> >& aStaticApplicator = shared_ptr< proxy_model_applicator<joint_space_type> >(),
                          const shared_ptr< JointTrajectory >& aTraj = shared_ptr< JointTrajectory >()) :
                          static_applicator(aStaticApplicator), traj(aTraj) {
      if(traj)
        last_wp = traj->get_waypoint_at_time(traj->get_start_time());
    };
    
    
    virtual void synchronize_proxy_model(double t) const {
      if( (!traj) || (!static_applicator) )
        return;
      
      last_wp = traj->move_time_diff_from(last_wp, t - last_wp.second.time);
      
      static_applicator->apply_to_model(last_wp.second.pt, traj->get_temporal_space().get_space_topology());
      
    };
    
/*******************************************************************************
                   ReaK's RTTI and Serialization interfaces
*******************************************************************************/

    virtual void RK_CALL save(ReaK::serialization::oarchive& A, unsigned int) const {
      proxy_model_updater::save(A,proxy_model_updater::getStaticObjectType()->TypeVersion());
      A & RK_SERIAL_SAVE_WITH_NAME(static_applicator)
        & RK_SERIAL_SAVE_WITH_NAME(traj);
    };
    virtual void RK_CALL load(ReaK::serialization::iarchive& A, unsigned int) {
      proxy_model_updater::load(A,proxy_model_updater::getStaticObjectType()->TypeVersion());
      A & RK_SERIAL_LOAD_WITH_NAME(static_applicator)
        & RK_SERIAL_LOAD_WITH_NAME(traj);
      if(traj)
        last_wp = traj->get_waypoint_at_time(traj->get_start_time());
    };

    RK_RTTI_MAKE_CONCRETE_1BASE(self,0xC240002A,1,"proxy_traj_applicator",proxy_model_updater)
    
    
};




};

};

#endif

