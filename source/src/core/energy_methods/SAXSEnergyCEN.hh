// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/scoring/saxs/SAXSEnergy.hh
/// @brief  "Energy" based on a similarity of theoretical SAXS spectrum computed for a pose and the experimental data
/// @author Dominik Gront (dgront@chem.uw.edu.pl)


#ifndef INCLUDED_core_scoring_saxs_SAXSEnergyCEN_hh
#define INCLUDED_core_scoring_saxs_SAXSEnergyCEN_hh

// Package headers
#include <core/energy_methods/SAXSEnergyCreatorCEN.hh>
#include <core/energy_methods/SAXSEnergy.hh>


#include <core/scoring/ScoreType.hh>


#include <core/chemical/ChemicalManager.hh>


namespace core {
namespace energy_methods {


class SAXSEnergyCEN : public SAXSEnergy  {
public:

	/// c-tor
	SAXSEnergyCEN() : SAXSEnergy( cen_cfg_file_,
		chemical::ChemicalManager::get_instance()->residue_type_set(core::chemical::CENTROID), core::scoring::saxs_cen_score, utility::pointer::make_shared< SAXSEnergyCreatorCEN >()) {}

};


}
}

#endif
