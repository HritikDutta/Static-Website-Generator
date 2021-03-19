#pragma once

#include "containers/string.h"
#include "containers/darray.h"

typedef struct
{
    String name;
    String date;
    DArray(String) skills;
    String description;
} Project;

Project project_make();
void project_free(Project* project);

typedef struct
{
    String name;
    String color;
    DArray(String) abilities;
    String blerb;
    DArray(String) projects;
} Persona;

Persona persona_make();
void persona_free(Persona* persona);

typedef struct
{
    String home_template;
    String page_template;
    String outdir;
    DArray(Persona) peronas;
} Portfolio;

Portfolio portfolio_make();
void portfolio_free(Portfolio* portfolio);