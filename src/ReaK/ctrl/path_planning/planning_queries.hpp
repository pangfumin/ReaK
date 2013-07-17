/**
 * \file planning_queries.hpp
 * 
 * This library defines class templates to encode a path-planning or motion-planning query as the 
 * contract to be fulfilled by path-planners used in ReaK. 
 * 
 * \author Sven Mikael Persson <mikael.s.persson@gmail.com>
 * \date July 2013
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

#ifndef REAK_PLANNING_QUERIES_HPP
#define REAK_PLANNING_QUERIES_HPP

#include "base/defs.hpp"
#include "base/named_object.hpp"

#include "metric_space_concept.hpp"
#include "steerable_space_concept.hpp"
#include "random_sampler_concept.hpp"
#include "subspace_concept.hpp"

#include "trajectory_base.hpp"
#include "seq_path_base.hpp"
#include "seq_path_wrapper.hpp"
#include "interpolation/point_to_point_path.hpp"
#include "interpolation/discrete_point_path.hpp"

#include "any_motion_graphs.hpp"

#include <boost/mpl/if.hpp>

#include <map>

namespace ReaK {
  
namespace pp {



/**
 * This class is the basic OOP interface for a path planner. 
 * OOP-style planners are useful to hide away
 * the cumbersome details of calling the underlying planning algorithms which are 
 * generic programming (GP) style and thus provide a lot more flexibility but are difficult
 * to deal with in the user-space. The OOP planners are meant to offer a much simpler interface,
 * i.e., a member function that "solves the problem" and returns the solution path or trajectory.
 */
template <typename FreeSpaceType>
class planning_query : public named_object {
  public:
    typedef planning_query<FreeSpaceType> self;
    typedef FreeSpaceType space_type;
    typedef typename subspace_traits<FreeSpaceType>::super_space_type super_space_type;
    
    BOOST_CONCEPT_ASSERT((SubSpaceConcept<FreeSpaceType>));
    
    typedef typename topology_traits< super_space_type >::point_type point_type;
    typedef typename topology_traits< super_space_type >::point_difference_type point_difference_type;
    
    typedef typename boost::mpl::if_< is_temporal_space<space_type>,
      trajectory_base< super_space_type >,
      seq_path_base< super_space_type > >::type solution_base_type;
    
    typedef shared_ptr< solution_base_type > solution_record_ptr;
    
  public:
    
    shared_ptr< space_type > space;
    
    /**
     * Returns the best solution distance registered in this query object.
     * \return The best solution distance registered in this query object.
     */
    virtual double get_best_solution_distance() const {
      return std::numeric_limits<double>::infinity();
    };
    
    /**
     * Returns true if the solver should keep on going trying to solve the path-planning problem.
     * \return True if the solver should keep on going trying to solve the path-planning problem.
     */
    virtual bool keep_going() const { return true; };
    
    /**
     * This function is called to reset the internal state of the planner.
     */
    virtual void reset_solution_records() { };
    
    virtual const point_type& get_start_position() const = 0;
    
    /**
     * This function returns the distance of the collision-free travel from the given point to the 
     * goal region.
     * \param pos The position from which to try and reach the goal.
     * \return The distance of the collision-free travel from the given point to the goal region.
     */
    virtual double get_distance_to_goal(const point_type& pos) { return std::numeric_limits<double>::infinity(); };
    
    /**
     * This function returns the heuristic distance of the bird-flight travel from the given 
     * point to the goal region.
     * \param pos The position from which to reach the goal.
     * \return The heuristic distance of the bird-flight travel from the given 
     * point to the goal region.
     */
    virtual double get_heuristic_to_goal(const point_type& pos) { return std::numeric_limits<double>::infinity(); };
    
  protected:
    
    virtual solution_record_ptr 
      register_solution_from_optimal_mg(graph::any_graph::vertex_descriptor start_node, 
                                        graph::any_graph::vertex_descriptor goal_node, 
                                        double goal_distance,
                                        graph::any_graph& g) = 0;
    
    virtual solution_record_ptr 
      register_solution_from_basic_mg(graph::any_graph::vertex_descriptor start_node, 
                                      graph::any_graph::vertex_descriptor goal_node, 
                                      double goal_distance,
                                      graph::any_graph& g) = 0;
    
    virtual solution_record_ptr 
      register_joining_point_from_optimal_mg(graph::any_graph::vertex_descriptor start_node, 
                                             graph::any_graph::vertex_descriptor goal_node, 
                                             graph::any_graph::vertex_descriptor join1_node, 
                                             graph::any_graph::vertex_descriptor join2_node, 
                                             double goal_distance,
                                             graph::any_graph& g1, 
                                             graph::any_graph& g2) = 0;
    
    virtual solution_record_ptr 
      register_joining_point_from_basic_mg(graph::any_graph::vertex_descriptor start_node, 
                                           graph::any_graph::vertex_descriptor goal_node, 
                                           graph::any_graph::vertex_descriptor join1_node, 
                                           graph::any_graph::vertex_descriptor join2_node, 
                                           double goal_distance,
                                           graph::any_graph& g1, 
                                           graph::any_graph& g2) = 0;
    
  public:
    
    /**
     * This function registers a solution path (if one is found) and invokes the path-planning 
     * reporter to report on that solution path.
     * \note This function works for optimal motion graphs (optimal: uses a shortest-distance rewiring strategy).
     * \param start_node The start node in the motion-graph.
     * \param goal_node The goal node in the motion-graph.
     * \param g The current motion-graph.
     * \return True if a new solution was registered.
     */
    template <typename Vertex, typename Graph>
    typename boost::enable_if< boost::is_convertible< typename Graph::vertex_bundled*, optimal_mg_vertex< FreeSpaceType >* >,
    solution_record_ptr >::type register_solution(Vertex start_node, Vertex goal_node, double goal_distance, Graph& g) {
      typedef any_optimal_motion_graph<FreeSpaceType, Graph> TEGraph;
      typedef typename boost::graph_traits<TEGraph>::vertex_descriptor TEVertex;
      
      TEGraph te_g( &g );
      TEVertex te_start = TEVertex( boost::any( start_node) );
      TEVertex te_goal  = TEVertex( boost::any( goal_node ) );
      
      return register_solution_from_optimal_mg(te_start, te_goal, goal_distance, te_g);
    };
    
    /**
     * This function registers a solution path (if one is found) and invokes the path-planning 
     * reporter to report on that solution path.
     * \note This function works for basic motion graphs.
     * \param start_node The start node in the motion-graph.
     * \param goal_node The goal node in the motion-graph.
     * \param g The current motion-graph.
     * \return True if a new solution was registered.
     */
    template <typename Vertex, typename Graph>
    typename boost::disable_if< boost::is_convertible< typename Graph::vertex_bundled*, optimal_mg_vertex< FreeSpaceType >* >,
    solution_record_ptr >::type register_solution(Vertex start_node, Vertex goal_node, double goal_distance, Graph& g) {
      typedef any_motion_graph<FreeSpaceType, Graph> TEGraph;
      typedef typename boost::graph_traits<TEGraph>::vertex_descriptor TEVertex;
      
      TEGraph te_g( &g );
      TEVertex te_start = TEVertex( boost::any( start_node) );
      TEVertex te_goal  = TEVertex( boost::any( goal_node ) );
      
      return register_solution_from_basic_mg(te_start, te_goal, goal_distance, te_g);
    };
    
    
    
    /**
     * This function registers a solution path (if one is found) and invokes the path-planning 
     * reporter to report on that solution path.
     * \note This function works for optimal motion graphs (optimal: uses a shortest-distance rewiring strategy).
     * \param start_node The start node in the motion-graph.
     * \param goal_node The goal node in the motion-graph.
     * \param g The current motion-graph.
     * \return True if a new solution was registered.
     */
    template <typename Vertex, typename Graph>
    typename boost::enable_if< boost::is_convertible< typename Graph::vertex_bundled*, optimal_mg_vertex< FreeSpaceType >* >,
    solution_record_ptr >::type register_joining_point(Vertex start_node, Vertex goal_node, 
                                        Vertex join1_node, Vertex join2_node, 
                                        double joining_distance, 
                                        Graph& g1, Graph& g2) {
      typedef any_optimal_motion_graph<FreeSpaceType, Graph> TEGraph;
      typedef typename boost::graph_traits<TEGraph>::vertex_descriptor TEVertex;
      
      TEGraph te_g1( &g1 );
      TEGraph te_g2( &g2 );
      TEVertex te_start = TEVertex( boost::any( start_node) );
      TEVertex te_goal  = TEVertex( boost::any( goal_node ) );
      TEVertex te_join1 = TEVertex( boost::any( join1_node) );
      TEVertex te_join2 = TEVertex( boost::any( join2_node) );
      
      return register_joining_point_from_optimal_mg(te_start, te_goal, te_join1, te_join2, joining_distance, te_g1, te_g2);
    };
    
    /**
     * This function registers a solution path (if one is found) and invokes the path-planning 
     * reporter to report on that solution path.
     * \note This function works for basic motion graphs.
     * \param start_node The start node in the motion-graph.
     * \param goal_node The goal node in the motion-graph.
     * \param g The current motion-graph.
     * \return True if a new solution was registered.
     */
    template <typename Vertex, typename Graph>
    typename boost::disable_if< boost::is_convertible< typename Graph::vertex_bundled*, optimal_mg_vertex< FreeSpaceType >* >,
    solution_record_ptr >::type register_joining_point(Vertex start_node, Vertex goal_node, 
                                        Vertex join1_node, Vertex join2_node, 
                                        double joining_distance, 
                                        Graph& g1, Graph& g2) {
      typedef any_motion_graph<FreeSpaceType, Graph> TEGraph;
      typedef typename boost::graph_traits<TEGraph>::vertex_descriptor TEVertex;
      
      TEGraph te_g1( &g1 );
      TEGraph te_g2( &g2 );
      TEVertex te_start = TEVertex( boost::any( start_node) );
      TEVertex te_goal  = TEVertex( boost::any( goal_node ) );
      TEVertex te_join1 = TEVertex( boost::any( join1_node) );
      TEVertex te_join2 = TEVertex( boost::any( join2_node) );
      
      return register_joining_point_from_basic_mg(te_start, te_goal, te_join1, te_join2, joining_distance, te_g1, te_g2);
    };
    
    
    
    /**
     * Parametrized constructor.
     * \param aName The name for this object.
     * \param aWorld A topology which represents the C-free (obstacle-free configuration space).
     */
    planning_query(const std::string& aName, const shared_ptr< space_type >& aWorld) : named_object(), space(aWorld) { 
      setName(aName);
    };
    
    virtual ~planning_query() { };
    
    
/*******************************************************************************
                   ReaK's RTTI and Serialization interfaces
*******************************************************************************/

    virtual void RK_CALL save(serialization::oarchive& A, unsigned int) const {
      named_object::save(A,named_object::getStaticObjectType()->TypeVersion());
      A & RK_SERIAL_SAVE_WITH_NAME(space);
    };

    virtual void RK_CALL load(serialization::iarchive& A, unsigned int) {
      named_object::load(A,named_object::getStaticObjectType()->TypeVersion());
      A & RK_SERIAL_LOAD_WITH_NAME(space);
    };

    RK_RTTI_MAKE_ABSTRACT_1BASE(self,0xC2460001,1,"planning_query",named_object)
};









namespace detail {
  
  template <typename FreeSpaceType>
  typename boost::enable_if< is_steerable_space< FreeSpaceType >,
  shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >::type 
    register_basic_solution_path_impl(FreeSpaceType& space, 
                                      const graph::any_graph& g, 
                                      graph::any_graph::vertex_descriptor start_node, 
                                      graph::any_graph::vertex_descriptor goal_node,
                                      const typename topology_traits< FreeSpaceType >::point_type& goal_pos,
                                      double goal_distance,
                                      std::map<double, shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >& solutions) {
    typedef typename subspace_traits<FreeSpaceType>::super_space_type super_space_type;
    typedef shared_ptr< seq_path_base< super_space_type > > solution_record_ptr;
    typedef typename topology_traits< super_space_type >::point_type point_type;
    typedef graph::any_graph::vertex_descriptor Vertex;
    typedef graph::any_graph::edge_descriptor Edge;
    typedef typename steerable_space_traits< super_space_type >::steer_record_type SteerRecordType;
    typedef typename SteerRecordType::point_fraction_iterator SteerIter;
    
    shared_ptr< super_space_type > sup_space_ptr(&(space.get_super_space()),null_deleter());
    
    graph::any_graph::property_map_by_ptr< const point_type >      position     = graph::get_dyn_prop<const point_type&>("vertex_position", g);
    graph::any_graph::property_map_by_ptr< const SteerRecordType > steer_record = graph::get_dyn_prop<const SteerRecordType&>("edge_steer_record", g);
    
    double solutions_total_dist = goal_distance;
    
    shared_ptr< seq_path_wrapper< discrete_point_path<super_space_type> > > new_sol(new seq_path_wrapper< discrete_point_path<super_space_type> >("planning_solution", discrete_point_path<super_space_type>(sup_space_ptr,get(distance_metric, *sup_space_ptr))));
    discrete_point_path<super_space_type>& waypoints = new_sol->get_underlying_path();
    
    if(goal_distance > 0.0) {
      std::pair< point_type, SteerRecordType > goal_steer_result = space.steer_position_toward(position[goal_node], 1.0, goal_pos);
      waypoints.push_front(goal_pos);
      for(SteerIter it = goal_steer_result.second.end_fraction_travel(); it != goal_steer_result.second.begin_fraction_travel(); it -= 1.0)
        waypoints.push_front( point_type(*it) );
    };
    
    waypoints.push_front(position[goal_node]);
    
    while((in_degree(goal_node, g)) && (!g.equal_descriptors(goal_node, start_node))) {
      Edge e = *(in_edges(goal_node, g).first);
      Vertex u = source(e, g);
      const SteerRecordType& sr = steer_record[e];
      for(SteerIter it = sr.end_fraction_travel(); it != sr.begin_fraction_travel(); it -= 1.0)
        waypoints.push_front( point_type(*it) );
      solutions_total_dist += get(distance_metric, *sup_space_ptr)(position[u], position[goal_node], *sup_space_ptr);
      goal_node = u;
      waypoints.push_front(position[goal_node]);
    };
    
    if( g.equal_descriptors(goal_node, start_node) && ((solutions.empty()) || (solutions_total_dist < solutions.begin()->first))) {
      solutions[solutions_total_dist] = new_sol;
      return new_sol;
    };
    
    return solution_record_ptr();
  };
  
  template <typename FreeSpaceType>
  typename boost::disable_if< is_steerable_space< FreeSpaceType >,
  shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >::type 
    register_basic_solution_path_impl(FreeSpaceType& space, 
                                      const graph::any_graph& g, 
                                      graph::any_graph::vertex_descriptor start_node, 
                                      graph::any_graph::vertex_descriptor goal_node,
                                      const typename topology_traits< FreeSpaceType >::point_type& goal_pos,
                                      double goal_distance,
                                      std::map<double, shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >& solutions) {
    typedef typename subspace_traits<FreeSpaceType>::super_space_type super_space_type;
    typedef shared_ptr< seq_path_base< super_space_type > > solution_record_ptr;
    typedef typename topology_traits< super_space_type >::point_type point_type;
    typedef graph::any_graph::vertex_descriptor Vertex;
    
    shared_ptr< super_space_type > sup_space_ptr(&(space.get_super_space()),null_deleter());
    
    graph::any_graph::property_map_by_ptr< const point_type > position = graph::get_dyn_prop<const point_type&>("vertex_position", g);
    
    double solutions_total_dist = goal_distance;
    
    shared_ptr< seq_path_wrapper< point_to_point_path<super_space_type> > > new_sol(new seq_path_wrapper< point_to_point_path<super_space_type> >("planning_solution", point_to_point_path<super_space_type>(sup_space_ptr,get(distance_metric, *sup_space_ptr))));
    point_to_point_path<super_space_type>& waypoints = new_sol->get_underlying_path();
    
    if(goal_distance > 0.0)
      waypoints.push_front(goal_pos);
    
    waypoints.push_front(position[goal_node]);
    
    while(in_degree(goal_node, g) && (!g.equal_descriptors(goal_node, start_node))) {
      Vertex v = source(*(in_edges(goal_node, g).first), g);
      solutions_total_dist += get(distance_metric, *sup_space_ptr)(position[v], position[goal_node], *sup_space_ptr);
      waypoints.push_front(position[v]);
      goal_node = v;
    };
    
    if( g.equal_descriptors(goal_node, start_node) && ((solutions.empty()) || (solutions_total_dist < solutions.begin()->first))) {
      solutions[solutions_total_dist] = new_sol;
      return new_sol;
    };
    
    return solution_record_ptr();
  };
  
  
  
  template <typename FreeSpaceType>
  typename boost::enable_if< is_steerable_space< FreeSpaceType >,
  shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >::type 
    register_optimal_solution_path_impl(FreeSpaceType& space, 
                                        const graph::any_graph& g, 
                                        graph::any_graph::vertex_descriptor start_node, 
                                        graph::any_graph::vertex_descriptor goal_node,
                                        const typename topology_traits< FreeSpaceType >::point_type& goal_pos,
                                        double goal_distance,
                                        std::map<double, shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >& solutions) {
    typedef typename subspace_traits<FreeSpaceType>::super_space_type super_space_type;
    typedef shared_ptr< seq_path_base< super_space_type > > solution_record_ptr;
    typedef typename topology_traits< super_space_type >::point_type point_type;
    typedef typename steerable_space_traits< super_space_type >::steer_record_type SteerRecordType;
    typedef typename SteerRecordType::point_fraction_iterator SteerIter;
    typedef graph::any_graph::in_edge_iterator InEdgeIter;
    typedef graph::any_graph::vertex_descriptor Vertex;
    
    shared_ptr< super_space_type > sup_space_ptr(&(space.get_super_space()),null_deleter());
    
    graph::any_graph::property_map_by_ptr< const point_type >      position       = graph::get_dyn_prop<const point_type&>("vertex_position", g);
    graph::any_graph::property_map_by_ptr< const std::size_t >     predecessor    = graph::get_dyn_prop<const std::size_t&>("vertex_predecessor", g);
    graph::any_graph::property_map_by_ptr< const double >          distance_accum = graph::get_dyn_prop<const double&>("vertex_distance_accum", g);
    graph::any_graph::property_map_by_ptr< const SteerRecordType > steer_record   = graph::get_dyn_prop<const SteerRecordType&>("edge_steer_record", g);
    
    double solutions_total_dist = distance_accum[goal_node] + goal_distance;
    
    if( ! (solutions_total_dist < std::numeric_limits<double>::infinity()) ||
        ( (!solutions.empty()) && (solutions_total_dist >= solutions.begin()->first) ) )
      return solution_record_ptr();
    
    shared_ptr< seq_path_wrapper< discrete_point_path<super_space_type> > > new_sol(new seq_path_wrapper< discrete_point_path<super_space_type> >("planning_solution", discrete_point_path<super_space_type>(sup_space_ptr,get(distance_metric, *sup_space_ptr))));
    discrete_point_path<super_space_type>& waypoints = new_sol->get_underlying_path();
    
    if(goal_distance > 0.0) {
      std::pair< point_type, SteerRecordType > goal_steer_result = space.steer_position_toward(position[goal_node], 1.0, goal_pos);
      waypoints.push_front(goal_pos);
      for(SteerIter it = goal_steer_result.second.end_fraction_travel(); it != goal_steer_result.second.begin_fraction_travel(); it -= 1.0)
        waypoints.push_front( point_type(*it) );
    };
    
    waypoints.push_front(position[goal_node]);
    
    while(!g.equal_descriptors(goal_node, start_node)) {
      std::pair<InEdgeIter,InEdgeIter> er = in_edges(goal_node, g);
      goal_node = Vertex( boost::any( predecessor[goal_node] ) ); 
      while( ( er.first != er.second ) && ( !g.equal_descriptors(goal_node, source(*(er.first), g)) ) )
        ++(er.first);
      if(er.first == er.second)
        break;
      const SteerRecordType& sr = steer_record[*(er.first)];
      for(SteerIter it = sr.end_fraction_travel(); it != sr.begin_fraction_travel(); it -= 1.0)
        waypoints.push_front( point_type(*it) );
      waypoints.push_front(position[goal_node]);
    };
    
    if(g.equal_descriptors(goal_node, start_node)) {
      solutions[solutions_total_dist] = new_sol;
      return new_sol;
    };
    
    return solution_record_ptr();
  };
  
  
  
  template <typename FreeSpaceType>
  typename boost::disable_if< is_steerable_space< FreeSpaceType >,
  shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >::type 
    register_optimal_solution_path_impl(FreeSpaceType& space, 
                                        const graph::any_graph& g, 
                                        graph::any_graph::vertex_descriptor start_node, 
                                        graph::any_graph::vertex_descriptor goal_node,
                                        const typename topology_traits< FreeSpaceType >::point_type& goal_pos,
                                        double goal_distance,
                                        std::map<double, shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >& solutions) {
    typedef typename subspace_traits<FreeSpaceType>::super_space_type super_space_type;
    typedef shared_ptr< seq_path_base< super_space_type > > solution_record_ptr;
    typedef typename topology_traits< super_space_type >::point_type point_type;
    typedef graph::any_graph::vertex_descriptor Vertex;
    
    shared_ptr< super_space_type > sup_space_ptr(&(space.get_super_space()),null_deleter());
    
    graph::any_graph::property_map_by_ptr< const point_type >  position       = graph::get_dyn_prop<const point_type&>("vertex_position", g);
    graph::any_graph::property_map_by_ptr< const std::size_t > predecessor    = graph::get_dyn_prop<const std::size_t&>("vertex_predecessor", g);
    graph::any_graph::property_map_by_ptr< const double >      distance_accum = graph::get_dyn_prop<const double&>("vertex_distance_accum", g);
    
    double solutions_total_dist = distance_accum[goal_node] + goal_distance;
    
    if( ! (solutions_total_dist < std::numeric_limits<double>::infinity()) ||
        ( (!solutions.empty()) && (solutions_total_dist >= solutions.begin()->first) ) )
      return solution_record_ptr();
    
    shared_ptr< seq_path_wrapper< point_to_point_path<super_space_type> > > new_sol(new seq_path_wrapper< point_to_point_path<super_space_type> >("planning_solution", point_to_point_path<super_space_type>(sup_space_ptr,get(distance_metric, *sup_space_ptr))));
    point_to_point_path<super_space_type>& waypoints = new_sol->get_underlying_path();
    
    if(goal_distance > 0.0)
      waypoints.push_front(goal_pos);
    
    waypoints.push_front(position[goal_node]);
    
    while(!g.equal_descriptors(goal_node, start_node)) {
      goal_node = Vertex( boost::any( predecessor[goal_node] ) ); 
      waypoints.push_front( position[goal_node] );
    };
    
    if(g.equal_descriptors(goal_node, start_node)) {
      solutions[solutions_total_dist] = new_sol;
      return new_sol;
    };
    
    return solution_record_ptr();
  };
  
  
  
  
  
  
  
  template <typename FreeSpaceType>
  typename boost::enable_if< is_steerable_space< FreeSpaceType >,
  shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >::type 
    register_basic_solution_path_impl(FreeSpaceType& space, 
                                      const graph::any_graph& g1, 
                                      const graph::any_graph& g2, 
                                      graph::any_graph::vertex_descriptor start_node, 
                                      graph::any_graph::vertex_descriptor goal_node, 
                                      graph::any_graph::vertex_descriptor join1_node, 
                                      graph::any_graph::vertex_descriptor join2_node,
                                      double joining_distance,
                                      std::map<double, shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >& solutions) {
    typedef typename subspace_traits<FreeSpaceType>::super_space_type super_space_type;
    typedef shared_ptr< seq_path_base< super_space_type > > solution_record_ptr;
    typedef typename topology_traits< super_space_type >::point_type point_type;
    typedef graph::any_graph::vertex_descriptor Vertex;
    typedef graph::any_graph::edge_descriptor Edge;
    typedef typename steerable_space_traits< super_space_type >::steer_record_type SteerRecordType;
    typedef typename SteerRecordType::point_fraction_iterator SteerIter;
    
    shared_ptr< super_space_type > sup_space_ptr(&(space.get_super_space()),null_deleter());
    
    graph::any_graph::property_map_by_ptr< const point_type >      position1     = graph::get_dyn_prop<const point_type&>("vertex_position", g1);
    graph::any_graph::property_map_by_ptr< const SteerRecordType > steer_record1 = graph::get_dyn_prop<const SteerRecordType&>("edge_steer_record", g1);
    graph::any_graph::property_map_by_ptr< const point_type >      position2     = graph::get_dyn_prop<const point_type&>("vertex_position", g2);
    graph::any_graph::property_map_by_ptr< const SteerRecordType > steer_record2 = graph::get_dyn_prop<const SteerRecordType&>("edge_steer_record", g2);
    
    double solutions_total_dist = joining_distance;
    
    shared_ptr< seq_path_wrapper< discrete_point_path<super_space_type> > > new_sol(new seq_path_wrapper< discrete_point_path<super_space_type> >("planning_solution", discrete_point_path<super_space_type>(sup_space_ptr,get(distance_metric, *sup_space_ptr))));
    discrete_point_path<super_space_type>& waypoints = new_sol->get_underlying_path();
    
    if(joining_distance > 0.0) {
      std::pair< point_type, SteerRecordType > join_steer_result = space.steer_position_toward(position1[join1_node], 1.0, position2[join2_node]);
      for(SteerIter it = join_steer_result.second.end_fraction_travel(); it != join_steer_result.second.begin_fraction_travel(); it -= 1.0)
        waypoints.push_front( point_type(*it) );
    };
    
    waypoints.push_front(position1[join1_node]);
    
    while((in_degree(join1_node, g1)) && (!g1.equal_descriptors(join1_node, start_node))) {
      Edge e = *(in_edges(join1_node, g1).first);
      Vertex u = source(e, g1);
      const SteerRecordType& sr = steer_record1[e];
      for(SteerIter it = sr.end_fraction_travel(); it != sr.begin_fraction_travel(); it -= 1.0)
        waypoints.push_front( point_type(*it) );
      solutions_total_dist += get(distance_metric, *sup_space_ptr)(position1[u], position1[join1_node], *sup_space_ptr);
      join1_node = u;
      waypoints.push_front(position1[join1_node]);
    };
    
    waypoints.push_back(position2[join2_node]);
    
    while((in_degree(join2_node, g2)) && (!g2.equal_descriptors(join2_node, goal_node))) {
      Edge e = *(in_edges(join2_node, g2).first);
      Vertex u = source(e, g2);
      const SteerRecordType& sr = steer_record2[e];
      for(SteerIter it = sr.end_fraction_travel(); it != sr.begin_fraction_travel(); it -= 1.0)
        waypoints.push_back( point_type(*it) );
      solutions_total_dist += get(distance_metric, *sup_space_ptr)(position2[u], position2[join2_node], *sup_space_ptr);
      join2_node = u;
      waypoints.push_back(position2[join2_node]);
    };
    
    
    if( g1.equal_descriptors(join1_node, start_node) && 
        g2.equal_descriptors(join2_node, goal_node) && 
        ( (solutions.empty()) || (solutions_total_dist < solutions.begin()->first) ) ) {
      solutions[solutions_total_dist] = new_sol;
      return new_sol;
    };
    
    return solution_record_ptr();
  };
  
  template <typename FreeSpaceType>
  typename boost::disable_if< is_steerable_space< FreeSpaceType >,
  shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >::type 
    register_basic_solution_path_impl(FreeSpaceType& space, 
                                      const graph::any_graph& g1, 
                                      const graph::any_graph& g2, 
                                      graph::any_graph::vertex_descriptor start_node, 
                                      graph::any_graph::vertex_descriptor goal_node, 
                                      graph::any_graph::vertex_descriptor join1_node, 
                                      graph::any_graph::vertex_descriptor join2_node,
                                      double joining_distance,
                                      std::map<double, shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >& solutions) {
    typedef typename subspace_traits<FreeSpaceType>::super_space_type super_space_type;
    typedef shared_ptr< seq_path_base< super_space_type > > solution_record_ptr;
    typedef typename topology_traits< super_space_type >::point_type point_type;
    typedef graph::any_graph::vertex_descriptor Vertex;
    
    shared_ptr< super_space_type > sup_space_ptr(&(space.get_super_space()),null_deleter());
    
    graph::any_graph::property_map_by_ptr< const point_type > position1 = graph::get_dyn_prop<const point_type&>("vertex_position", g1);
    graph::any_graph::property_map_by_ptr< const point_type > position2 = graph::get_dyn_prop<const point_type&>("vertex_position", g2);
    
    double solutions_total_dist = joining_distance;
    
    shared_ptr< seq_path_wrapper< point_to_point_path<super_space_type> > > new_sol(new seq_path_wrapper< point_to_point_path<super_space_type> >("planning_solution", point_to_point_path<super_space_type>(sup_space_ptr,get(distance_metric, *sup_space_ptr))));
    point_to_point_path<super_space_type>& waypoints = new_sol->get_underlying_path();
    
    waypoints.push_front(position1[join1_node]);
    
    while(in_degree(join1_node, g1) && (!g1.equal_descriptors(join1_node, start_node))) {
      Vertex v = source(*(in_edges(join1_node, g1).first), g1);
      solutions_total_dist += get(distance_metric, *sup_space_ptr)(position1[v], position1[join1_node], *sup_space_ptr);
      waypoints.push_front(position1[v]);
      join1_node = v;
    };
    
    waypoints.push_back(position2[join2_node]);
    
    while(in_degree(join2_node, g2) && (!g2.equal_descriptors(join2_node, goal_node))) {
      Vertex v = source(*(in_edges(join2_node, g2).first), g2);
      solutions_total_dist += get(distance_metric, *sup_space_ptr)(position2[join2_node], position2[v], *sup_space_ptr);
      waypoints.push_back(position2[v]);
      join2_node = v;
    };
    
    if( g1.equal_descriptors(join1_node, start_node) && 
        g2.equal_descriptors(join2_node, goal_node) && 
        ( (solutions.empty()) || (solutions_total_dist < solutions.begin()->first) ) ) {
      solutions[solutions_total_dist] = new_sol;
      return new_sol;
    };
    
    return solution_record_ptr();
  };
  
  
  
  template <typename FreeSpaceType>
  typename boost::enable_if< is_steerable_space< FreeSpaceType >,
  shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >::type 
    register_optimal_solution_path_impl(FreeSpaceType& space, 
                                        const graph::any_graph& g1, 
                                        const graph::any_graph& g2, 
                                        graph::any_graph::vertex_descriptor start_node, 
                                        graph::any_graph::vertex_descriptor goal_node, 
                                        graph::any_graph::vertex_descriptor join1_node, 
                                        graph::any_graph::vertex_descriptor join2_node,
                                        double joining_distance,
                                        std::map<double, shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >& solutions) {
    typedef typename subspace_traits<FreeSpaceType>::super_space_type super_space_type;
    typedef shared_ptr< seq_path_base< super_space_type > > solution_record_ptr;
    typedef typename topology_traits< super_space_type >::point_type point_type;
    typedef typename steerable_space_traits< super_space_type >::steer_record_type SteerRecordType;
    typedef typename SteerRecordType::point_fraction_iterator SteerIter;
    typedef graph::any_graph::in_edge_iterator InEdgeIter;
    typedef graph::any_graph::vertex_descriptor Vertex;
    
    shared_ptr< super_space_type > sup_space_ptr(&(space.get_super_space()),null_deleter());
    
    graph::any_graph::property_map_by_ptr< const point_type >      position1       = graph::get_dyn_prop<const point_type&>("vertex_position", g1);
    graph::any_graph::property_map_by_ptr< const std::size_t >     predecessor1    = graph::get_dyn_prop<const std::size_t&>("vertex_predecessor", g1);
    graph::any_graph::property_map_by_ptr< const double >          distance_accum1 = graph::get_dyn_prop<const double&>("vertex_distance_accum", g1);
    graph::any_graph::property_map_by_ptr< const SteerRecordType > steer_record1   = graph::get_dyn_prop<const SteerRecordType&>("edge_steer_record", g1);
    graph::any_graph::property_map_by_ptr< const point_type >      position2       = graph::get_dyn_prop<const point_type&>("vertex_position", g2);
    graph::any_graph::property_map_by_ptr< const std::size_t >     predecessor2    = graph::get_dyn_prop<const std::size_t&>("vertex_predecessor", g2);
    graph::any_graph::property_map_by_ptr< const double >          distance_accum2 = graph::get_dyn_prop<const double&>("vertex_distance_accum", g2);
    graph::any_graph::property_map_by_ptr< const SteerRecordType > steer_record2   = graph::get_dyn_prop<const SteerRecordType&>("edge_steer_record", g2);
    
    double solutions_total_dist = distance_accum1[join1_node] + distance_accum2[join2_node] + joining_distance;
    
    if( ! (solutions_total_dist < std::numeric_limits<double>::infinity()) ||
        ( (!solutions.empty()) && (solutions_total_dist >= solutions.begin()->first) ) )
      return solution_record_ptr();
    
    shared_ptr< seq_path_wrapper< discrete_point_path<super_space_type> > > new_sol(new seq_path_wrapper< discrete_point_path<super_space_type> >("planning_solution", discrete_point_path<super_space_type>(sup_space_ptr,get(distance_metric, *sup_space_ptr))));
    discrete_point_path<super_space_type>& waypoints = new_sol->get_underlying_path();
    
    if(joining_distance > 0.0) {
      std::pair< point_type, SteerRecordType > join_steer_result = space.steer_position_toward(position1[join1_node], 1.0, position2[join2_node]);
      for(SteerIter it = join_steer_result.second.end_fraction_travel(); it != join_steer_result.second.begin_fraction_travel(); it -= 1.0)
        waypoints.push_front( point_type(*it) );
    };
    
    waypoints.push_front(position1[join1_node]);
    
    while(!g1.equal_descriptors(join1_node, start_node)) {
      std::pair<InEdgeIter,InEdgeIter> er = in_edges(join1_node, g1);
      join1_node = Vertex( boost::any( predecessor1[join1_node] ) ); 
      while( ( er.first != er.second ) && ( !g1.equal_descriptors(join1_node, source(*(er.first), g1)) ) )
        ++(er.first);
      if(er.first == er.second)
        break;
      const SteerRecordType& sr = steer_record1[*(er.first)];
      for(SteerIter it = sr.end_fraction_travel(); it != sr.begin_fraction_travel(); it -= 1.0)
        waypoints.push_front( point_type(*it) );
      waypoints.push_front(position1[join1_node]);
    };
    
    waypoints.push_back(position2[join2_node]);
    
    while(!g2.equal_descriptors(join2_node, goal_node)) {
      std::pair<InEdgeIter,InEdgeIter> er = in_edges(join2_node, g2);
      join2_node = Vertex( boost::any( predecessor2[join2_node] ) ); 
      while( ( er.first != er.second ) && ( !g2.equal_descriptors(join2_node, source(*(er.first), g2)) ) )
        ++(er.first);
      if(er.first == er.second)
        break;
      const SteerRecordType& sr = steer_record2[*(er.first)];
      for(SteerIter it = sr.end_fraction_travel(); it != sr.begin_fraction_travel(); it -= 1.0)
        waypoints.push_back( point_type(*it) );
      waypoints.push_back(position2[join2_node]);
    };
    
    
    if(g1.equal_descriptors(join1_node, start_node) && g2.equal_descriptors(join2_node, goal_node)) {
      solutions[solutions_total_dist] = new_sol;
      return new_sol;
    };
    
    return solution_record_ptr();
  };
  
  
  
  template <typename FreeSpaceType>
  typename boost::disable_if< is_steerable_space< FreeSpaceType >,
  shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >::type 
    register_optimal_solution_path_impl(FreeSpaceType& space, 
                                        const graph::any_graph& g1, 
                                        const graph::any_graph& g2, 
                                        graph::any_graph::vertex_descriptor start_node, 
                                        graph::any_graph::vertex_descriptor goal_node, 
                                        graph::any_graph::vertex_descriptor join1_node, 
                                        graph::any_graph::vertex_descriptor join2_node,
                                        double joining_distance,
                                        std::map<double, shared_ptr< seq_path_base< typename subspace_traits<FreeSpaceType>::super_space_type > > >& solutions) {
    typedef typename subspace_traits<FreeSpaceType>::super_space_type super_space_type;
    typedef shared_ptr< seq_path_base< super_space_type > > solution_record_ptr;
    typedef typename topology_traits< super_space_type >::point_type point_type;
    typedef graph::any_graph::vertex_descriptor Vertex;
    
    shared_ptr< super_space_type > sup_space_ptr(&(space.get_super_space()),null_deleter());
    
    graph::any_graph::property_map_by_ptr< const point_type >  position1       = graph::get_dyn_prop<const point_type&>("vertex_position", g1);
    graph::any_graph::property_map_by_ptr< const std::size_t > predecessor1    = graph::get_dyn_prop<const std::size_t&>("vertex_predecessor", g1);
    graph::any_graph::property_map_by_ptr< const double >      distance_accum1 = graph::get_dyn_prop<const double&>("vertex_distance_accum", g1);
    graph::any_graph::property_map_by_ptr< const point_type >  position2       = graph::get_dyn_prop<const point_type&>("vertex_position", g2);
    graph::any_graph::property_map_by_ptr< const std::size_t > predecessor2    = graph::get_dyn_prop<const std::size_t&>("vertex_predecessor", g2);
    graph::any_graph::property_map_by_ptr< const double >      distance_accum2 = graph::get_dyn_prop<const double&>("vertex_distance_accum", g2);
    
    double solutions_total_dist = distance_accum1[join1_node] + distance_accum2[join2_node] + joining_distance;
    
    if( ! (solutions_total_dist < std::numeric_limits<double>::infinity()) ||
        ( (!solutions.empty()) && (solutions_total_dist >= solutions.begin()->first) ) )
      return solution_record_ptr();
    
    shared_ptr< seq_path_wrapper< point_to_point_path<super_space_type> > > new_sol(new seq_path_wrapper< point_to_point_path<super_space_type> >("planning_solution", point_to_point_path<super_space_type>(sup_space_ptr,get(distance_metric, *sup_space_ptr))));
    point_to_point_path<super_space_type>& waypoints = new_sol->get_underlying_path();
    
    waypoints.push_front(position1[join1_node]);
    
    while(!g1.equal_descriptors(join1_node, start_node)) {
      join1_node = Vertex( boost::any( predecessor1[join1_node] ) ); 
      waypoints.push_front( position1[join1_node] );
    };
    
    waypoints.push_back(position2[join2_node]);
    
    while(!g2.equal_descriptors(join2_node, goal_node)) {
      join2_node = Vertex( boost::any( predecessor2[join2_node] ) ); 
      waypoints.push_back( position2[join2_node] );
    };
    
    if(g1.equal_descriptors(join1_node, start_node) && g2.equal_descriptors(join2_node, goal_node)) {
      solutions[solutions_total_dist] = new_sol;
      return new_sol;
    };
    
    return solution_record_ptr();
  };
  
  
  
};




/**
 * This class is the basic OOP interface for a path planner. 
 * OOP-style planners are useful to hide away
 * the cumbersome details of calling the underlying planning algorithms which are 
 * generic programming (GP) style and thus provide a lot more flexibility but are difficult
 * to deal with in the user-space. The OOP planners are meant to offer a much simpler interface,
 * i.e., a member function that "solves the problem" and returns the solution path or trajectory.
 */
template <typename FreeSpaceType>
class path_planning_p2p_query : public planning_query<FreeSpaceType> {
  public:
    typedef path_planning_p2p_query<FreeSpaceType> self;
    typedef planning_query<FreeSpaceType> base_type;
    typedef typename base_type::space_type space_type;
    typedef typename base_type::super_space_type super_space_type;
    
    typedef typename base_type::point_type point_type;
    typedef typename base_type::point_difference_type point_difference_type;
    
    typedef typename base_type::solution_record_ptr solution_record_ptr;
    
  public:
    
    point_type start_pos;
    point_type goal_pos;
    std::size_t max_num_results;
    
    std::map<double, solution_record_ptr > solutions;
    
    
    /**
     * Returns the best solution distance registered in this query object.
     * \return The best solution distance registered in this query object.
     */
    virtual double get_best_solution_distance() const {
      if(solutions.size() == 0)
        return std::numeric_limits<double>::infinity();
      else
        return solutions.begin()->first;
    };
    
    /**
     * Returns true if the solver should keep on going trying to solve the path-planning problem.
     * \return True if the solver should keep on going trying to solve the path-planning problem.
     */
    virtual bool keep_going() const {
      return (max_num_results > solutions.size());
    };
    
    /**
     * This function is called to reset the internal state of the planner.
     */
    virtual void reset_solution_records() {
      solutions.clear();
    };
    
    virtual const point_type& get_start_position() const { return start_pos; };
    
    virtual double get_distance_to_goal(const point_type& pos) { 
      return get(distance_metric, *(this->space))(pos, goal_pos, *(this->space));
    };
    
    virtual double get_heuristic_to_goal(const point_type& pos) { 
      return get(distance_metric, this->space->get_super_space())(pos, goal_pos, this->space->get_super_space());
    };
    
  protected:
    
    virtual solution_record_ptr 
      register_solution_from_optimal_mg(graph::any_graph::vertex_descriptor start_node, 
                                        graph::any_graph::vertex_descriptor goal_node, 
                                        double goal_distance,
                                        graph::any_graph& g) {
      return detail::register_optimal_solution_path_impl(*(this->space), g, start_node, goal_node, goal_pos, goal_distance, solutions);
    };
    
    virtual solution_record_ptr 
      register_solution_from_basic_mg(graph::any_graph::vertex_descriptor start_node, 
                                      graph::any_graph::vertex_descriptor goal_node, 
                                      double goal_distance,
                                      graph::any_graph& g) {
      return detail::register_basic_solution_path_impl(*(this->space), g, start_node, goal_node, goal_pos, goal_distance, solutions);
    };
    
    virtual solution_record_ptr 
      register_joining_point_from_optimal_mg(graph::any_graph::vertex_descriptor start_node, 
                                             graph::any_graph::vertex_descriptor goal_node, 
                                             graph::any_graph::vertex_descriptor join1_node, 
                                             graph::any_graph::vertex_descriptor join2_node, 
                                             double joining_distance,
                                             graph::any_graph& g1, 
                                             graph::any_graph& g2) {
      return detail::register_optimal_solution_path_impl(*(this->space), g1, g2, start_node, goal_node, join1_node, join2_node, joining_distance, solutions);
    };
    
    virtual solution_record_ptr 
      register_joining_point_from_basic_mg(graph::any_graph::vertex_descriptor start_node, 
                                           graph::any_graph::vertex_descriptor goal_node, 
                                           graph::any_graph::vertex_descriptor join1_node, 
                                           graph::any_graph::vertex_descriptor join2_node, 
                                           double joining_distance,
                                           graph::any_graph& g1, 
                                           graph::any_graph& g2) {
      return detail::register_basic_solution_path_impl(*(this->space), g1, g2, start_node, goal_node, join1_node, join2_node, joining_distance, solutions);
    };
    
    
  public:
    
    
    /**
     * Parametrized constructor.
     * \param aName The name for this object.
     * \param aWorld A topology which represents the C-free (obstacle-free configuration space).
     */
    path_planning_p2p_query(const std::string& aName,
                            const shared_ptr< space_type >& aWorld,
                            const point_type& aStartPos,
                            const point_type& aGoalPos,
                            std::size_t aMaxNumResults = 1) :
                            base_type(aName, aWorld), start_pos(aStartPos), goal_pos(aGoalPos), max_num_results(aMaxNumResults) { };
    
    virtual ~path_planning_p2p_query() { };
    
    
/*******************************************************************************
                   ReaK's RTTI and Serialization interfaces
*******************************************************************************/

    virtual void RK_CALL save(serialization::oarchive& A, unsigned int) const {
      base_type::save(A,base_type::getStaticObjectType()->TypeVersion());
      A & RK_SERIAL_SAVE_WITH_NAME(start_pos)
        & RK_SERIAL_SAVE_WITH_NAME(goal_pos)
        & RK_SERIAL_SAVE_WITH_NAME(max_num_results);
    };

    virtual void RK_CALL load(serialization::iarchive& A, unsigned int) {
      base_type::load(A,base_type::getStaticObjectType()->TypeVersion());
      A & RK_SERIAL_LOAD_WITH_NAME(start_pos)
        & RK_SERIAL_LOAD_WITH_NAME(goal_pos)
        & RK_SERIAL_LOAD_WITH_NAME(max_num_results);
      solutions.clear();
    };

    RK_RTTI_MAKE_ABSTRACT_1BASE(self,0xC2460001,1,"path_planning_p2p_query",base_type)
};


};

};

#endif

