/**
 * \file rrtstar_path_planner.tpp
 * 
 * This library contains template definitions of a class to solve path planning problems using the 
 * Rapidly-exploring Random Tree Star (RRT*) algorithm (or one of its variants). 
 * Given a C_free (configuration space restricted to non-colliding points) and a 
 * result reporting policy, this class will probabilistically construct a motion-graph 
 * that will connect a starting point and a goal point with a path through C-free 
 * that is as close as possible to the optimal path in terms of distance.
 * 
 * \author Sven Mikael Persson <mikael.s.persson@gmail.com>
 * \date August 2012
 */

/*
 *    Copyright 2012 Sven Mikael Persson
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

#ifndef REAK_RRTSTAR_PATH_PLANNER_TPP
#define REAK_RRTSTAR_PATH_PLANNER_TPP

#include "rrtstar_path_planner.hpp"

#include <ReaK/ctrl/graph_alg/rrt_star.hpp>

#include "motion_graph_structures.hpp"


// BGL-Extra includes:
#include <boost/graph/tree_adaptor.hpp>
#include <boost/graph/more_property_tags.hpp>
#include <boost/graph/more_property_maps.hpp>


#include "metric_space_search.hpp"
#include "topological_search.hpp"

#include "p2p_planning_query.hpp"
#include "path_planner_options.hpp"
#include <ReaK/ctrl/graph_alg/neighborhood_functors.hpp>
#include "any_motion_graphs.hpp"
#include "planning_visitors.hpp"

namespace ReaK {
  
namespace pp {


template <typename FreeSpaceType, typename IsBidirPlanner = boost::mpl::false_>
struct rrtstar_bundle_factory {
  
  typedef boost::mpl::and_< IsBidirPlanner, is_reversible_space<FreeSpaceType> > is_bidir;
  
  typedef typename subspace_traits<FreeSpaceType>::super_space_type super_space_type;
  typedef typename topology_traits<super_space_type>::point_type point_type;
  
  typedef typename boost::mpl::if_< is_bidir,
    bidir_optimal_mg_vertex<FreeSpaceType>,
    optimal_mg_vertex<FreeSpaceType> >::type vertex_prop;
  typedef mg_vertex_data<FreeSpaceType> basic_vertex_prop;
  typedef optimal_mg_edge<FreeSpaceType> edge_prop;
  
  typedef typename motion_segment_directionality<FreeSpaceType>::type directionality_tag;
  
  typedef planning_visitor<FreeSpaceType> visitor_type;
  
  typedef boost::data_member_property_map<point_type, vertex_prop > position_map;
  typedef boost::data_member_property_map<double, vertex_prop> distance_map;
  typedef boost::data_member_property_map<std::size_t, vertex_prop> predecessor_map;
  typedef typename boost::mpl::if_< is_bidir,
    boost::data_member_property_map<double, vertex_prop>,
    ReaK::graph::detail::infinite_double_value_prop_map >::type fwd_distance_map;
    
  template <typename Graph>
  struct successor_map {
    typedef typename boost::mpl::if_< is_bidir,
      boost::data_member_property_map<std::size_t, vertex_prop>,
      ReaK::graph::detail::null_vertex_prop_map<Graph> >::type type;
  };
  
  typedef boost::data_member_property_map<double, edge_prop > weight_map;
  
  struct ls_motion_graph {
    typedef boost::adjacency_list_BC< boost::vecBC, boost::poolBC,
      directionality_tag, vertex_prop, edge_prop> type;
    typedef typename boost::graph_traits<type>::vertex_descriptor vertex_type;
    
    struct space_part_type {
      template <typename GraphPositionMap>
      space_part_type(const type&, const shared_ptr<const super_space_type>&, GraphPositionMap) { };
    };
    
    typedef linear_neighbor_search<type> nn_finder_type;
    static nn_finder_type get_nn_finder(type&, space_part_type&) { return nn_finder_type(); };
    
    typedef any_knn_synchro nn_synchro_type;
    static nn_synchro_type get_nn_synchro(nn_finder_type& ) {
      return nn_synchro_type();
    };
    
    static type get_motion_graph() {
      return type();
    };
  };
  
  template <unsigned int Arity, typename TreeStorageTag>
  struct dvp_motion_graph {
    typedef boost::adjacency_list_BC< boost::vecBC, boost::poolBC,
      directionality_tag, vertex_prop, edge_prop> type;
    typedef typename boost::graph_traits<type>::vertex_descriptor vertex_type;
    
    typedef typename boost::property_map< type, point_type basic_vertex_prop::* >::type graph_position_map;
    typedef dvp_tree<vertex_type, super_space_type, graph_position_map, Arity, random_vp_chooser, TreeStorageTag > space_part_type;
    static space_part_type get_space_part(type& mg, shared_ptr<const super_space_type> s_ptr) {
      return space_part_type(mg, s_ptr, get(&basic_vertex_prop::position, mg));
    };
    
    typedef multi_dvp_tree_search<type, space_part_type> nn_finder_type;
    static nn_finder_type get_nn_finder(type& mg, space_part_type& space_part) { 
      nn_finder_type nn_finder;
      nn_finder.graph_tree_map[&mg] = &space_part;
      return nn_finder;
    };
    
    typedef type_erased_knn_synchro< type, nn_finder_type > nn_synchro_type;
    static nn_synchro_type get_nn_synchro(nn_finder_type& nn_finder) {
      return nn_synchro_type(nn_finder);
    };
    
    static type get_motion_graph() {
      return type();
    };
  };
  
#ifdef RK_PLANNERS_ENABLE_DVP_ADJ_LIST_LAYOUT
  
  template <unsigned int Arity, typename TreeStorageTag>
  struct alt_motion_graph {
    typedef dvp_adjacency_list< vertex_prop, edge_prop, super_space_type, 
      position_map, Arity, random_vp_chooser, TreeStorageTag,
      boost::vecBC, directionality_tag, boost::listBC > alt_graph_type;
    typedef typename alt_graph_type::adj_list_type type;
    typedef alt_graph_type space_part_type;
    typedef typename boost::graph_traits<type>::vertex_descriptor vertex_type;
    
    static space_part_type get_space_part(shared_ptr<const super_space_type> s_ptr) {
      return space_part_type(s_ptr, position_map(&vertex_prop::position));
    };
    
    typedef multi_dvp_tree_search<type, space_part_type> nn_finder_type;
    static nn_finder_type get_nn_finder(type& mg, space_part_type& space_part) { 
      nn_finder_type nn_finder;
      nn_finder.graph_tree_map[&mg] = &space_part;
      return nn_finder;
    };
    
    typedef any_knn_synchro nn_synchro_type;
    static nn_synchro_type get_nn_synchro(nn_finder_type& ) {
      return nn_synchro_type();
    };
    
    static type get_motion_graph(space_part_type& space_part) {
      return space_part.get_adjacency_list();
    };
  };
  
#endif
  
  template <typename MotionGraphType>
  static void init_motion_graph(MotionGraphType& motion_graph, visitor_type& vis, planning_query<FreeSpaceType>& query) {
    typedef typename boost::graph_traits<MotionGraphType>::vertex_descriptor Vertex;
    vertex_prop vp_start;
    vp_start.position = query.get_start_position();
    Vertex start_node = add_vertex(vp_start, motion_graph);
    vis.m_start_node = boost::any( start_node );
    path_planning_p2p_query<FreeSpaceType>* p2p_query_ptr = reinterpret_cast< path_planning_p2p_query<FreeSpaceType>* >(query.castTo(path_planning_p2p_query<FreeSpaceType>::getStaticObjectType()));
    if( p2p_query_ptr ) {
      vertex_prop vp_goal;
      vp_goal.position = p2p_query_ptr->goal_pos;
      Vertex goal_node  = add_vertex(vp_goal,  motion_graph);
      vis.m_goal_node = boost::any( goal_node );
      vis.initialize_vertex(goal_node, motion_graph);
    };
    vis.initialize_vertex(start_node, motion_graph);
  };
  
  
  template <typename VertexProp>
  static
  typename boost::disable_if< boost::is_convertible<VertexProp*, bidir_optimal_mg_vertex<FreeSpaceType>*>,
  fwd_distance_map >::type dispatched_make_fwd_dist_map() {
    return fwd_distance_map();
  };
  
  template <typename VertexProp>
  static
  typename boost::enable_if< boost::is_convertible<VertexProp*, bidir_optimal_mg_vertex<FreeSpaceType>*>,
  fwd_distance_map >::type dispatched_make_fwd_dist_map() {
    return fwd_distance_map(&VertexProp::fwd_distance_accum);
  };
  
  template <typename MotionGraphType, typename VertexProp>
  static
  typename boost::disable_if< boost::is_convertible<VertexProp*, bidir_optimal_mg_vertex<FreeSpaceType>*>,
  typename successor_map<MotionGraphType>::type >::type dispatched_make_succ_map() {
    return typename successor_map<MotionGraphType>::type();
  };
  
  template <typename MotionGraphType, typename VertexProp>
  static
  typename boost::enable_if< boost::is_convertible<VertexProp*, bidir_optimal_mg_vertex<FreeSpaceType>*>,
  typename successor_map<MotionGraphType>::type >::type dispatched_make_succ_map() {
    return typename successor_map<MotionGraphType>::type(&VertexProp::successor);
  };
  
  template <typename MotionGraphType, typename NcSelector>
  static
  ReaK::graph::rrtstar_bundle<
    MotionGraphType, super_space_type, visitor_type, NcSelector, 
    position_map, weight_map,
    distance_map, predecessor_map, fwd_distance_map, typename successor_map<MotionGraphType>::type >
      make_bundle(MotionGraphType& motion_graph, 
                  visitor_type& vis, NcSelector nc_selector, 
                  shared_ptr<const super_space_type> s_ptr) {
    typedef typename boost::graph_traits<MotionGraphType>::vertex_descriptor Vertex;
    return ReaK::graph::make_rrtstar_bundle(
        motion_graph, boost::any_cast<Vertex>( vis.m_start_node ),
        ( vis.m_goal_node.empty() ? boost::graph_traits<MotionGraphType>::null_vertex() : boost::any_cast<Vertex>( vis.m_goal_node ) ),
        *s_ptr, vis, nc_selector, 
        position_map(&vertex_prop::position), 
        weight_map(&edge_prop::weight), 
        distance_map(&vertex_prop::distance_accum), 
        predecessor_map(&vertex_prop::predecessor),
        dispatched_make_fwd_dist_map<vertex_prop>(),
        dispatched_make_succ_map<MotionGraphType, vertex_prop>());
  };
  
  
  template <typename CallBidir, typename MotionGraphType, typename NcSelector> 
  static
  typename boost::disable_if< CallBidir >::type
    dispatched_make_call_to_planner(MotionGraphType& motion_graph, visitor_type& vis, NcSelector nc_selector, 
                                    shared_ptr<const super_space_type> s_ptr, std::size_t method_flags) {
    
    if(method_flags & USE_BRANCH_AND_BOUND_PRUNING_FLAG) {
      ReaK::graph::generate_bnb_rrt_star( make_bundle(motion_graph, vis, nc_selector, s_ptr), get(random_sampler, *s_ptr) );
    } else { /* assume nominal method only. */
      ReaK::graph::generate_rrt_star( make_bundle(motion_graph, vis, nc_selector, s_ptr), get(random_sampler, *s_ptr) );
    };
    
  };
  
  template <typename CallBidir, typename MotionGraphType, typename NcSelector> 
  static
  typename boost::enable_if< CallBidir >::type
    dispatched_make_call_to_planner(MotionGraphType& motion_graph, visitor_type& vis, NcSelector nc_selector, 
                                    shared_ptr<const super_space_type> s_ptr, std::size_t method_flags) {
    
    if(method_flags & USE_BRANCH_AND_BOUND_PRUNING_FLAG) {
      ReaK::graph::generate_bnb_rrt_star_bidir( make_bundle(motion_graph, vis, nc_selector, s_ptr), get(random_sampler, *s_ptr) );
    } else { /* assume nominal method only. */
      ReaK::graph::generate_rrt_star_bidir( make_bundle(motion_graph, vis, nc_selector, s_ptr), get(random_sampler, *s_ptr) );
    };
    
  };
  
  template <typename MotionGraphType, typename NcSelector> 
  static
  void make_call_to_planner(MotionGraphType& motion_graph, visitor_type& vis, NcSelector nc_selector, 
                            shared_ptr<const super_space_type> s_ptr, std::size_t method_flags) {
    dispatched_make_call_to_planner<is_bidir>(motion_graph, vis, nc_selector, s_ptr, method_flags);
  };
  
  
};


template <typename FreeSpaceType>
template <typename RRTStarFactory>
void rrtstar_planner<FreeSpaceType>::solve_planning_query_impl(planning_query<FreeSpaceType>& aQuery) {
  
  this->reset_internal_state();
  
  double space_dim = double( this->get_space_dimensionality() );
  double space_Lc = aQuery.get_heuristic_to_goal( aQuery.get_start_position() );
  
  typedef typename subspace_traits<FreeSpaceType>::super_space_type  SuperSpace;
  shared_ptr<const SuperSpace> sup_space_ptr(&(this->m_space->get_super_space()),null_deleter());
  
  // Some MACROs to reduce the size of the code below.
  
#define RK_RRTSTAR_PLANNER_SETUP_LS_OR_DVP_SUPPORT_STRUCTURES                                           \
    typedef typename MGFactory::type         MotionGraphType;                                           \
    MotionGraphType motion_graph = MGFactory::get_motion_graph();                                       \
    RRTStarFactory::init_motion_graph(motion_graph, vis, aQuery);                                       \
                                                                                                        \
    typedef typename MGFactory::space_part_type     SpacePartType;                                      \
    typedef typename RRTStarFactory::basic_vertex_prop   BasicVProp;                                    \
    SpacePartType space_part(motion_graph, sup_space_ptr, get(&BasicVProp::position, motion_graph));    \
                                                                                                        \
    typedef typename MGFactory::nn_finder_type      NNFinderType;                                       \
    NNFinderType nn_finder = MGFactory::get_nn_finder(motion_graph, space_part);                        \
                                                                                                        \
    ReaK::graph::star_neighborhood< NNFinderType > nc_selector(nn_finder, space_dim, 3.0 * space_Lc);   \
                                                                                                        \
    typedef typename MGFactory::nn_synchro_type NNSynchroType;                                          \
    NNSynchroType NN_synchro = MGFactory::get_nn_synchro(nn_finder);                                    \
    vis.m_nn_synchro = &NN_synchro;
  
  
#define RK_RRTSTAR_PLANNER_SETUP_ALT_SUPPORT_STRUCTURES                                                 \
    typedef typename MGFactory::type         MotionGraphType;                                           \
    typedef typename MGFactory::space_part_type     SpacePartType;                                      \
    typedef typename RRTStarFactory::position_map        PosMap;                                        \
    typedef typename RRTStarFactory::vertex_prop         VProp;                                         \
    SpacePartType space_part(sup_space_ptr, PosMap(&VProp::position));                                  \
                                                                                                        \
    MotionGraphType motion_graph = MGFactory::get_motion_graph(space_part);                             \
                                                                                                        \
    typedef typename MGFactory::nn_finder_type      NNFinderType;                                       \
    NNFinderType nn_finder = MGFactory::get_nn_finder(motion_graph, space_part);                        \
                                                                                                        \
    ReaK::graph::star_neighborhood< NNFinderType > nc_selector(nn_finder, space_dim, 3.0 * space_Lc);   \
                                                                                                        \
    typedef typename MGFactory::nn_synchro_type NNSynchroType;                                          \
    NNSynchroType NN_synchro = MGFactory::get_nn_synchro(nn_finder);                                    \
    vis.m_nn_synchro = &NN_synchro;                                                                     \
                                                                                                        \
    RRTStarFactory::init_motion_graph(motion_graph, vis, aQuery); 
  
//     ReaK::graph::fixed_neighborhood< NNFinderType > nc_selector(nn_finder, 10, this->get_sampling_radius()); 
  
  
  typedef typename RRTStarFactory::visitor_type VisitorType;
  VisitorType vis(this, &aQuery);
  
  if((this->m_data_structure_flags & MOTION_GRAPH_STORAGE_MASK) == ADJ_LIST_MOTION_GRAPH) {
    
    if((this->m_data_structure_flags & KNN_METHOD_MASK) == LINEAR_SEARCH_KNN) {
      
      typedef typename RRTStarFactory::ls_motion_graph MGFactory;
      
      RK_RRTSTAR_PLANNER_SETUP_LS_OR_DVP_SUPPORT_STRUCTURES
      
      RRTStarFactory::make_call_to_planner(motion_graph, vis, nc_selector, sup_space_ptr, this->m_planning_method_flags);
      
    } else if((this->m_data_structure_flags & KNN_METHOD_MASK) == DVP_BF2_TREE_KNN) {
      
      typedef typename RRTStarFactory::template dvp_motion_graph< 2, boost::bfl_d_ary_tree_storage<2> > MGFactory;
      
      RK_RRTSTAR_PLANNER_SETUP_LS_OR_DVP_SUPPORT_STRUCTURES
      
      RRTStarFactory::make_call_to_planner(motion_graph, vis, nc_selector, sup_space_ptr, this->m_planning_method_flags);
      
    } else if((this->m_data_structure_flags & KNN_METHOD_MASK) == DVP_BF4_TREE_KNN) {
      
      typedef typename RRTStarFactory::template dvp_motion_graph< 4, boost::bfl_d_ary_tree_storage<4> > MGFactory;
      
      RK_RRTSTAR_PLANNER_SETUP_LS_OR_DVP_SUPPORT_STRUCTURES
      
      RRTStarFactory::make_call_to_planner(motion_graph, vis, nc_selector, sup_space_ptr, this->m_planning_method_flags);
      
#ifdef RK_PLANNERS_ENABLE_VEBL_TREE
        
    } else if((this->m_data_structure_flags & KNN_METHOD_MASK) == DVP_COB2_TREE_KNN) {
      
      typedef typename RRTStarFactory::template dvp_motion_graph< 2, boost::vebl_d_ary_tree_storage<2> > MGFactory;
      
      RK_RRTSTAR_PLANNER_SETUP_LS_OR_DVP_SUPPORT_STRUCTURES
      
      RRTStarFactory::make_call_to_planner(motion_graph, vis, nc_selector, sup_space_ptr, this->m_planning_method_flags);
      
    } else if((this->m_data_structure_flags & KNN_METHOD_MASK) == DVP_COB4_TREE_KNN) {
      
      typedef typename RRTStarFactory::template dvp_motion_graph< 4, boost::vebl_d_ary_tree_storage<4> > MGFactory;
      
      RK_RRTSTAR_PLANNER_SETUP_LS_OR_DVP_SUPPORT_STRUCTURES
      
      RRTStarFactory::make_call_to_planner(motion_graph, vis, nc_selector, sup_space_ptr, this->m_planning_method_flags);
      
#endif
      
    };
    
#ifdef RK_PLANNERS_ENABLE_DVP_ADJ_LIST_LAYOUT
    
  } else if((this->m_data_structure_flags & MOTION_GRAPH_STORAGE_MASK) == DVP_ADJ_LIST_MOTION_GRAPH) {
    
    if((this->m_data_structure_flags & KNN_METHOD_MASK) == DVP_BF2_TREE_KNN) {
      
      typedef typename RRTStarFactory::template alt_motion_graph< 2, boost::bfl_d_ary_tree_storage<2> > MGFactory;
      
      RK_RRTSTAR_PLANNER_SETUP_ALT_SUPPORT_STRUCTURES
      
      RRTStarFactory::make_call_to_planner(motion_graph, vis, nc_selector, sup_space_ptr, this->m_planning_method_flags);
      
    } else if((this->m_data_structure_flags & KNN_METHOD_MASK) == DVP_BF4_TREE_KNN) {
      
      typedef typename RRTStarFactory::template alt_motion_graph< 4, boost::bfl_d_ary_tree_storage<4> > MGFactory;
      
      RK_RRTSTAR_PLANNER_SETUP_ALT_SUPPORT_STRUCTURES
      
      RRTStarFactory::make_call_to_planner(motion_graph, vis, nc_selector, sup_space_ptr, this->m_planning_method_flags);
      
#ifdef RK_PLANNERS_ENABLE_VEBL_TREE
      
    } else if((this->m_data_structure_flags & KNN_METHOD_MASK) == DVP_COB2_TREE_KNN) {
      
      typedef typename RRTStarFactory::template alt_motion_graph< 2, boost::vebl_d_ary_tree_storage<2> > MGFactory;
      
      RK_RRTSTAR_PLANNER_SETUP_ALT_SUPPORT_STRUCTURES
      
      RRTStarFactory::make_call_to_planner(motion_graph, vis, nc_selector, sup_space_ptr, this->m_planning_method_flags);
      
    } else if((this->m_data_structure_flags & KNN_METHOD_MASK) == DVP_COB4_TREE_KNN) {
      
      typedef typename RRTStarFactory::template alt_motion_graph< 4, boost::vebl_d_ary_tree_storage<4> > MGFactory;
      
      RK_RRTSTAR_PLANNER_SETUP_ALT_SUPPORT_STRUCTURES
      
      RRTStarFactory::make_call_to_planner(motion_graph, vis, nc_selector, sup_space_ptr, this->m_planning_method_flags);
        
#endif
      
    };
    
#endif
    
  };
  
#undef RK_RRTSTAR_PLANNER_SETUP_LS_OR_DVP_SUPPORT_STRUCTURES
#undef RK_RRTSTAR_PLANNER_SETUP_ALT_SUPPORT_STRUCTURES
  
};



template <typename FreeSpaceType>
void rrtstar_planner<FreeSpaceType>::solve_planning_query(planning_query<FreeSpaceType>& aQuery) {
  
  if((this->m_planning_method_flags & PLANNING_DIRECTIONALITY_MASK) == UNIDIRECTIONAL_PLANNING) {
    
    this->solve_planning_query_impl< rrtstar_bundle_factory<FreeSpaceType> >(aQuery);
    
  } else {  /* NOTE: Bi-directional version: */
    
    this->solve_planning_query_impl< rrtstar_bundle_factory<FreeSpaceType, boost::mpl::true_ > >(aQuery);
    
  };
  
};


};

};

#endif

