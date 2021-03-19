#include "portfolio.h"

#include "containers/string.h"
#include "containers/darray.h"

Persona persona_make()
{
    DArray(String) abilities;
    da_make(abilities);

    DArray(String) projects;
    da_make(projects);

    return (Persona){ 0, 0, abilities, 0, projects };
}

Portfolio portfolio_make()
{
    DArray(Persona) personas;
    da_make(personas);
    return (Portfolio){ 0, 0, 0, personas };
}