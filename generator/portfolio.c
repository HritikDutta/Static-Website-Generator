#include "portfolio.h"

#include "containers/string.h"
#include "containers/darray.h"

Project project_make()
{
    DArray(String) skills;
    da_make(skills);
    return (Project) { 0, 0, skills, 0 };
}

void project_free(Project* project)
{
    if (project->name)
        string_free(&project->name);

    if (project->date)
        string_free(&project->date);

    if (project->description)
        string_free(&project->description);

    da_foreach(String, skill, project->skills)
        string_free(skill);
    da_free(project->skills);        
}

Persona persona_make()
{
    DArray(String) abilities;
    da_make(abilities);

    DArray(Project) projects;
    da_make(projects);

    return (Persona){ 0, 0, 0, 0, abilities, 0, projects };
}

void persona_free(Persona* persona)
{
    if (persona->name)
        string_free(&persona->name);

    if (persona->color)
        string_free(&persona->color);

    if (persona->image)
        string_free(&persona->image);

    if (persona->icon)
        string_free(&persona->icon);

    if (persona->blerb)
        string_free(&persona->blerb);

    da_foreach(String, ab, persona->abilities)
        string_free(ab);
    da_free(persona->abilities);

    da_foreach(Project, pj, persona->projects)
        project_free(pj);
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