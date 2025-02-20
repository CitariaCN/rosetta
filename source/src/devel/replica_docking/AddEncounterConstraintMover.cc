// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @brief  very loose AtomPair cst from the geometry center of docking partner1 to geometry center of docking
/// partner2, using in replica docking to avoid the two docking partners never meet each other after some move

#include <devel/replica_docking/AddEncounterConstraintMover.hh>
#include <devel/replica_docking/AddEncounterConstraintMoverCreator.hh>

#include <protocols/scoring/InterfaceInfo.hh>

#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/pose/datacache/CacheableDataType.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/id/AtomID.hh>
#include <core/scoring/constraints/ConstraintIO.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/constraints/BoundConstraint.hh>
#include <core/scoring/constraints/AtomPairConstraint.hh>
#include <core/scoring/func/Func.fwd.hh>
#include <core/chemical/ResidueType.hh>

#include <utility/tag/Tag.hh>


#include <numeric/xyzVector.hh>


#include <basic/datacache/BasicDataCache.hh>
#include <basic/Tracer.hh>
// XSD XRW Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/moves/mover_schemas.hh>

#include <protocols/scoring/Interface.hh> // AUTO IWYU For Interface

using namespace utility::tag;

namespace devel {
namespace replica_docking {

using namespace core;
using namespace protocols;
using namespace core::scoring;
using namespace protocols::scoring;

static basic::Tracer tr( "devel.replica_docking.AddEncounterConstraintMover" );




///////////////////////////////////////////////////////////////////////

AddEncounterConstraintMover::AddEncounterConstraintMover()
: moves::Mover( AddEncounterConstraintMover::mover_name() )
{
	cst_ = nullptr;
}

moves::MoverOP
AddEncounterConstraintMover::clone() const {
	return utility::pointer::make_shared< AddEncounterConstraintMover >( *this );
}

AddEncounterConstraintMover::~AddEncounterConstraintMover() = default;

void
AddEncounterConstraintMover::apply(pose::Pose& pose){
	//  core::scoring::constraints::AtomPairConstraintCOP cst_;
	if ( !cst_ ) {
		tr.Debug << "Encounter constraint is not initialized yet" << std::endl;
		//  tr.Debug << "what is in cst_:" << cst_ << std::endl;
		cst_ = generate_encounter_cst( pose );
		constraints::ConstraintSet cst_set;
		cst_set.add_constraint( cst_ );
		constraints::ConstraintIO::write_constraints( tr.Info, cst_set , pose );
		tr.Info << std::endl;
		//   constraints::ConstraintIO::write_constraints( "Encounter.cst", cst_set, pose );
	}
	pose.add_constraint( cst_ );
	tr.Debug << "Encounter constraints have been applied" << std::endl;
}


void
AddEncounterConstraintMover::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap &
){
	gap_ = tag->getOption< Real > ( "gap", 10.0 );
}


core::scoring::constraints::AtomPairConstraintCOP
AddEncounterConstraintMover::generate_encounter_cst( pose::Pose & pose) {

	protocols::scoring::InterfaceInfo const & interface_info( interface_from_pose( pose ) );
	tr.Debug << "interface_info.num_jump(): "<< interface_info.num_jump() << std::endl;
	runtime_assert( interface_info.num_jump() ==1 ); //currently only 1 interface supported... can be changed later...
	interface_jump_ = interface_info.interface(1)->jump_id();
	tr.Debug << "interface_jump id: " << interface_jump_ << std::endl;
	ObjexxFCL::FArray1D_bool subunit_separation( pose.size(), false );
	kinematics::FoldTree const& f(pose.fold_tree());
	f.partition_by_jump( interface_jump_, subunit_separation );
	// i is subunit 1 if domain_separation( i ) == true
	// i is subunit 2 if domain_separation( j ) == false
	core::Size pos1_end = 0;
	for ( core::Size i =1, i_end = pose.size() ; i <= i_end; i++ ) {
		if ( subunit_separation(i) ) {
			pos1_end = i;
		}
	}  // to get the last residue of partner1
	tr.Debug << " pos1_end got using partition_by_jump: " << pos1_end << std::endl;
	tr.Debug << " cutpoint_by_jump: " << f.cutpoint_by_jump( interface_jump_ ) << std::endl;

	core::Size center_pos1 (core::pose::residue_center_of_mass( pose, 1, pos1_end ) );  //mass center of partner 1
	core::Size center_pos2 (core::pose::residue_center_of_mass( pose, pos1_end + 1, pose.size() ) ); // mass center of partner 2
	tr.Debug << "center_pos1: " << center_pos1 << "; center_pos2: " << center_pos2 << std::endl;

	Real dist_pos1 = 0.0;
	Real dist_pos2 = 0.0;
	Real tmp = 0.0;
	id::AtomID atom_pos1( pose.residue_type( center_pos1 ).atom_index( "CA" ), center_pos1 );
	id::AtomID atom_pos2( pose.residue_type( center_pos2 ).atom_index( "CA" ), center_pos2 );
	numeric::xyzVector<double> coords_center_pos1 = pose.xyz( atom_pos1 );
	numeric::xyzVector<double> coords_center_pos2 = pose.xyz( atom_pos2 );
	for ( core::Size i = 1; i<= pos1_end; i++ ) {
		tmp = coords_center_pos1.distance_squared( pose.xyz( id::AtomID( pose.residue_type( i ).atom_index( "CA" ), i) ) );
		if ( dist_pos1 < tmp ) { dist_pos1 = tmp; }
	}

	for ( core::Size i = pos1_end + 1; i<= pose.size(); i++ ) {
		tmp = coords_center_pos2.distance_squared( pose.xyz( id::AtomID( pose.residue_type( i ).atom_index( "CA" ), i) ) );
		if ( dist_pos2 < tmp ) { dist_pos2 = tmp; }
	}
	tr.Debug << "dist_pos1: " << std::sqrt(dist_pos1) << "; dist_pos2: " << std::sqrt(dist_pos2) << std::endl;
	func::FuncOP bound_func( new constraints::BoundFunc(0.1, std::sqrt(dist_pos1) + std::sqrt(dist_pos2) + gap_, 0.5, 10000, "Encounter") );
	constraints::AtomPairConstraintCOP cst( utility::pointer::make_shared< constraints::AtomPairConstraint >( atom_pos1, atom_pos2, bound_func ) );

	return cst;
}


InterfaceInfo const &
AddEncounterConstraintMover::interface_from_pose( pose::Pose const & pose ) const
{
	InterfaceInfoCOP ptr = utility::pointer::static_pointer_cast< InterfaceInfo const >(
		pose.data().get_const_ptr( pose::datacache::CacheableDataType::INTERFACE_INFO )
	);
	if ( !ptr ) {
		utility_exit_with_message( "cannot find interface information !!!\nMake sure that docking::setup_foldtree() "
			"has been called, for instance by using the DockSetupMover before the AddEncounterConstraintMover" );
	}
	tr.Debug << "extracted InterfaceInfo from PoseCache with address " << ptr << std::endl;
	return *ptr;
}

std::string AddEncounterConstraintMover::get_name() const {
	return mover_name();
}

std::string AddEncounterConstraintMover::mover_name() {
	return "AddEncounterConstraintMover";
}

void AddEncounterConstraintMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	AttributeList attlist;
	attlist + XMLSchemaAttribute::attribute_w_default(
		"gap", xsct_real,
		"XSD_XRW: TO DO",
		"10.0");

	protocols::moves::xsd_type_definition_w_attributes(
		xsd, mover_name(),
		"XSD_XRW: TO DO",
		attlist );
}

std::string AddEncounterConstraintMoverCreator::keyname() const {
	return AddEncounterConstraintMover::mover_name();
}

protocols::moves::MoverOP
AddEncounterConstraintMoverCreator::create_mover() const {
	return utility::pointer::make_shared< AddEncounterConstraintMover >();
}

void AddEncounterConstraintMoverCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	AddEncounterConstraintMover::provide_xml_schema( xsd );
}


}//replica_docking
}//devel
