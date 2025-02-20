// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/features/InterfaceDdGMover.cc
/// @brief Calculates ddG of binding
/// @author Kyle Barlow (kb@kylebarlow.com)


#include <protocols/features/InterfaceDdGMover.hh>
#include <protocols/features/InterfaceDdGMoverCreator.hh>

#include <protocols/rigid/RigidBodyMover.hh>
#include <protocols/rosetta_scripts/util.hh>
#include <protocols/rosetta_scripts/SavePoseMover.hh>

#include <protocols/features/ReportToDB.hh>

#include <basic/Tracer.hh>
#include <utility/tag/Tag.hh>

#include <core/conformation/symmetry/SymmetricConformation.hh>
#include <core/conformation/symmetry/SymmetryInfo.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/pose/Pose.hh>
#include <core/pose/chains_util.hh>
#include <core/pose/symmetry/util.hh>

#include <boost/format.hpp>

// XSD XRW Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/moves/mover_schemas.hh>

static basic::Tracer TR( "protocols.features.InterfaceDdGMover" );

namespace protocols {
namespace features {

InterfaceDdGMover::InterfaceDdGMover():
	protocols::moves::Mover( "InterfaceDdGMover" ),
	scorefxn_(nullptr),
	translate_by_(1000),
	unbound_wt_pose_(nullptr),
	bound_wt_pose_(nullptr),
	unbound_mut_pose_(nullptr),
	bound_mut_pose_(nullptr),
	apply_pose_is_mutant_(true),
	wt_save_pose_mover_(nullptr),
	mut_save_pose_mover_(nullptr),
	bound_wt_db_reporter_(nullptr),
	unbound_wt_db_reporter_(nullptr),
	bound_mut_db_reporter_(nullptr),
	unbound_mut_db_reporter_(nullptr),
	report_to_db_(false)
{}

InterfaceDdGMover::~InterfaceDdGMover() = default;

void
InterfaceDdGMover::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & data
)
{

	// First, check for errors and throw exception

	if ( ( tag->hasOption("chain_num") || tag->hasOption("chain_name") ) && tag->hasOption("jump") ) {
		throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "you can specify either chains or jump in the ddG mover, but not both");
	}

	if ( tag->hasOption("chain_num") && tag->hasOption("chain_name") ) {
		throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "you can specify chains either by number or by name, but not both");
	}

	if ( tag->hasOption("mut_ref_savepose_mover") && tag->hasOption("wt_ref_savepose_mover") ) {
		throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError,  "you can only specify either the mutant reference pose (SavePoseMover) or the wildtype reference pose (SavePoseMover) in this tag. The undefined pose will be set as the pose given to this mover in the apply function." );
	}

	chain_ids_.clear(); // Useful if parse_my_tag is called multiple times in unit tests
	chain_names_.clear();
	jump_ids_.clear();

	if ( tag->hasOption("jump") ) {
		add_jump_id( tag->getOption<core::Size>("jump") );
	}

	if ( tag->hasOption("chain_num") ) {
		for ( auto & chain_num : utility::string_split( tag->getOption<std::string>("chain_num"), ',', core::Size() ) ) {
			add_chain_id( chain_num );
		}
	}

	if ( tag->hasOption("chain_name") ) {
		utility::vector1<std::string> chain_names = utility::string_split(tag->getOption<std::string>("chain_name"),',',std::string());
		for ( auto & chain_name : chain_names ) {
			add_chain_name( chain_name );
		}
	}

	// Use a default of chain 1 if no chain or jump to move has been defined yet
	if ( chain_ids_.size() == 0 && chain_names_.size() == 0 && jump_ids_.size() == 0 ) {
		TR.Warning << "No chain/jump defined to move; defaulting to first chain" << std::endl;
		add_chain_id( 1 );
	}

	set_scorefxn( protocols::rosetta_scripts::parse_score_function( tag, data, "commandline" ) );

	// Load saved reference pose(s), if defined
	if ( tag->hasOption("mut_ref_savepose_mover") ) {
		moves::MoverOP mover = protocols::rosetta_scripts::parse_mover( tag->getOption< std::string >( "mut_ref_savepose_mover" ), data );
		mut_save_pose_mover_ = utility::pointer::dynamic_pointer_cast < protocols::rosetta_scripts::SavePoseMover > (mover);
		apply_pose_is_mutant_ = false;
	}

	if ( tag->hasOption("wt_ref_savepose_mover") ) {
		moves::MoverOP mover = protocols::rosetta_scripts::parse_mover( tag->getOption< std::string >( "wt_ref_savepose_mover" ), data );
		wt_save_pose_mover_ = utility::pointer::dynamic_pointer_cast < protocols::rosetta_scripts::SavePoseMover > (mover);
		apply_pose_is_mutant_ = true;
	}

	// Set ReportToDB mover
	if ( tag->hasOption("db_reporter") ) {
		set_db_reporter( utility::pointer::dynamic_pointer_cast < protocols::features::ReportToDB > ( protocols::rosetta_scripts::parse_mover( tag->getOption< std::string >( "db_reporter" ), data ) ) );
	}

}

void
InterfaceDdGMover::set_db_reporter(
	protocols::features::ReportToDBOP db_reporter
) {
	bound_wt_db_reporter_ = utility::pointer::dynamic_pointer_cast < protocols::features::ReportToDB > ( db_reporter->clone() );
	bound_wt_db_reporter_->set_batch_name( "bound_wt_" + bound_wt_db_reporter_->get_batch_name() );

	unbound_wt_db_reporter_ = utility::pointer::dynamic_pointer_cast < protocols::features::ReportToDB > ( db_reporter->clone() );
	unbound_wt_db_reporter_->set_batch_name( "unbound_wt_" + unbound_wt_db_reporter_->get_batch_name() );

	bound_mut_db_reporter_ = utility::pointer::dynamic_pointer_cast < protocols::features::ReportToDB > ( db_reporter->clone() );
	bound_mut_db_reporter_->set_batch_name( "bound_mut_" + bound_mut_db_reporter_->get_batch_name() );

	unbound_mut_db_reporter_ = utility::pointer::dynamic_pointer_cast < protocols::features::ReportToDB > ( db_reporter->clone() );
	unbound_mut_db_reporter_->set_batch_name( "unbound_mut_" + unbound_mut_db_reporter_->get_batch_name() );

	report_to_db_ = true;
}

std::set< core::Size >
InterfaceDdGMover::get_movable_jumps( core::pose::Pose const & pose ) const {

	std::set< core::Size > jumps;

	if ( !jump_ids_.empty() && ( !chain_ids_.empty() || !chain_names_.empty() ) ) {
		throw CREATE_EXCEPTION(utility::excn::BadInput, "you can specify either chains or jump in the ddG mover, but not both");
	}

	if ( !jump_ids_.empty() ) {
		// We can't/don't need to invert if we're just jump specifications.
		jumps.insert( jump_ids_.begin(), jump_ids_.end() );
		return jumps;
	}

	// There's probably a way to do this which is slightly more direct, but this was (more or less) the existing algorithm.

	std::set< core::Size > chain_ids( chain_ids_.begin(), chain_ids_.end() );

	for ( std::string const & chain_name: chain_names_ ) {
		auto const & ids_from_name = core::pose::get_chain_ids_from_chain(chain_name, pose);
		chain_ids.insert( ids_from_name.begin(), ids_from_name.end() );
	}

	// Test to see if chain 1 is selected to be unbound
	// If so, invert selection of chains to move, as we can't move the root chain 1, but we
	// can move everything else to get the same result
	bool invert_chain_ids = (chain_ids.count(1) >= 1);

	std::set<core::Size> chain_ids_to_move;
	if ( invert_chain_ids ) {
		// Add inverted chain ids
		for ( core::Size pose_chain = 1 ; pose_chain <= pose.conformation().num_chains() ; pose_chain++ ) {
			if ( chain_ids.count( pose_chain ) == 0 ) {
				chain_ids_to_move.insert( pose_chain );
			}
		}
	} else {
		// Add non-inverted chain ids
		chain_ids_to_move = chain_ids;
	}

	if ( chain_ids_to_move.empty() ) {
		throw CREATE_EXCEPTION(utility::excn::BadInput, "In InterfaceDdGMover, there are no valid chains to translate when unbinding!");
	}

	// Print chain_ids_to_move to tracer
	if ( invert_chain_ids ) {
		TR << "Translating/unbinding (inverted because we can't move the first chain) chain ID(s): ";
	} else {
		TR << "Translating/unbinding chain ID(s): ";
	}
	for ( auto & chain_id : chain_ids_to_move ) {
		TR << chain_id << " ";
	}
	TR << std::endl;

	// Check that all chain ids are actually in passed Pose, and convert to jumps
	for ( auto & chain_id : chain_ids_to_move ) {
		runtime_assert( chain_id >= 1 && chain_id <= pose.conformation().num_chains() );
		jumps.insert( core::pose::get_jump_id_from_chain_id( chain_id, pose ) );
	}

	return jumps;
}

void
InterfaceDdGMover::unbind (
	core::pose::Pose & pose
) const {

	if ( core::pose::symmetry::is_symmetric( pose ) ) {
		// Have to defer this check to now, as we don't have a pose to check the symmetry of at parse time.
		if ( ! jump_ids_.empty() || ! chain_names_.empty() || chain_ids_.size() > 1 || ( chain_ids_.size() == 1 && chain_ids_[1] != 1 ) ) {
			throw CREATE_EXCEPTION(utility::excn::BadInput, "You can't specify chains or jumps when using a symmetric pose with InterfaceDdGMover." );
		}

		auto & symm_conf( dynamic_cast<core::conformation::symmetry::SymmetricConformation & > ( pose.conformation()) );
		std::map< core::Size, core::conformation::symmetry::SymDof > dofs ( symm_conf.Symmetry_Info()->get_dofs() );

		rigid::RigidBodyDofSeqTransMoverOP translate( new rigid::RigidBodyDofSeqTransMover( dofs ) );
		translate->step_size( translate_by_ );
		translate->apply( pose );
		return;
	}

	std::set< core::Size > jumps_to_move = get_movable_jumps(pose);

	if ( jumps_to_move.empty() ) {
		// We have no information about chains or jumps to move, so let's not try and unbind
		throw CREATE_EXCEPTION(utility::excn::BadInput,  "InterfaceDdGMover needs to have chains (numbers or names) or jumps defined to move" );
	}

	// We want to translate each jump the same direction, though it doesnt matter much which one
	core::Vector translation_axis(1,0,0);
	for ( core::Size jump_id: jumps_to_move ) {
		rigid::RigidBodyTransMoverOP translate( new rigid::RigidBodyTransMover( pose, jump_id ) );
		translate->step_size( translate_by_ );
		translate->trans_axis( translation_axis );
		translate->apply( pose );
	}
}

protocols::moves::MoverOP
InterfaceDdGMover::clone() const
{
	return utility::pointer::make_shared< InterfaceDdGMover >( *this );
}


protocols::moves::MoverOP
InterfaceDdGMover::fresh_instance() const
{
	return utility::pointer::make_shared< InterfaceDdGMover >();
}

std::string
InterfaceDdGMover::get_name() const
{
	return InterfaceDdGMoverCreator::mover_name();
}

std::string
InterfaceDdGMover::mover_name() {
	return "InterfaceDdGMover";
}

void
InterfaceDdGMover::apply( core::pose::Pose & pose )
{
	const_pose_apply( pose );
}

void
InterfaceDdGMover::const_pose_apply( core::pose::Pose const & pose ) {
	if ( apply_pose_is_mutant_ ) {
		set_mut_pose( pose );
	} else {
		set_wt_pose( pose );
	}

	compute();

	if ( report_to_db_ ) {
		bound_wt_db_reporter_->apply( *bound_wt_pose_ );
		unbound_wt_db_reporter_->apply( *unbound_wt_pose_ );
		bound_mut_db_reporter_->apply( *bound_mut_pose_ );
		unbound_mut_db_reporter_->apply( *unbound_mut_pose_ );
	}
}

///@brief Determines which residues, if any, are different between bound_wt_pose_ and bound_mut_pose_
utility::vector1< std::tuple<std::string, core::Size, std::string> >
InterfaceDdGMover::mutation_list() const {

	utility::vector1< std::tuple<std::string, core::Size, std::string> > mutations;

	if ( bound_wt_pose_->size() != bound_mut_pose_->size() ) {
		TR.Warning << "Reference (wildtype) pose and current (mutant) pose have a different number of residues; cannot compare sequences" << std::endl;
		return mutations;
	}

	for ( core::Size resi = 1 ; resi <= bound_wt_pose_->size() ; ++resi ) {
		if ( bound_wt_pose_->residue(resi).name3() != bound_mut_pose_->residue(resi).name3() ) {
			mutations.push_back( std::make_tuple(
				bound_wt_pose_->residue(resi).name3(),
				resi,
				bound_mut_pose_->residue(resi).name3()
				) );
		}
	}

	return mutations;

}


core::Real
InterfaceDdGMover::compute()
{
	// See if we have pointers to SavePoseMover(s)
	if ( mut_save_pose_mover_ != nullptr ) {
		set_mut_pose( mut_save_pose_mover_->get_cloned_saved_pose() );
	}
	if ( wt_save_pose_mover_ != nullptr ) {
		set_wt_pose( wt_save_pose_mover_->get_cloned_saved_pose() );
	}

	// Runtime assert for these pointers, as the use may forget to set them
	runtime_assert( bound_mut_pose_ != nullptr );
	runtime_assert( bound_wt_pose_ != nullptr );

	// Debug assert for these pointers, as this class should always make sure they are set
	// if the bound pointers above are set
	debug_assert( unbound_wt_pose_ != nullptr );
	debug_assert( unbound_mut_pose_ != nullptr );

	unbound_wt_score_ = ( *scorefxn_ )( *unbound_wt_pose_ );
	bound_wt_score_ = ( *scorefxn_ )( *bound_wt_pose_ );
	unbound_mut_score_ = ( *scorefxn_ )( *unbound_mut_pose_ );
	bound_mut_score_ = ( *scorefxn_ )( *bound_mut_pose_ );
	ddG_score_ = (bound_mut_score_ - unbound_mut_score_) - (bound_wt_score_ - unbound_wt_score_);

	utility::vector1< std::tuple<std::string, core::Size, std::string> > mutations = mutation_list();
	if ( mutations.size() == 0 ) {
		TR << "interface binding energy ddG score: " << ddG_score_ << std::endl;
	} else {
		TR << "Mutations between stored wildtype and mutant poses:\n";
		std::string wt_res;
		core::Size res_id;
		std::string mut_res;
		static const std::string format_string = "%6s %=10s %=10s\n";
		TR << boost::format(format_string) % "ResNum" % "WTResiType" % "MutResiType";
		for ( auto & mutation_tuple : mutations ) {
			std::tie( wt_res, res_id, mut_res ) = mutation_tuple;
			TR << boost::format(format_string) % utility::to_string(res_id) % wt_res % mut_res;
		}
		TR << std::endl;

		TR << "interface binding energy ddG score (after mutation(s)): " << ddG_score_ << std::endl;
	}

	return ddG_score_;
}

///@brief appends chain_id to chain_ids_
///@details takes pose in as an argument for input checking
void
InterfaceDdGMover::add_chain_id( core::Size chain_id, core::pose::Pose const & pose ) {
	if ( ! core::pose::has_chain( chain_id, pose ) ) {
		throw CREATE_EXCEPTION(utility::excn::BadInput,
			"InterfaceDdGMover cannot add chain_id " + utility::to_string( chain_id ) + " to pose; out of range"
		);
	}

	chain_ids_.push_back( chain_id );
}

void
InterfaceDdGMover::add_chain_id( core::Size chain_id ) {
	chain_ids_.push_back( chain_id );
}

///@brief converts chain_name to a chain_id and then appends
///@details takes pose in as an argument for input checking
void
InterfaceDdGMover::add_chain_name( std::string const & chain_name, core::pose::Pose const & pose ) {
	if ( ! core::pose::has_chain( chain_name, pose ) ) {
		throw CREATE_EXCEPTION(utility::excn::BadInput,
			"InterfaceDdGMover cannot add chain_name " + chain_name + " to pose; out of range"
		);
	}

	chain_names_.push_back( chain_name );
}

void
InterfaceDdGMover::add_chain_name( std::string const & chain_name ) {
	chain_names_.push_back( chain_name );
}

///@brief converts jump to a chain_id and then appends
///@details takes pose in as an argument for input checking
void
InterfaceDdGMover::add_jump_id( core::Size jump, core::pose::Pose const & pose )
{
	if ( jump > pose.num_jump() || jump < 1 ) {
		throw CREATE_EXCEPTION(utility::excn::BadInput,
			"InterfaceDdGMover cannot add jump " + utility::to_string( jump ) + " to pose; out of range"
		);
	}

	jump_ids_.push_back( jump );
}

void
InterfaceDdGMover::add_jump_id( core::Size jump ) {
	jump_ids_.push_back( jump );
}

void
InterfaceDdGMover::set_scorefxn( core::scoring::ScoreFunctionCOP sfxn_in ) {
	scorefxn_ = sfxn_in->clone();
}

/// @brief Stores the wildtype pose
void
InterfaceDdGMover::set_wt_pose( core::pose::Pose const & pose ) {
	bound_wt_pose_ = pose.clone();
	unbound_wt_pose_ = pose.clone();
	unbind( *unbound_wt_pose_ );
}

/// @brief Stores the wildtype pose
void
InterfaceDdGMover::set_wt_pose( core::pose::PoseOP pose ) {
	bound_wt_pose_ = pose;
	unbound_wt_pose_ = pose->clone();
	unbind( *unbound_wt_pose_ );
}

/// @brief Stores the mutant pose
void
InterfaceDdGMover::set_mut_pose( core::pose::Pose const & pose ) {
	bound_mut_pose_ = pose.clone();
	unbound_mut_pose_ = pose.clone();
	unbind( *unbound_mut_pose_ );
}

/// @brief Stores the mutant pose
void
InterfaceDdGMover::set_mut_pose( core::pose::PoseOP pose ) {
	bound_mut_pose_ = pose;
	unbound_mut_pose_ = pose->clone();
	unbind( *unbound_mut_pose_ );
}

core::pose::PoseCOP
InterfaceDdGMover::get_unbound_wt_pose() const {
	return unbound_wt_pose_->get_self_ptr();
}

core::pose::PoseCOP
InterfaceDdGMover::get_bound_wt_pose()  const {
	return bound_wt_pose_->get_self_ptr();
}

core::pose::PoseCOP
InterfaceDdGMover::get_unbound_mut_pose() const {
	return unbound_mut_pose_->get_self_ptr();
}

core::pose::PoseCOP
InterfaceDdGMover::get_bound_mut_pose() const {
	return bound_mut_pose_->get_self_ptr();
}

utility::vector1< std::pair<std::string, core::Real> >
InterfaceDdGMover::get_all_scores() const {
	utility::vector1< std::pair<std::string, core::Real> > all_scores;
	all_scores.push_back( std::make_pair( "unbound_wt", unbound_wt_score_ ) );
	all_scores.push_back( std::make_pair( "bound_wt", bound_wt_score_ ) );
	all_scores.push_back( std::make_pair( "unbound_mut", unbound_mut_score_ ) );
	all_scores.push_back( std::make_pair( "bound_mut", bound_mut_score_ ) );
	all_scores.push_back( std::make_pair( "ddG", ddG_score_ ) );
	return all_scores;
}

void
InterfaceDdGMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{

	using namespace utility::tag;
	AttributeList attlist;

	rosetta_scripts::attributes_for_parse_score_function( attlist );

	attlist + XMLSchemaAttribute(
		"jump", xsct_positive_integer,
		"jump that connects two sides of the interface"
	);

	attlist + XMLSchemaAttribute(
		"chain_num", xsct_positive_integer_cslist,
		"comma separated list (or just one) chain number(s) that defines one side of the interface"
	);

	attlist + XMLSchemaAttribute(
		"chain_name", xsct_chain_cslist,
		"comma separated list (or just one) name/letter of chain(s) that defines one side of the interface"
	);

	attlist + XMLSchemaAttribute(
		"mut_ref_savepose_mover", xs_string,
		"name of previously defined SavePoseMover that has stored the mutant Pose"
	);

	attlist + XMLSchemaAttribute(
		"wt_ref_savepose_mover", xs_string,
		"name of previously defined SavePoseMover that has stored the wildtype Pose"
	);

	attlist + XMLSchemaAttribute(
		"db_reporter", xs_string,
		"name of previously defined db_report mover. Reporter (and its associated features) will be run on the bound mutant, bound wildtype, unbound mutant, and unbound wildtype states"
	);

	protocols::moves::xsd_type_definition_w_attributes( xsd, mover_name(), "This mover reports the change in binding affinity (ddG) after mutation in the interface", attlist );
}

} //protocols
} //features
