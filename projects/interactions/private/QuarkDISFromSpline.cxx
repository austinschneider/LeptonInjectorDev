#include "SIREN/interactions/CharmDISFromSpline.h"

#include <map>                                             // for map, opera...
#include <set>                                             // for set, opera...
#include <array>                                           // for array
#include <cmath>                                           // for pow, log10
#include <tuple>                                           // for tie, opera...
#include <memory>                                          // for allocator
#include <string>                                          // for basic_string
#include <vector>                                          // for vector
#include <assert.h>                                        // for assert
#include <stddef.h>                                        // for size_t
#include <map>
#include <limits>
#include <cmath>
#include <string>
#include <iostream>

#include <rk/rk.hh>                                        // for P4, Boost
#include <rk/geom3.hh>                                     // for Vector3

#include <photospline/splinetable.h>                       // for splinetable
//#include <photospline/cinter/splinetable.h>

#include "SIREN/interactions/CrossSection.h"     // for CrossSection
#include "SIREN/dataclasses/InteractionRecord.h"  // for Interactio...
#include "SIREN/dataclasses/Particle.h"           // for Particle
#include "SIREN/utilities/Random.h"               // for SIREN_random
#include "SIREN/utilities/Constants.h"            // for electronMass

namespace siren {
namespace interactions {

namespace {
///Check whether a given point in phase space is physically realizable.
///Based on equations 6-8 of http://dx.doi.org/10.1103/PhysRevD.66.113007
///S. Kretzer and M. H. Reno
///"Tau neutrino deep inelastic charged current interactions"
///Phys. Rev. D 66, 113007
///\param x Bjorken x of the interaction
///\param y Bjorken y of the interaction
///\param E Incoming neutrino in energy in the lab frame ($E_\nu$)
///\param M Mass of the target nucleon ($M_N$)
///\param m Mass of the secondary lepton ($m_\tau$)
bool kinematicallyAllowed(double x, double y, double E, double M, double m) {
    if(x > 1) //Eq. 6 right inequality
        return false;
    if(x < ((m * m) / (2 * M * (E - m)))) //Eq. 6 left inequality
        return false;
    //denominator of a and b
    double d = 2 * (1 + (M * x) / (2 * E));
    //the numerator of a (or a*d)
    double ad = 1 - m * m * ((1 / (2 * M * E * x)) + (1 / (2 * E * E)));
    double term = 1 - ((m * m) / (2 * M * E * x));
    //the numerator of b (or b*d)
    double bd = sqrt(term * term - ((m * m) / (E * E)));

    double s = 2 * M * E;
    double Q2 = s * x * y;
    double Mc = siren::utilities::Constants::D0Mass;
    return ((ad - bd) <= d * y and d * y <= (ad + bd)) && (Q2 / (1 - x) + pow(M, 2) >= pow(M + Mc, 2)); //Eq. 7
}
}

CharmDISFromSpline::CharmDISFromSpline() {}

CharmDISFromSpline::CharmDISFromSpline(std::vector<char> differential_data, std::vector<char> total_data, int interaction, double target_mass, double minimum_Q2, std::set<siren::dataclasses::ParticleType> primary_types, std::set<siren::dataclasses::ParticleType> target_types, std::string units) : primary_types_(primary_types), target_types_(target_types), interaction_type_(interaction), target_mass_(target_mass), minimum_Q2_(minimum_Q2) {
    LoadFromMemory(differential_data, total_data);
    InitializeSignatures();
    SetUnits(units);
}

CharmDISFromSpline::CharmDISFromSpline(std::vector<char> differential_data, std::vector<char> total_data, int interaction, double target_mass, double minimum_Q2, std::vector<siren::dataclasses::ParticleType> primary_types, std::vector<siren::dataclasses::ParticleType> target_types, std::string units) : primary_types_(primary_types.begin(), primary_types.end()), target_types_(target_types.begin(), target_types.end()), interaction_type_(interaction), target_mass_(target_mass), minimum_Q2_(minimum_Q2) {
    LoadFromMemory(differential_data, total_data);
    InitializeSignatures();
    SetUnits(units);
}

CharmDISFromSpline::CharmDISFromSpline(std::string differential_filename, std::string total_filename, int interaction, double target_mass, double minimum_Q2, std::set<siren::dataclasses::ParticleType> primary_types, std::set<siren::dataclasses::ParticleType> target_types, std::string units) : primary_types_(primary_types), target_types_(target_types), interaction_type_(interaction), target_mass_(target_mass), minimum_Q2_(minimum_Q2) {
    LoadFromFile(differential_filename, total_filename);
    InitializeSignatures();
    SetUnits(units);
}

CharmDISFromSpline::CharmDISFromSpline(std::string differential_filename, std::string total_filename, std::set<siren::dataclasses::ParticleType> primary_types, std::set<siren::dataclasses::ParticleType> target_types, std::string units) : primary_types_(primary_types), target_types_(target_types) {
    LoadFromFile(differential_filename, total_filename);
    ReadParamsFromSplineTable();
    InitializeSignatures();
    SetUnits(units);
}

CharmDISFromSpline::CharmDISFromSpline(std::string differential_filename, std::string total_filename, int interaction, double target_mass, double minimum_Q2, std::vector<siren::dataclasses::ParticleType> primary_types, std::vector<siren::dataclasses::ParticleType> target_types, std::string units) : primary_types_(primary_types.begin(), primary_types.end()), target_types_(target_types.begin(), target_types.end()), interaction_type_(interaction), target_mass_(target_mass), minimum_Q2_(minimum_Q2) {
    LoadFromFile(differential_filename, total_filename);
    InitializeSignatures();
    SetUnits(units);
}

CharmDISFromSpline::CharmDISFromSpline(std::string differential_filename, std::string total_filename, std::vector<siren::dataclasses::ParticleType> primary_types, std::vector<siren::dataclasses::ParticleType> target_types, std::string units) : primary_types_(primary_types.begin(), primary_types.end()), target_types_(target_types.begin(), target_types.end()) {
    LoadFromFile(differential_filename, total_filename);
    ReadParamsFromSplineTable();
    InitializeSignatures();
    SetUnits(units);
}
    
void CharmDISFromSpline::SetUnits(std::string units) {
    std::transform(units.begin(), units.end(), units.begin(),
        [](unsigned char c){ return std::tolower(c); });
    if(units == "cm") {
        unit = 1.0;
    } else if(units == "m") {
        unit = 10000.0;
    } else {
        throw std::runtime_error("Cross section units not supported!");
    }
}

void CharmDISFromSpline::SetInteractionType(int interaction) {
    interaction_type_ = interaction;
}

bool CharmDISFromSpline::equal(CrossSection const & other) const {
    const CharmDISFromSpline* x = dynamic_cast<const CharmDISFromSpline*>(&other);

    if(!x)
        return false;
    else
        return
            std::tie(
            interaction_type_,
            target_mass_,
            minimum_Q2_,
            signatures_,
            primary_types_,
            target_types_,
            differential_cross_section_,
            total_cross_section_)
            ==
            std::tie(
            x->interaction_type_,
            x->target_mass_,
            x->minimum_Q2_,
            x->signatures_,
            x->primary_types_,
            x->target_types_,
            x->differential_cross_section_,
            x->total_cross_section_);
}

void CharmDISFromSpline::LoadFromFile(std::string dd_crossSectionFile, std::string total_crossSectionFile) {

    differential_cross_section_ = photospline::splinetable<>(dd_crossSectionFile.c_str());

    if(differential_cross_section_.get_ndim()!=3 && differential_cross_section_.get_ndim()!=2)
        throw std::runtime_error("cross section spline has " + std::to_string(differential_cross_section_.get_ndim())
                + " dimensions, should have either 3 (log10(E), log10(x), log10(y)) or 2 (log10(E), log10(y))");

    total_cross_section_ = photospline::splinetable<>(total_crossSectionFile.c_str());

    if(total_cross_section_.get_ndim() != 1)
        throw std::runtime_error("Total cross section spline has " + std::to_string(total_cross_section_.get_ndim())
                + " dimensions, should have 1, log10(E)");
}

void CharmDISFromSpline::LoadFromMemory(std::vector<char> & differential_data, std::vector<char> & total_data) {
    differential_cross_section_.read_fits_mem(differential_data.data(), differential_data.size());
    total_cross_section_.read_fits_mem(total_data.data(), total_data.size());
}

double CharmDISFromSpline::GetLeptonMass(siren::dataclasses::ParticleType lepton_type) {
    int32_t lepton_number = std::abs(static_cast<int32_t>(lepton_type));
    double lepton_mass;
    switch(lepton_number) {
        case 11:
            lepton_mass = siren::utilities::Constants::electronMass;
            break;
        case 13:
            lepton_mass = siren::utilities::Constants::muonMass;
            break;
        case 15:
            lepton_mass = siren::utilities::Constants::tauMass;
            break;
        case 12:
            lepton_mass = 0;
        case 14:
            lepton_mass = 0;
        case 16:
            lepton_mass = 0;
            break;
        default:
            throw std::runtime_error("Unknown lepton type!");
    }
    return lepton_mass;
}

void CharmDISFromSpline::ReadParamsFromSplineTable() {
    // returns true if successfully read target mass
    bool mass_good = differential_cross_section_.read_key("TARGETMASS", target_mass_);
    if (mass_good) {std::cout << "read target mass!!" << std::endl;} // for debugging purposes
    // returns true if successfully read interaction type
    bool int_good = differential_cross_section_.read_key("INTERACTION", interaction_type_);
    // returns true if successfully read minimum Q2
    bool q2_good = differential_cross_section_.read_key("Q2MIN", minimum_Q2_);

    if(!int_good) {
        // assume DIS to preserve compatability with previous versions
        interaction_type_ = 1;
    }

    if(!q2_good) {
        // assume 1 GeV^2
        minimum_Q2_ = 1;
    }

    std::cout << "Q2 good status is "  << q2_good << "and is set to " << minimum_Q2_;

    if(!mass_good) {
        if(int_good) {
            if(interaction_type_ == 1 or interaction_type_ == 2) {
                target_mass_ = (siren::dataclasses::isLepton(siren::dataclasses::ParticleType::PPlus)+
                        siren::dataclasses::isLepton(siren::dataclasses::ParticleType::Neutron))/2;
            } else if(interaction_type_ == 3) {
                target_mass_ = siren::dataclasses::isLepton(siren::dataclasses::ParticleType::EMinus);
            } else {
                throw std::runtime_error("Logic error. Interaction type is not 1, 2, or 3!");
            }

        } else {
            if(differential_cross_section_.get_ndim() == 3) {
                target_mass_ = siren::utilities::Constants::isoscalarMass;
                
            } else if(differential_cross_section_.get_ndim() == 2) {
                target_mass_ = siren::utilities::Constants::electronMass;
            } else {
                throw std::runtime_error("Logic error. Spline dimensionality is not 2, or 3!");
            }
        }
    }
    std::cout << "target mass is " << target_mass_ << std::endl;

}

void CharmDISFromSpline::InitializeSignatures() {
    signatures_.clear();
    for(auto primary_type : primary_types_) {
        dataclasses::InteractionSignature signature;
        signature.primary_type = primary_type;

        if(not isNeutrino(primary_type)) {
            throw std::runtime_error("This DIS implementation only supports neutrinos as primaries!");
        }

        siren::dataclasses::ParticleType charged_lepton_product = siren::dataclasses::ParticleType::unknown;
        siren::dataclasses::ParticleType neutral_lepton_product = primary_type;

        if(primary_type == siren::dataclasses::ParticleType::NuE) {
            charged_lepton_product = siren::dataclasses::ParticleType::EMinus;
        } else if(primary_type == siren::dataclasses::ParticleType::NuEBar) {
            charged_lepton_product = siren::dataclasses::ParticleType::EPlus;
        } else if(primary_type == siren::dataclasses::ParticleType::NuMu) {
            charged_lepton_product = siren::dataclasses::ParticleType::MuMinus;
        } else if(primary_type == siren::dataclasses::ParticleType::NuMuBar) {
            charged_lepton_product = siren::dataclasses::ParticleType::MuPlus;
        } else if(primary_type == siren::dataclasses::ParticleType::NuTau) {
            charged_lepton_product = siren::dataclasses::ParticleType::TauMinus;
        } else if(primary_type == siren::dataclasses::ParticleType::NuTauBar) {
            charged_lepton_product = siren::dataclasses::ParticleType::TauPlus;
        } else {
            throw std::runtime_error("InitializeSignatures: Unkown parent neutrino type!");
        }

        if(interaction_type_ == 1) {
            signature.secondary_types.push_back(charged_lepton_product);
        } else if(interaction_type_ == 2) {
            signature.secondary_types.push_back(neutral_lepton_product);
        } else if(interaction_type_ == 3) {
            signature.secondary_types.push_back(siren::dataclasses::ParticleType::Hadrons);
        } else {
            throw std::runtime_error("InitializeSignatures: Unkown interaction type!");
        }

        signature.secondary_types.push_back(siren::dataclasses::ParticleType::Charm);
        for(auto target_type : target_types_) {
            signature.target_type = target_type;

            signatures_.push_back(signature);

            std::pair<siren::dataclasses::ParticleType, siren::dataclasses::ParticleType> key(primary_type, target_type);
            signatures_by_parent_types_[key].push_back(signature);
        }
    }
}

double CharmDISFromSpline::TotalCrossSection(dataclasses::InteractionRecord const & interaction) const {
    siren::dataclasses::ParticleType primary_type = interaction.signature.primary_type;
    rk::P4 p1(geom3::Vector3(interaction.primary_momentum[1], interaction.primary_momentum[2], interaction.primary_momentum[3]), interaction.primary_mass);
    double primary_energy;
    primary_energy = interaction.primary_momentum[0];
    // if we are below threshold, return 0
    if(primary_energy < InteractionThreshold(interaction)) {
        std::cout << "DIS::interaction threshold not satisfied" << std::endl;
        return 0;
    }
    return TotalCrossSection(primary_type, primary_energy);
}

double CharmDISFromSpline::TotalCrossSection(siren::dataclasses::ParticleType primary_type, double primary_energy) const {
    if(not primary_types_.count(primary_type)) {
        throw std::runtime_error("Supplied primary not supported by cross section!");
    }
    double log_energy = log10(primary_energy);

    if(log_energy < total_cross_section_.lower_extent(0)
            or log_energy > total_cross_section_.upper_extent(0)) {
        throw std::runtime_error("Interaction energy ("+ std::to_string(primary_energy) +
                ") out of cross section table range: ["
                + std::to_string(pow(10.,total_cross_section_.lower_extent(0))) + " GeV,"
                + std::to_string(pow(10.,total_cross_section_.upper_extent(0))) + " GeV]");
    }

    int center;
    total_cross_section_.searchcenters(&log_energy, &center);

    double log_xs = total_cross_section_.ndsplineeval(&log_energy, &center, 0);
    if (std::pow(10.0, log_xs) == 0) {
        std::cout << "DIS::cross section evaluated to 0" << std::endl;
    }

    return unit * std::pow(10.0, log_xs);
}

double CharmDISFromSpline::DifferentialCrossSection(dataclasses::InteractionRecord const & interaction) const {
    rk::P4 p1(geom3::Vector3(interaction.primary_momentum[1], interaction.primary_momentum[2], interaction.primary_momentum[3]), interaction.primary_mass);
    rk::P4 p2(geom3::Vector3(0, 0, 0), interaction.target_mass);
    double primary_energy;
    primary_energy = interaction.primary_momentum[0];
    assert(interaction.signature.secondary_types.size() == 2);
    unsigned int lepton_index = (isLepton(interaction.signature.secondary_types[0])) ? 0 : 1;
    unsigned int other_index = 1 - lepton_index;

    std::array<double, 4> const & mom3 = interaction.secondary_momenta[lepton_index];
    std::array<double, 4> const & mom4 = interaction.secondary_momenta[other_index];
    rk::P4 p3(geom3::Vector3(mom3[1], mom3[2], mom3[3]), interaction.secondary_masses[lepton_index]);
    rk::P4 p4(geom3::Vector3(mom4[1], mom4[2], mom4[3]), interaction.secondary_masses[other_index]);

    rk::P4 q = p1 - p3;

    double Q2 = -q.dot(q);
    double x, y;
    double lepton_mass = GetLeptonMass(interaction.signature.secondary_types[lepton_index]);


    y = 1.0 - p2.dot(p3) / p2.dot(p1);
    x = Q2 / (2.0 * p2.dot(q));
    double log_energy = log10(primary_energy);
    std::array<double,3> coordinates{{log_energy, log10(x), log10(y)}};
    std::array<int,3> centers;


    if (Q2 < minimum_Q2_ || !kinematicallyAllowed(x, y, primary_energy, target_mass_, lepton_mass)
        || !differential_cross_section_.searchcenters(coordinates.data(), centers.data())) {
                // std::cout << "weighting: revert back to saved x and y" << std::endl;
            double E1_lab = interaction.interaction_parameters.at("energy");
            double E2_lab = p2.e();
            x = interaction.interaction_parameters.at("bjorken_x");
            y = interaction.interaction_parameters.at("bjorken_y");
            Q2 = 2. * E1_lab * E2_lab * x * y;
    }
    return DifferentialCrossSection(primary_energy, x, y, lepton_mass, Q2);


}

double CharmDISFromSpline::DifferentialCrossSection(double energy, double x, double y, double secondary_lepton_mass, double Q2) const {
    double log_energy = log10(energy);
    // check preconditions
    if(log_energy < differential_cross_section_.lower_extent(0)
            || log_energy>differential_cross_section_.upper_extent(0))
        {std::cout << "Diff xsec: not in bounds" << std::endl;
            return 0.0;}
    if(x <= 0 || x >= 1) {
        std::cout << "x is out of bounds with x = " << x << std::endl;
        return 0.0;
    }
    if(y <= 0 || y >= 1){
        std::cout << "y is out of bounds with x = " << y << std::endl;
        return 0.0;
    }

    // we assume that:
    // the target is stationary so its energy is just its mass
    // the incoming neutrino is massless, so its kinetic energy is its total energy
    if(std::isnan(Q2)) {
        Q2 = 2.0 * energy * target_mass_ * x * y;
    }
    if(Q2 < minimum_Q2_) {
        std::cout << "Q2 is smaller than minimum Q2 with " << Q2 << " < " << minimum_Q2_ << std::endl;
        return 0;
    } // cross section not calculated, assumed to be zero

    if(!kinematicallyAllowed(x, y, energy, target_mass_, secondary_lepton_mass)) {
        std::cout << "not kinematically allowed!" << std::endl;
        return 0;
    }
    std::array<double,3> coordinates{{log_energy, log10(x), log10(y)}};
    std::array<int,3> centers;
    if(!differential_cross_section_.searchcenters(coordinates.data(), centers.data())) {
        std::cout << "search centers failed!" << std::endl;
        return 0;
    }
    double result = pow(10., differential_cross_section_.ndsplineeval(coordinates.data(), centers.data(), 0));
    assert(result >= 0);
    if (std::isinf(result)) {
        std::cout << "energy, x, y, Q2 are " << energy << " " << x << " " << y << " " << Q2 << " " << std::endl;
        std::cout << "spline value read is " << differential_cross_section_.ndsplineeval(coordinates.data(), centers.data(), 0) << std::endl;
    }

    return unit * result;
}

double CharmDISFromSpline::InteractionThreshold(dataclasses::InteractionRecord const & interaction) const {
    // Consider implementing DIS thershold at some point
    return 0;
}

void CharmDISFromSpline::SampleFinalState(dataclasses::CrossSectionDistributionRecord & record, std::shared_ptr<siren::utilities::SIREN_random> random) const {
    // Uses Metropolis-Hastings Algorithm!
    // useful for cases where we don't know the supremum of our distribution, and the distribution is multi-dimensional
    if (differential_cross_section_.get_ndim() != 3) {
        throw std::runtime_error("I expected 3 dimensions in the cross section spline, but got " + std::to_string(differential_cross_section_.get_ndim()) +". Maybe your fits file doesn't have the right 'INTERACTION' key?");
    }
    rk::P4 p1(geom3::Vector3(record.primary_momentum[1], record.primary_momentum[2], record.primary_momentum[3]), record.primary_mass);
    rk::P4 p2(geom3::Vector3(0, 0, 0), record.target_mass);

    // we assume that:
    // the target is stationary so its energy is just its mass
    // the incoming neutrino is massless, so its kinetic energy is its total energy
    // double s = target_mass_ * trecord.secondary_momentarget_mass_ + 2 * target_mass_ * primary_energy;
    // double s = std::pow(rk::invMass(p1, p2), 2);

    double primary_energy;
    rk::P4 p1_lab;
    rk::P4 p2_lab;
    p1_lab = p1;
    p2_lab = p2;
    primary_energy = p1_lab.e();

    unsigned int lepton_index = (isLepton(record.signature.secondary_types[0])) ? 0 : 1;
    unsigned int other_index = 1 - lepton_index;
    double m = GetLeptonMass(record.signature.secondary_types[lepton_index]);

    double m1 = record.primary_mass;
    double m3 = m;
    double E1_lab = p1_lab.e();
    double E2_lab = p2_lab.e();

    // The out-going particle always gets at least enough energy for its rest mass
    double yMax = 1 - m / primary_energy;
    double logYMax = log10(yMax);

    // The minimum allowed value of y occurs when x = 1 and Q is minimized
    double yMin = minimum_Q2_ / (2 * E1_lab * E2_lab);
    double logYMin = log10(yMin);
    // The minimum allowed value of x occurs when y = yMax and Q is minimized
    // double xMin = minimum_Q2_ / ((s - target_mass_ * target_mass_) * yMax);
    double xMin = minimum_Q2_ / (2 * E1_lab * E2_lab * yMax);
    double logXMin = log10(xMin);

    bool accept;

    // kin_vars and its twin are 3-vectors containing [nu-energy, Bjorken X, Bjorken Y]
    std::array<double,3> kin_vars, test_kin_vars;

    // centers of the cross section spline tales.
    std::array<int,3> spline_table_center, test_spline_table_center;

    // values of cross_section from the splines.  By * Bx * Spline(E,x,y)
    double cross_section, test_cross_section;

    // No matter what, we're evaluating at this specific energy.
    kin_vars[0] = test_kin_vars[0] = log10(primary_energy);

    // check preconditions
    if(kin_vars[0] < differential_cross_section_.lower_extent(0)
            || kin_vars[0] > differential_cross_section_.upper_extent(0))
        throw std::runtime_error("Interaction energy out of cross section table range: ["
                + std::to_string(pow(10.,differential_cross_section_.lower_extent(0))) + " GeV,"
                + std::to_string(pow(10.,differential_cross_section_.upper_extent(0))) + " GeV]");

    // sample an intial point
    do {
        // rejection sample a point which is kinematically allowed by calculation limits
        double trialQ;
        double trials = 0;
        do {
            if (trials >= 100) throw std::runtime_error("too much trials");
            trials += 1;
            kin_vars[1] = random->Uniform(logXMin,0);
            kin_vars[2] = random->Uniform(logYMin,logYMax);
            trialQ = (2 * E1_lab * E2_lab) * pow(10., kin_vars[1] + kin_vars[2]);
        } while(trialQ<minimum_Q2_ || !kinematicallyAllowed(pow(10., kin_vars[1]), pow(10., kin_vars[2]), primary_energy, target_mass_, m));

        accept = true;
        //sanity check: demand that the sampled point be within the table extents
        if(kin_vars[1] < differential_cross_section_.lower_extent(1)
                || kin_vars[1] > differential_cross_section_.upper_extent(1)) {
            accept = false;
        }
        if(kin_vars[2] < differential_cross_section_.lower_extent(2)
                || kin_vars[2] > differential_cross_section_.upper_extent(2)) {
            accept = false;
        }

        if(accept) {
            // finds the centers in the cross section spline table, returns true if it's successful
            // also sets the centers
            accept = differential_cross_section_.searchcenters(kin_vars.data(),spline_table_center.data());
        }
    } while(!accept);

    //TODO: better proposal distribution?
    double measure = pow(10., kin_vars[1] + kin_vars[2]); // Bx * By

    // Bx * By * xs(E, x, y)
    // evalutates the differential spline at that point
    cross_section = measure*pow(10., differential_cross_section_.ndsplineeval(kin_vars.data(), spline_table_center.data(), 0));

    // this is the magic part. Metropolis Hastings Algorithm.
    // MCMC method!
    const size_t burnin = 40; // converges to the correct distribution over multiple samplings.
    // big number means more accurate, but slower
    for(size_t j = 0; j <= burnin; j++) {
        // repeat the sampling from above to get a new valid point
        double trialQ;
        do {
            test_kin_vars[1] = random->Uniform(logXMin, 0);
            test_kin_vars[2] = random->Uniform(logYMin, logYMax);
            trialQ = (2 * E1_lab * E2_lab) * pow(10., test_kin_vars[1] + test_kin_vars[2]);
        } while(trialQ < minimum_Q2_ || !kinematicallyAllowed(pow(10., test_kin_vars[1]), pow(10., test_kin_vars[2]), primary_energy, target_mass_, m));

        accept = true;
        if(test_kin_vars[1] < differential_cross_section_.lower_extent(1)
                || test_kin_vars[1] > differential_cross_section_.upper_extent(1))
            accept = false;
        if(test_kin_vars[2] < differential_cross_section_.lower_extent(2)
                || test_kin_vars[2] > differential_cross_section_.upper_extent(2))
            accept = false;
        if(!accept)
            continue;

        accept = differential_cross_section_.searchcenters(test_kin_vars.data(), test_spline_table_center.data());
        if(!accept)
            continue;

        double measure = pow(10., test_kin_vars[1] + test_kin_vars[2]);
        double eval = differential_cross_section_.ndsplineeval(test_kin_vars.data(), test_spline_table_center.data(), 0);
        if(std::isnan(eval))
            continue;
        test_cross_section = measure * pow(10., eval);

        double odds = (test_cross_section / cross_section);
        accept = (cross_section == 0 || (odds > 1.) || random->Uniform(0, 1) < odds);

        if(accept) {
            kin_vars = test_kin_vars;
            cross_section = test_cross_section;
            // std::cout << "trial Q is" << trialQ << std::endl;
        }
    }
    ////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////
    double final_x = pow(10., kin_vars[1]);
    double final_y = pow(10., kin_vars[2]);
    record.interaction_parameters.clear();
    record.interaction_parameters["energy"] = E1_lab;
    record.interaction_parameters["bjorken_x"] = final_x;
    record.interaction_parameters["bjorken_y"] = final_y;

    double Q2 = 2 * E1_lab * E2_lab * pow(10.0, kin_vars[1] + kin_vars[2]);
    double p1x_lab = std::sqrt(p1_lab.px() * p1_lab.px() + p1_lab.py() * p1_lab.py() + p1_lab.pz() * p1_lab.pz());
    double pqx_lab = (m1*m1 + m3*m3 + 2 * p1x_lab * p1x_lab + Q2 + 2 * E1_lab * E1_lab * (final_y - 1)) / (2.0 * p1x_lab);
    double momq_lab = std::sqrt(m1*m1 + p1x_lab*p1x_lab + Q2 + E1_lab * E1_lab * (final_y * final_y - 1));
    double pqy_lab, Eq_lab;

    if (pqx_lab>momq_lab){
        // if current setting does not work, start looping through scalings
        int maxIterations = 10; 
        int iteration = 0;
        double p1_lab_x = p1_lab.px();
        double p1_lab_y = p1_lab.py();
        double p1_lab_z = p1_lab.pz();
        // loop to resolve precision issue
        while (iteration <= maxIterations) {
            Q2 = 2. * E1_lab * E2_lab * pow(10.0, kin_vars[1] + kin_vars[2]);
            p1x_lab = std::sqrt(p1_lab_x * p1_lab_x + p1_lab_y * p1_lab_y + p1_lab_z * p1_lab_z);
            pqx_lab = (m1*m1 + m3*m3 + 2 * p1x_lab * p1x_lab + Q2 + 2 * E1_lab * E1_lab * (final_y - 1)) / (2.0 * p1x_lab);
            momq_lab = std::sqrt(m1*m1 + p1x_lab*p1x_lab + Q2 + E1_lab * E1_lab * (final_y * final_y - 1));
            if (pqx_lab>momq_lab){
                // std::cout << "triggered on " << momq_lab << " and " << pqx_lab << std::endl;
                //scale down 
                E1_lab /= 10;
                E2_lab /= 10;
                p1_lab_x /= 10;
                p1_lab_y /= 10;
                p1_lab_z /= 10;
                m1 /= 10;
                m3 /= 10;
                //iteration += 1 to scale back
                iteration += 1;
                continue;
            }
            pqy_lab = std::sqrt((momq_lab + pqx_lab) * (momq_lab - pqx_lab));
            // std::cout << "finished with " << iteration << " iterations and " << momq_lab << " and " << pqx_lab << std::endl;
            break;
        }
            // //scale back
        if (iteration > 0) {
            // std::cout << "scaling back with " << pow(10.0, iteration);
            E1_lab *= pow(10.0, iteration);
            E2_lab *= pow(10.0, iteration);
            p1_lab_x *= pow(10.0, iteration);
            p1_lab_y *= pow(10.0, iteration);
            p1_lab_z *= pow(10.0, iteration);
            m1 *= pow(10.0, iteration);
            m3 *= pow(10.0, iteration);
            // std::cout << "and finished with " << momq_lab << " and " << pqx_lab << std::endl;
        }
        // pqy_lab = 0;
    } else {pqy_lab = std::sqrt(momq_lab*momq_lab - pqx_lab *pqx_lab);}
    Eq_lab = E1_lab * final_y;

    geom3::UnitVector3 x_dir = geom3::UnitVector3::xAxis();
    geom3::Vector3 p1_mom = p1_lab.momentum();
    geom3::UnitVector3 p1_lab_dir = p1_mom.direction();
    geom3::Rotation3 x_to_p1_lab_rot = geom3::rotationBetween(x_dir, p1_lab_dir);

    double phi = random->Uniform(0, 2.0 * M_PI);
    geom3::Rotation3 rand_rot(p1_lab_dir, phi);

    rk::P4 pq_lab(Eq_lab, geom3::Vector3(pqx_lab, pqy_lab, 0));
    pq_lab.rotate(x_to_p1_lab_rot);
    pq_lab.rotate(rand_rot);

    rk::P4 p3_lab((p1_lab - pq_lab).momentum(), m3);
    rk::P4 p4_lab = p2_lab + pq_lab;

    rk::P4 p3;
    rk::P4 p4;
    p3 = p3_lab;
    p4 = p4_lab;

    std::vector<siren::dataclasses::SecondaryParticleRecord> & secondaries = record.GetSecondaryParticleRecords();
    siren::dataclasses::SecondaryParticleRecord & lepton = secondaries[lepton_index];
    siren::dataclasses::SecondaryParticleRecord & other = secondaries[other_index];

    lepton.SetFourMomentum({p3.e(), p3.px(), p3.py(), p3.pz()});
    lepton.SetMass(p3.m());
    lepton.SetHelicity(record.primary_helicity);
    other.SetFourMomentum({p4.e(), p4.px(), p4.py(), p4.pz()});
    other.SetMass(p4.m());
    other.SetHelicity(record.target_helicity);
}

double CharmDISFromSpline::FinalStateProbability(dataclasses::InteractionRecord const & interaction) const {
    double dxs = DifferentialCrossSection(interaction);
    // if (dxs == 0) {
    //     std::cout << "diff xsec gives 0" << std::endl;
    // }
    double txs = TotalCrossSection(interaction);
    if(dxs == 0) {
        return 0.0;
    } else {
        // if (txs == 0) {std::cout << "wtf??? txs is 0 in final state prob" << txs << std::endl;}
        // if (std::isinf(dxs)) {std::cout << "dxs is inf in final state prob" << std::endl;}
        return dxs / txs;
    }
}

std::vector<siren::dataclasses::ParticleType> CharmDISFromSpline::GetPossiblePrimaries() const {
    return std::vector<siren::dataclasses::ParticleType>(primary_types_.begin(), primary_types_.end());
}

std::vector<siren::dataclasses::ParticleType> CharmDISFromSpline::GetPossibleTargetsFromPrimary(siren::dataclasses::ParticleType primary_type) const {
    return std::vector<siren::dataclasses::ParticleType>(target_types_.begin(), target_types_.end());
}

std::vector<dataclasses::InteractionSignature> CharmDISFromSpline::GetPossibleSignatures() const {
    return std::vector<dataclasses::InteractionSignature>(signatures_.begin(), signatures_.end());
}

std::vector<siren::dataclasses::ParticleType> CharmDISFromSpline::GetPossibleTargets() const {
    return std::vector<siren::dataclasses::ParticleType>(target_types_.begin(), target_types_.end());
}

std::vector<dataclasses::InteractionSignature> CharmDISFromSpline::GetPossibleSignaturesFromParents(siren::dataclasses::ParticleType primary_type, siren::dataclasses::ParticleType target_type) const {
    std::pair<siren::dataclasses::ParticleType, siren::dataclasses::ParticleType> key(primary_type, target_type);
    if(signatures_by_parent_types_.find(key) != signatures_by_parent_types_.end()) {
        return signatures_by_parent_types_.at(key);
    } else {
        return std::vector<dataclasses::InteractionSignature>();
    }
}

std::vector<std::string> CharmDISFromSpline::DensityVariables() const {
    return std::vector<std::string>{"Bjorken x", "Bjorken y"};
}

} // namespace interactions
} // namespace siren
