#ifndef HAMILTONIAN_TYPES_H
#define HAMILTONIAN_TYPES_H

#include "Configuration/Level.h"

/** SingleOrbitalID defines a Hamiltonian of size 1:
    just the one configuration from one orbital.
    They are distinguishable by PQN().
 */
class SingleOrbitalID : public HamiltonianID
{
public:
    SingleOrbitalID(): HamiltonianID(), pqn(0) {}
    SingleOrbitalID(int kappa, int pqn): HamiltonianID(kappa), pqn(pqn) {}
    SingleOrbitalID(const OrbitalInfo& info): HamiltonianID(info.Kappa()), pqn(info.PQN()) {}
    SingleOrbitalID(const SingleOrbitalID& other): HamiltonianID(other), pqn(other.pqn) {}

    virtual bool operator<(const SingleOrbitalID& other) const
    {
        if(sym < other.sym)
            return true;
        else if (other.sym < sym)
            return false;
        else
            return pqn < other.pqn;
    }

    virtual bool operator>(const SingleOrbitalID& other) const
    {   return (other < *this);
    }
    
    virtual bool operator==(const SingleOrbitalID& other) const
    {   return (sym == other.sym) && (pqn == other.pqn);
    }

    int GetPQN() const { return pqn; }
    OrbitalInfo GetOrbitalInfo() const { return OrbitalInfo(pqn, sym.GetKappa()); }

    virtual std::string Name() const override
    {   return GetOrbitalInfo().Name();
    }

    virtual std::string Print() const override
    {   return GetOrbitalInfo().Name();
    }

    virtual void Read(FILE* fp) override
    {
        HamiltonianID::Read(fp);
        pqn = configs->front().begin()->first.PQN();
    }
    
    virtual pHamiltonianID Clone() const
    {   return std::make_shared<SingleOrbitalID>(*this);
    }

protected:
    int pqn;
};

/** NonRelID defines a Hamiltonian generated from RelativisticConfigurations drawn from a single
    NonRelConfiguration.
 */
class NonRelID : public HamiltonianID
{
public:
    NonRelID(): HamiltonianID() {}
    NonRelID(const NonRelConfiguration& config, int two_j, pRelativisticConfigList rconfigs = nullptr):
        HamiltonianID(two_j, config.GetParity(), rconfigs), nrconfig(config) {}
    NonRelID(const NonRelID& other): HamiltonianID(other), nrconfig(other.nrconfig) {}

    virtual bool operator<(const NonRelID& other) const
    {
        if(sym < other.sym)
            return true;
        else if(other.sym < sym)
            return false;
        else
            return nrconfig < other.nrconfig;
    }

    virtual bool operator>(const NonRelID& other) const
    {   return (other < *this);
    }

    virtual bool operator==(const NonRelID& other) const
    {   return (sym == other.sym) && (nrconfig == other.nrconfig);
    }

    NonRelConfiguration& GetNonRelConfiguration() { return nrconfig; }
    const NonRelConfiguration& GetNonRelConfiguration() const { return nrconfig; }

    virtual std::string Name() const override
    {   return nrconfig.Name() + "." + HamiltonianID::Name();
    }

    virtual std::string Print() const override
    {   return nrconfig.Name() + " " + HamiltonianID::Print();
    }

    virtual void Read(FILE* fp) override
    {   HamiltonianID::Read(fp);
        nrconfig = NonRelConfiguration(configs->front());
    }

    virtual pHamiltonianID Clone() const
    {   return std::make_shared<NonRelID>(*this);
    }

protected:
    NonRelConfiguration nrconfig;
};

#endif
