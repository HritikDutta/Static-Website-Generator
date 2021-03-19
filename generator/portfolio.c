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

void persona_free(Persona* persona)
{
    if (persona->name)
        string_free(&persona->name);

    if (persona->color)
        string_free(&persona->color);

    if (persona->blerb)
        string_free(&persona->blerb);

    da_foreach(String, ab, persona->abilities)
        string_free(ab);
    da_free(persona->abilities);

    da_foreach(String, pj, persona->projects)
        string_free(pj);
    da_free(persona->projects);
}

Portfolio portfolio_make()
{
    DArray(Persona) personas;
    da_make(personas);
    return (Portfolio){ 0, 0, 0, personas };
}

void portfolio_free(Portfolio* portfolio)
{
    if (portfolio->home_template)
        string_free(&portfolio->home_template);

    if (portfolio->page_template)
        string_free(&portfolio->page_template);

    if (portfolio->outdir)
        string_free(&portfolio->outdir);

    da_foreach(Persona, persona, portfolio->peronas)
        persona_free(persona);
    da_free(portfolio->peronas);
}