#include "SpinorODE.h"

SpinorODE::SpinorODE(pLattice lattice): LatticeObserver(lattice), include_nonlocal(true)
{}

SpinorODE::SpinorODE(const SpinorODE& other): LatticeObserver(other.lattice), include_nonlocal(other.include_nonlocal)
{}

void SpinorODE::IncludeExchange(bool include_exchange)
{
    include_nonlocal = include_exchange;
}

bool SpinorODE::IncludeExchange() const
{
    return include_nonlocal;
}

void SpinorODE::GetDerivative(Orbital& fg)
{
    SetODEParameters(fg);

    double w[2];
    for(unsigned int i = 0; i < fg.size(); i++)
    {
        GetODEFunction(i, fg, w);
        fg.dfdr[i] = w[0];
        fg.dgdr[i] = w[1];
    }
}
