#include "portfolio.h"

#include "containers/string.h"
#include "containers/darray.h"

Project project_make()
{
    Project p = { 0 };

    da_make(p.skills);
    da_make(p.images);

    return p;
}

void project_free(Project* project)
{
    if (project->name)
        string_free(&project->name);

    if (project->date)
        string_free(&project->date);

    if (project->link)
        string_free(&project->link);

    if (project->description)
        string_free(&project->description);

    da_foreach(String, skill, project->skills)
        string_free(skill);
    da_free(project->skills);

    da_foreach(String, image, project->images)
        string_free(image);
    da_free(project->images);    
}

Persona persona_make()
{
    Persona p = { 0 };

    da_make(p.abilities);
    da_make(p.projects);

    return p;
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

    if (persona->blurb)
        string_free(&persona->blurb);

    da_foreach(String, ab, persona->abilities)
        string_free(ab);
    da_free(persona->abilities);

    da_foreach(Project, pj, persona->projects)
        project_free(pj);
    da_free(persona->projects);
}

Portfolio portfolio_make()
{
    Portfolio p = { 0 };
    da_make(p.personas);
    return p;
}

void portfolio_free(Portfolio* portfolio)
{
    if (portfolio->home_template)
        string_free(&portfolio->home_template);

    if (portfolio->page_template)
        string_free(&portfolio->page_template);

    if (portfolio->outdir)
        string_free(&portfolio->outdir);

    da_foreach(Persona, persona, portfolio->personas)
        persona_free(persona);
    da_free(portfolio->personas);
}