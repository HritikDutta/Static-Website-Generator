#pragma once

#include "portfolio.h"
#include "containers/string.h"
#include "containers/darray.h"
#include "containers/dictionary.h"

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

Webpage_Status template_parser_test(Portfolio portfolio);
Webpage_Status generate_webpages(Portfolio portfolio);

typedef enum
{
    VAR_NONE,
    VAR_BOOL,
    VAR_PERSONA,
    VAR_LINK,
    VAR_PROJECT,
    VAR_STRING,
    VAR_STRING_LIST,
    VAR_LINK_LIST,
    VAR_PROJECT_LIST,
    VAR_PERSONA_LIST,
} Variable_Type;

typedef struct
{
    Variable_Type type;
    union
    {
        struct {
            int value;
        } bool;

        struct {
            int selected;
            Persona data;
        } persona;

        struct {
            Link data;
        } link;

        struct {
            Project data;
        } project;

        struct {
            String data;
        } string;

        struct {
            DArray(String) list;
        } string_list;

        struct {
            DArray(Link) list;
        } link_list;

        struct {
            DArray(Project) list;
        } project_list;
        
        struct {
            DArray(Persona) list;
        } persona_list;
    };
} Variable;

Variable var_make_bool(int value);
Variable var_make_link(Link link);
Variable var_make_project(Project data);
Variable var_make_persona(Persona data, int selected);
Variable var_make_string(String data);
Variable var_make_string_list(DArray(String) list);
Variable var_make_link_list(DArray(Link) list);
Variable var_make_project_list(DArray(Project) list);
Variable var_make_persona_list(DArray(Persona) list);

typedef enum
{
    GEN_NO_GEN,
    GEN_FAILURE,
    GEN_SUCCESS
} Generator_Status;

typedef struct
{
    Dict(Variable) vs;
    DArray(Stage) stages;
    int cur_index;
    DArray(char) buffer;
    Generator_Status status;
    String message;
} Generator;

Generator generator_make(DArray(Stage) stages);
void generator_free(Generator* generator);
void generator_reset(Generator* generator);
void generate_page(Generator* generator, Portfolio portfolio, int selected_index);
String generator_output(Generator generator);