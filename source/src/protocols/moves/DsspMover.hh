// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file ./protocols/moves/DsspMover.hh
/// @brief  header file of DsspMover.cc
/// @author Nobuyasu Koga ( nobuyasau@uw.edu )

#ifndef INCLUDED_protocols_moves_DsspMover_hh
#define INCLUDED_protocols_moves_DsspMover_hh

// Project headers
#include <core/pose/Pose.fwd.hh>
#include <protocols/moves/Mover.hh>

#include <basic/datacache/DataMap.fwd.hh>
#include <protocols/moves/Mover.fwd.hh>
#include <utility/tag/Tag.fwd.hh>


#ifdef WIN32
#include <utility/tag/Tag.hh>
#endif


namespace protocols {
namespace moves {


class DsspMover : public Mover {
public:

	typedef protocols::moves::MoverOP MoverOP;
	typedef utility::tag::TagCOP TagCOP;
	typedef basic::datacache::DataMap DataMap;

public:

	// default constructor
	DsspMover();

	// @brief destructor
	~DsspMover() override;

	/// @brief clone this object
	MoverOP clone() const override;

	/// @brief create this type of object
	MoverOP fresh_instance() const override;

	// @brief virtual main operation
	void apply( core::pose::Pose & pose ) override;

	void parse_my_tag( TagCOP, basic::datacache::DataMap & ) override;

	std::string
	get_name() const override;

	static
	std::string
	mover_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

	/// @brief Select only the asymmetric unit residues or not.
	void set_asu_only( bool const asu_only );

private:
	bool reduced_IG_as_L_;
	bool asu_only_;

};


} // moves
} // protocols


#endif
