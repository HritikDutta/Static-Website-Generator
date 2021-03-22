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
        } property;

        struct
        {
            String list_name;
            String it_name;
            DArray(struct Stage) stages;
        } list;

        struct
        {
            String condition;
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
Webpage_Status generate_webpages(String template, Portfolio portfolio);