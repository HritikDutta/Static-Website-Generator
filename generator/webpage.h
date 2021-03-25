#pragma once

#include "portfolio.h"
#include "containers\string.h"
#include "containers\darray.h"

typedef enum
{
    STAGE_NONE,
    STAGE_HTML,
    STAGE_PROPERTY,
    STAGE_LIST,
    STAGE_CONDITIONAL
} Stage_Type;

typedef struct Stage
{
    Stage_Type type;
    
    union
    {
        struct
        {
            String content;
        } html;

        struct
        {
            String name;
            int parent_index;
        } property;

        struct
        {
            String it_name;
            int parent_index;
            DArray(struct Stage) stages;
        } list;

        struct
        {
            DArray(struct Stage) condition;
            DArray(struct Stage) stages_if_true;
            DArray(struct Stage) stages_if_false;
        } conditional;
    };
} Stage;

Stage html_stage_make();
Stage prop_stage_make();
Stage list_stage_make();
Stage cond_stage_make();
void stage_free(Stage* stage);

typedef enum
{
    TP_NO_PARSE,
    TP_FAILURE,
    TP_SUCCESS
} Template_Parser_Status;

typedef struct
{
    String content;
    int cur_index;
    DArray(Stage) stages;
    Template_Parser_Status status;
    String message;
} Template_Parser;

Template_Parser template_parser_make(String template);
void template_parser_free(Template_Parser* tp);
void template_parser_parse(Template_Parser* tp);

typedef enum
{
    WP_MISSING_TEMPLATE,
    WP_TEMPLATE_ERROR,
    WP_WRITE_ERROR,
    WP_SUCCESS
} Webpage_Status;

typedef enum
{
    GEN_NO_GEN,
    GEN_FAILURE,
    GEN_SUCCESS
} Generator_Status;

typedef struct
{
    DArray(Stage) stages;
    int cur_index;
    DArray(char) buffer;
    Generator_Status status;
    String message;
} Generator;

Generator generator_make();
void generator_free(Generator* generator);
void generate_persona_page(Generator* generator);
String generator_output(Generator* generator);

typedef enum
{
    WV_PROP,
    WV_PERSONA,
    WV_PROJECT,
    WV_PROJECT_LIST,
    WV_STRING_LIST,
} Webpage_Variable_Type;

typedef struct
{
    Webpage_Variable_Type type;
    union
    {
        struct
        {
            String value;
        } prop;

        struct
        {
            int selected;
            Persona data;
        } persona;

        struct
        {
            Project data;
        } project;

        struct
        {
            DArray(Project) list;
        } proj_list;

        struct
        {
            DArray(String) list;
        } string_list;
    };
    
} Webpage_Variable;

Webpage_Variable wv_make_prop(String value);
Webpage_Variable wv_make_persona(Persona data, int selected);
Webpage_Variable wv_make_project(Project data);
Webpage_Variable wv_make_proj_list(DArray(Project) data);
Webpage_Variable wv_make_slist(DArray(String) slist);
void wv_free(Webpage_Variable* var);

Webpage_Status template_parser_test(Portfolio portfolio);
Webpage_Status generate_webpages(String template, Portfolio portfolio);