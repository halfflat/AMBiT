#include "Include.h"
#include "ConfigGenerator.h"
#include "HartreeFock/ConfigurationParser.h"

ConfigGenerator::ConfigGenerator(pOrbitalManagerConst orbitals, MultirunOptions& userInput):
    orbitals(orbitals), user_input(userInput)
{
    NonRelSet.clear();
    leading_configs = pConfigList(new ConfigList());

    pOrbitalMapConst valence = orbitals->GetOrbitalMap(OrbitalClassification::valence);
    auto it = valence->begin();
    while(it != valence->end())
    {
        pOrbitalConst ds = it->second;
        if(ds != NULL)
        {
            if(ds->Kappa() < 0)
                NonRelSet.insert(NonRelInfo(ds->PQN(), ds->L()));
        }
        it++;
    }
}

pConfigList ConfigGenerator::GetLeadingConfigs()
{   return leading_configs;
}

pConfigListConst ConfigGenerator::GetLeadingConfigs() const
{   return leading_configs;
}

pConfigList ConfigGenerator::GenerateNonRelConfigurations(pHFOperator one_body, pHartreeY two_body)
{
    pConfigList nrlist = std::make_shared<ConfigList>();

    int electron_excitations = 0;
    int hole_excitations = 0;
    int num_electron_excitation_inputs = user_input.vector_variable_size("CI/ElectronExcitations");
    int num_hole_excitation_inputs = user_input.vector_variable_size("CI/HoleExcitations");
    
    // Default if no input detected is 2 electron excitations.
    // If input is detected it can either be a number, which would set electron_excitations and use ValenceBasis to determine states to excite to,
    // otherwise if input is of the form 'X, Y, ...' where X is the pqn and Y is the string with the basis (eg. 8spdf) then the code will
    // allow X excitations to the states in Y
    if(num_electron_excitation_inputs < 1)
    {
        num_electron_excitation_inputs = 1;
        electron_excitations = 2;
    }
    else if(num_electron_excitation_inputs == 1 && user_input("CI/ElectronExcitations", 2) >= 0)
    {
        electron_excitations = user_input("CI/ElectronExcitations", 2);
    }
    else if(num_electron_excitation_inputs%2 == 0)  // Input should come in pairs
    {
        electron_excitations = num_electron_excitation_inputs/2;
    }
    else
    {
        *errstream << "USAGE: CI/ElectronExcitations incorrectly specified." << std::endl;
        exit(1);
    }
    
    // Holes have default 0 excitations.
    if(num_hole_excitation_inputs < 1)
    {
        num_hole_excitation_inputs = 1;
        hole_excitations = 0;
    }
    else if(num_hole_excitation_inputs == 1 && user_input("CI/HoleExcitations", 0) >= 0)
    {
        hole_excitations = user_input("CI/HoleExcitations", 0);
    }
    else if(num_hole_excitation_inputs%2 == 0)  // Input should come in pairs
    {
        hole_excitations = num_hole_excitation_inputs/2;
    }
    else
    {
        *errstream << "USAGE: CI/HoleExcitations incorrectly specified." << std::endl;
        exit(1);
    }
    
    // Get leading configurations
    leading_configs->clear();
    int numValenceElectrons = 0;
    
    int num_configs = user_input.vector_variable_size("CI/LeadingConfigurations");
    for(int i = 0; i < num_configs; i++)
    {
        const std::string name = user_input("CI/LeadingConfigurations", "", i);
        NonRelConfiguration config(name);
        
        // Check that the configuration gels with the number of electrons
        if(i == 0)
            numValenceElectrons = config.ElectronNumber();
        else if(config.ElectronNumber() != numValenceElectrons)
        {   *errstream << "USAGE: LeadingConfiguration " << name
            << " does not have correct number of valence electrons." << std::endl;
            exit(1);
        }
        leading_configs->add(config);
    }

    if(numValenceElectrons == 0)
    {   // Add vacuum state: no holes or electrons
        leading_configs->add(NonRelConfiguration());
    }

    nrlist->merge(*leading_configs);
    
    // Total number of excitation steps
    int total_num_excitations = mmax(electron_excitations, hole_excitations);
    
    for(int excitation_step = 0; excitation_step < total_num_excitations; excitation_step++)
    {
        NonRelInfoSet valence_electrons;
        NonRelInfoSet valence_holes;
        
        // Electron subset
        if(excitation_step < electron_excitations)
        {
            for(const auto& pair: *orbitals->particle)
                valence_electrons.insert(NonRelInfo(pair.first));
            
            if(num_electron_excitation_inputs > 1)
            {
                std::string CI_basis_string = user_input("CI/ElectronExcitations", "", 2*excitation_step + 1);
                std::vector<int> limits = ConfigurationParser::ParseBasisSize(CI_basis_string);
                
                auto valence_it = valence_electrons.begin();
                while(valence_it != valence_electrons.end())
                {
                    int L = valence_it->L();
                    
                    if(L >= limits.size())
                        valence_it = valence_electrons.erase(valence_it);
                    else if(valence_it->PQN() > limits[L])
                        valence_it = valence_electrons.erase(valence_it);
                    else
                        valence_it++;
                }
            }
        }
        
        // Holes subset
        if(excitation_step < hole_excitations)
        {
            for(const auto& pair: *orbitals->hole)
                valence_holes.insert(NonRelInfo(pair.first));
            
            if(num_hole_excitation_inputs > 1)
            {
                std::string CI_basis_string = user_input("CI/HoleExcitations", "", 2*excitation_step + 1);
                std::vector<int> limits = ConfigurationParser::ParseBasisSize(CI_basis_string);
                
                auto valence_it = valence_holes.begin();
                while(valence_it != valence_holes.end())
                {
                    int L = valence_it->L();
                    
                    if(L >= limits.size())
                        valence_it = valence_holes.erase(valence_it);
                    else if(valence_it->PQN() > limits[L])
                        valence_it = valence_holes.erase(valence_it);
                    else
                        valence_it++;
                }
            }
        }
        
        GenerateExcitations(nrlist, valence_electrons, valence_holes);
    }
    
    // Add extra configurations not to be considered leading configurations.
    int num_extra_configs = user_input.vector_variable_size("CI/ExtraConfigurations");
    for(int i = 0; i < num_extra_configs; i++)
    {
        const std::string extraname = user_input("CI/ExtraConfigurations", "", i);
        NonRelConfiguration extraconfig(extraname);
        
        if(extraconfig.ElectronNumber() != numValenceElectrons)
        {
            *errstream << "USAGE: ExtraConfiguration " << extraname
            << " does not have correct number of valence electrons." << std::endl;
            exit(1);
        }
        nrlist->add(extraconfig);
    }

    nrlist->unique();
    return nrlist;
}

pRelativisticConfigList ConfigGenerator::GenerateRelativisticConfigurations(pConfigList nrlist) const
{
    pRelativisticConfigList rlist = std::make_shared<RelativisticConfigList>();
    ConfigList::iterator it = nrlist->begin();
    while(it != nrlist->end())
    {
        rlist->append(*it->GenerateRelativisticConfigs());
        it++;
    }
    
    rlist->sort(FewestProjectionsFirstComparator());
    rlist->unique();
    return rlist;
}

pRelativisticConfigList ConfigGenerator::GenerateRelativisticConfigurations(pConfigList nrlist, const Symmetry& sym, bool generate_projections) const
{
    pRelativisticConfigList rlist = std::make_shared<RelativisticConfigList>();
    ConfigList::iterator it = nrlist->begin();
    while(it != nrlist->end())
    {
        // Add states of correct parity
        if(it->GetParity() == sym.GetParity() && sym.GetTwoJ() <= it->GetTwiceMaxProjection())
            rlist->append(*it->GenerateRelativisticConfigs());

        it++;
    }
    
    if(generate_projections)
        GenerateProjections(rlist, sym.GetTwoJ());
    
    return rlist;
}

pRelativisticConfigList ConfigGenerator::GenerateRelativisticConfigurations(const Symmetry& sym, bool generate_projections)
{
    pConfigList nrlist = GenerateNonRelConfigurations();

    // Remove states of wrong parity
    ConfigList::iterator it = nrlist->begin();

    while(it != nrlist->end())
    {
        if(it->GetParity() != sym.GetParity())
        {   it = nrlist->erase(it);
        }
        else
            it++;
    }

    nrlist->unique();

    pRelativisticConfigList rlist(GenerateRelativisticConfigurations(nrlist));

    if(generate_projections)
        GenerateProjections(rlist, sym.GetTwoJ());

    return rlist;
}

void ConfigGenerator::GenerateExcitations(pConfigList configlist, const NonRelInfoSet& electron_valence, const NonRelInfoSet& hole_valence) const
{
    ConfigList old_list(*configlist);

    // Go through the set of initial configurations
    ConfigList::const_iterator config_it = old_list.begin();
    while(config_it != old_list.end())
    {
        // Move electrons and holes:
        // For each single particle state in the configuration
        NonRelConfiguration::const_iterator particle_it = config_it->begin();
        while(particle_it != config_it->end())
        {
            // Electron or hole?
            if(particle_it->second > 0)
            {
                // Get another single particle state to move to
                for(const auto& electron: electron_valence)
                {
                    if(electron != particle_it->first)
                    {
                        NonRelConfiguration new_config(*config_it);
                        new_config.RemoveSingleParticle(particle_it->first);
                        if(new_config.AddSingleParticle(electron))
                            configlist->add(new_config);
                    }
                }
            }
            else
            {   // Get another single particle state to move to
                for(const auto& hole: hole_valence)
                {
                    if(hole != particle_it->first)
                    {
                        NonRelConfiguration new_config(*config_it);
                        new_config.AddSingleParticle(particle_it->first);
                        if(new_config.RemoveSingleParticle(hole))
                            configlist->add(new_config);
                    }
                }
            }

            particle_it++;
        }

        // Pair creation
        for(const auto& electron: electron_valence)
        {
            NonRelConfiguration config_with_extra_electron(*config_it);
            if(config_with_extra_electron.AddSingleParticle(electron))
            {
                for(const auto& hole: hole_valence)
                {
                    NonRelConfiguration new_config(config_with_extra_electron);
                    if(new_config.RemoveSingleParticle(hole))
                        configlist->add(new_config);
                }
            }
        }

        // Pair annihilation
        particle_it = config_it->begin();
        while(particle_it != config_it->end())
        {
            auto other_particle_it = particle_it;
            other_particle_it++;

            while(other_particle_it != config_it->end())
            {
                if((particle_it->second < 0) && (other_particle_it->second > 0))
                {
                    NonRelConfiguration new_config(*config_it);
                    new_config.AddSingleParticle(particle_it->first);
                    new_config.RemoveSingleParticle(other_particle_it->first);
                    configlist->add(new_config);
                }
                else if((particle_it->second > 0) && (other_particle_it->second < 0))
                {
                    NonRelConfiguration new_config(*config_it);
                    new_config.RemoveSingleParticle(particle_it->first);
                    new_config.AddSingleParticle(other_particle_it->first);
                    configlist->add(new_config);
                }

                other_particle_it++;
            }

            particle_it++;
        }

        config_it++;
    }

    configlist->unique();
}

void ConfigGenerator::GenerateProjections(pRelativisticConfigList rlist, int two_m, int two_j) const
{
    if(rlist == nullptr || rlist->size() == 0)
        return;

    Parity parity = rlist->front().GetParity();
    int electron_number = rlist->front().ElectronNumber();

    std::string angular_directory = string_macro(ANGULAR_DATA_DIRECTORY);
    if(user_input.search("AngularDataDirectory"))
        angular_directory = user_input("AngularDataDirectory", "");

    // Get correct library
    pAngularDataLibrary angular_library(new AngularDataLibrary(electron_number, Symmetry(two_j, parity), two_m, angular_directory));
    angular_library->Read();

    auto it = rlist->begin();
    while(it != rlist->end())
    {
        if(it->GetProjections(angular_library))
            it++;
        else
            it = rlist->erase(it);
    }

    angular_library->GenerateCSFs();

    // Write even if there are no CSFs for a given J since this is not so obvious
    angular_library->Write();

    // Remove from list if there are no CSFs for a particular RelativisticConfiguration.
    it = rlist->begin();
    while(it != rlist->end())
    {
        if(it->NumCSFs())
            it++;
        else
            it = rlist->erase(it);
    }
}

void ConfigGenerator::GenerateProjections(pConfigList nrlist, const Symmetry& sym, int two_m) const
{
    if(nrlist == nullptr || nrlist->size() == 0)
        return;

    int electron_number = nrlist->front().ElectronNumber();

    std::string angular_directory = string_macro(ANGULAR_DATA_DIRECTORY);
    if(user_input.search("AngularDataDirectory"))
        angular_directory = user_input("AngularDataDirectory", "");

    // Get correct library
    pAngularDataLibrary angular_library(new AngularDataLibrary(electron_number, sym, two_m, angular_directory));
    angular_library->Read();

    for(auto& nrconfig: *nrlist)
    {
        // Clear relconfiglist if symmetry is incorrect
        if(nrconfig.GetParity() != sym.GetParity() ||
           nrconfig.GetTwiceMaxProjection() < sym.GetTwoJ())
            nrconfig.relconfiglist = nullptr;
        else
        {
            nrconfig.GenerateRelativisticConfigs();

            auto it = nrconfig.relconfiglist->begin();
            while(it != nrconfig.relconfiglist->end())
            {
                if(it->GetTwiceMaxProjection() < sym.GetTwoJ())
                    it = nrconfig.relconfiglist->erase(it);
                else if(it->GetProjections(angular_library))
                    it++;
                else
                    it = nrconfig.relconfiglist->erase(it);
            }
        }
    }

    angular_library->GenerateCSFs();

    // Write even if there are no CSFs for a given J since this is not so obvious
    angular_library->Write();

    // Remove from list if there are no CSFs for a particular RelativisticConfiguration.
    for(auto& nrconfig: *nrlist)
    {
        auto it = nrconfig.relconfiglist->begin();
        while(it != nrconfig.relconfiglist->end())
        {
            if(it->NumCSFs())
                it++;
            else
                it = nrconfig.relconfiglist->erase(it);
        }

        if(nrconfig.relconfiglist->size() == 0)
            nrconfig.relconfiglist = nullptr;
    }
}
