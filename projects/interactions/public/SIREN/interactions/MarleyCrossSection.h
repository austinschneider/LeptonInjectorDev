#pragma once
#ifndef SIREN_MarleyCrossSection_H
#define SIREN_MarleyCrossSection_H

#include <memory>                                 // for shared_ptr
#include <string>                                 // for string
#include <vector>                                 // for vector
#include <cstdint>                                // for uint32_t

#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/base_class.hpp>
#include <cereal/types/utility.hpp>

#include "SIREN/dataclasses/Particle.h"  // for Particle
#include "SIREN/dataclasses/InteractionSignature.h" // for InteractionSignature
#include "SIREN/utilities/Random.h" // for SIREN_random
#include "SIREN/interactions/Interaction.h" // for Interaction
#include "SIREN/interactions/CrossSection.h"

namespace siren { namespace dataclasses { class InteractionRecord; } }
namespace siren { namespace dataclasses { class CrossSectionDistributionRecord; } }
namespace siren { namespace dataclasses { struct InteractionSignature; } }
namespace siren { namespace utilities { class SIREN_random; } }

namespace siren {
namespace interactions {

class MarleyCrossSection : public CrossSection {
//friend cereal::access;
private:
    void SampleFinalState(dataclasses::InteractionRecord &, std::shared_ptr<siren::utilities::SIREN_random>) const;
    marley::Generator marley_generator_;  // Objeto generador de MARLEY
    void InitializeMarley(const std::string& marley_config);  // Función para configurar MARLEY

public:
    MarleyCrossSection(const std::string& marley_config);
    virtual ~MarleyCrossSection() {};
    bool operator==(MarleyCrossSection const & other) const;
    virtual bool equal(MarleyCrossSection const & other) const = 0;
    virtual double TotalCrossSection(dataclasses::InteractionRecord const &) const override;
    virtual double TotalCrossSectionAllFinalStates(dataclasses::InteractionRecord const &) const = 0;
    virtual double DifferentialCrossSection(dataclasses::InteractionRecord const &) const = 0;
    virtual double InteractionThreshold(dataclasses::InteractionRecord const &) const = 0;
    virtual void SampleFinalState(dataclasses::CrossSectionDistributionRecord &, std::shared_ptr<siren::utilities::SIREN_random>) const = 0;

    virtual std::vector<siren::dataclasses::ParticleType> GetPossibleTargets() const = 0;
    virtual std::vector<siren::dataclasses::ParticleType> GetPossibleTargetsFromPrimary(siren::dataclasses::ParticleType primary_type) const = 0;
    virtual std::vector<siren::dataclasses::ParticleType> GetPossiblePrimaries() const = 0;
    virtual std::vector<dataclasses::InteractionSignature> GetPossibleSignatures() const = 0;

    virtual std::vector<dataclasses::InteractionSignature> GetPossibleSignaturesFromParents(siren::dataclasses::ParticleType primary_type, siren::dataclasses::ParticleType target_type) const = 0;
    virtual double FinalStateProbability(dataclasses::InteractionRecord const & record) const = 0;
    virtual std::vector<std::string> DensityVariables() const = 0;
    template<class Archive>
    void save(Archive & archive, std::uint32_t const version) const {};
    template<class Archive>
    void load(Archive & archive, std::uint32_t const version) {};
}; // class MarleyCrossSection

} // namespace interactions
} // namespace siren

CEREAL_CLASS_VERSION(siren::interactions::MarleyCrossSection, 0);
CEREAL_REGISTER_TYPE(siren::interactions::MarleyCrossSection);
CEREAL_REGISTER_POLYMORPHIC_RELATION(siren::interactions::Interaction, siren::interactions::MarleyCrossSection);

#endif // SIREN_MarleyCrossSection_H
