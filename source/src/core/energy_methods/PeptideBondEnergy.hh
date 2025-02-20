// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/energy_methods/PeptideBondEnergy.hh
/// @brief  scoring method that defines ideal bond lengths between
/// carbonyl carbon of residue N and nitrogen of residue N+1.
/// @author James Thompson

#ifndef INCLUDED_core_energy_methods_PeptideBondEnergy_hh
#define INCLUDED_core_energy_methods_PeptideBondEnergy_hh

// Unit headers
#include <core/energy_methods/PeptideBondEnergy.fwd.hh>

// Package headers
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/EnergyMap.fwd.hh>
#include <core/scoring/methods/ContextIndependentTwoBodyEnergy.hh>

// Project headers
#include <core/pose/Pose.fwd.hh>
#include <core/kinematics/DomainMap.fwd.hh>
#include <core/id/AtomID.fwd.hh>

#include <utility/vector1.hh>


namespace core {
namespace energy_methods {


/// @brief PeptideBondEnergy class iterates across all residues in finalize()
/// and determines the penalty between residues i and i+1 by the distance
/// the C-N bond. Evantually I'd also like to add bond angle constraints as
/// well, but that's handled by OmegaTether at the moment.
//class PeptideBondEnergy : public core::scoring::methods::WholeStructureEnergy  {
class PeptideBondEnergy : public core::scoring::methods::ContextIndependentTwoBodyEnergy  {
public:
	typedef core::scoring::methods::ContextIndependentTwoBodyEnergy  parent;

public:

	PeptideBondEnergy();

	/// clone
	core::scoring::methods::EnergyMethodOP
	clone() const override
	{
		return utility::pointer::make_shared< PeptideBondEnergy >();
	}

	/// called at the end of energy evaluation
	//virtual
	//void
	//finalize_total_energy(
	// pose::Pose & pose,
	// core::scoring::ScoreFunction const &,
	// core::scoring::EnergyMap & totals
	//) const;

	void
	residue_pair_energy(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		pose::Pose const & pose,
		core::scoring::ScoreFunction const &,
		core::scoring::EnergyMap & emap
	) const override;

	void
	eval_intrares_energy(
		conformation::Residue const &,
		pose::Pose const &,
		core::scoring::ScoreFunction const &,
		core::scoring::EnergyMap &
	) const override {}

	/// called during gradient-based minimization inside dfunc
	/**
	F1 and F2 are not zeroed -- contributions from this atom are
	just summed in
	**/
	void
	eval_atom_derivative(
		id::AtomID const & id,
		pose::Pose const & pose,
		kinematics::DomainMap const & domain_map,
		core::scoring::ScoreFunction const & sfxn,
		core::scoring::EnergyMap const & weights,
		Vector & F1,
		Vector & F2
	) const override;

	void
	indicate_required_context_graphs( utility::vector1< bool > & ) const override;

	bool
	defines_intrares_energy( core::scoring::EnergyMap const & /*weights*/ ) const override {
		return false;
	}

	Distance
	atomic_interaction_cutoff() const override;
	core::Size version() const override;

private:

};

} // scoring
} // core


#endif
