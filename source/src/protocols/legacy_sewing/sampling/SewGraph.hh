// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file SewGraph.hh
///
/// @brief
/// @author Tim Jacobs

#ifndef INCLUDED_protocols_legacy_sewing_sampling_SewGraph_hh
#define INCLUDED_protocols_legacy_sewing_sampling_SewGraph_hh

//Unit headers
#include <protocols/legacy_sewing/sampling/SewGraph.fwd.hh>

//Package headers
#include <protocols/legacy_sewing/hashing/Hasher.hh>
#include <protocols/legacy_sewing/conformation/Model.hh>

#include <utility/graph/Graph.hh>

//Protocol headers

//Utility headers

//C++ headers
#include <set>

//External headers

namespace protocols {
namespace legacy_sewing  {

//////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////   Node Class   ///////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

class ModelNode : public utility::graph::Node {

public:

	ModelNode( utility::graph::Graph * owner, core::Size index );

	~ModelNode() override {}

	Model const &
	model() const;

	void
	model(
		Model const & model
	);

	void
	copy_from(
		utility::graph::Node const * source
	) override;

	std::set<core::Size> const &
	segment_ids() const;

	void
	segment_ids(
		std::set<core::Size> const & segment_ids
	);

	friend
	std::ostream &
	operator<< ( std::ostream & out, ModelNode const & atom);

private:
	Model model_;
	std::set<core::Size> segment_ids_;
};

//////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////   Edge Class   ///////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

class HashEdge : public utility::graph::Edge {

public:

	HashEdge(
		SewGraph * owner,
		core::Size n1,
		core::Size n2
	);

	void
	basis_pair( BasisPair basis_pair ){ basis_pair_ = basis_pair; }

	BasisPair const &
	basis_pair() const { return basis_pair_; }

	core::Size
	model_resnum(
		int model_id
	) const;

	friend
	std::ostream &
	operator<< ( std::ostream & out, HashEdge const & edge);

private:

	BasisPair basis_pair_;

};


//////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////   SewGraph Class   ///////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

class SewGraph : public utility::graph::Graph {

public:

	~SewGraph() override;
	SewGraph();
	SewGraph( SewGraph const & src );

	///@brief default construct
	SewGraph(
		std::map< int, Model > const & models,
		core::Size segment_matches_per_edge
	);

	/// @brief Factory method for node creation
	utility::graph::Node*
	create_new_node( core::Size index ) override;

	/// @brief Factory method for edge creation
	utility::graph::Edge*
	create_new_edge( core::Size index1, core::Size index2 ) override;

	void
	delete_edge( utility::graph::Edge * edge ) override;

	HashEdge *
	find_hash_edge(core::Size n1, core::Size n2);

	HashEdge const *
	find_hash_edge(core::Size n1, core::Size n2) const;

	std::set<core::Size>
	get_node_indices_from_model_id(int model_id) const;

	ModelNode const *
	get_model_node(core::Size n) const;

	ModelNode const *
	get_model_node(
		int model_id,
		std::set<core::Size> segment_ids
	) const;

	///@brief get a random node from the graph
	ModelNode const *
	get_random_node() const;

	///@brief get a random node involved in
	///at least one edge from the graph
	ModelNode const *
	get_random_node_with_edges() const;

	void
	set_special_edges(
		ScoreResults const & scores
	);

	void
	add_special_edges();

	void
	generate_binary_score_file(
		std::string score_filename,
		std::string binary_filename
	);

	void
	add_all_model_edges_from_binary(
		std::string filename,
		int model_id
	);

	void
	add_edges_from_binary(
		std::string filename,
		core::Size node_id
	);

	void
	report_binary_stats(
		std::map< int, Model > const & models,
		std::string filename
	);

private:

	boost::unordered_object_pool< HashEdge > * hash_edge_pool_;

	std::map< int, std::set<core::Size> > model_indices_;
	core::Size last_node_added_;

	ScoreResults special_edge_data_;

};


} //legacy_sewing namespace
} //protocols namespace

#endif
