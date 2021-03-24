#include "webpage.h"

#include <stdio.h>
#include "filestuff.h"
#include "containers/hd_assert.h"
#include "containers/darray.h"
#include "containers/dictionary.h"

Stage html_stage_make()
{
    Stage s;
    s.type = STAGE_HTML;
    s.html.content = NULL;
    return s;
}

Stage prop_stage_make()
{
    Stage s;
    s.type = STAGE_PROPERTY;
    s.property.name   = NULL;
    s.property.parent = NULL;
    return s;
}

Stage list_stage_make()
{
    Stage s;
    s.type = STAGE_LIST;
    s.list.it_name   = NULL;
    s.list.stages    = NULL;
    da_make(s.list.stages);
    return s;
}

Stage cond_stage_make()
{
    Stage s;
    s.type = STAGE_CONDITIONAL;
    s.conditional.condition       = NULL;
    s.conditional.stages_if_true  = NULL;
    s.conditional.stages_if_false = NULL;
    da_make(s.conditional.condition);
    da_make(s.conditional.stages_if_true);
    da_make(s.conditional.stages_if_false);
    return s;
}

void stage_free(Stage* stage)
{
    switch (stage->type)
    {
        case STAGE_HTML:
        {
            if (stage->html.content)
                string_free(&stage->html.content);
        } break;

        case STAGE_PROPERTY:
        {
            if (stage->property.name)
                string_free(&stage->property.name);

            if (stage->property.parent)
                string_free(&stage->property.parent);
        } break;

        case STAGE_LIST:
        {
            if (stage->list.it_name)
                string_free(&stage->list.it_name);

            da_foreach(Stage, s, stage->list.stages)
                stage_free(s);
            da_free(stage->list.stages);
        } break;

        case STAGE_CONDITIONAL:
        {
            da_foreach(Stage, s, stage->conditional.condition)
                stage_free(s);
            da_free(stage->conditional.condition);

            da_foreach(Stage, s, stage->conditional.stages_if_true)
                stage_free(s);
            da_free(stage->conditional.stages_if_true);

            da_foreach(Stage, s, stage->conditional.stages_if_false)
                stage_free(s);
            da_free(stage->conditional.stages_if_false);
        } break;
    }
}

Template_Parser template_parser_make(String template)
{
    DArray(Stage) stages = NULL;
    da_make(stages);
    return (Template_Parser){ template, 0, stages, TP_NO_PARSE, NULL };
}

void template_parser_free(Template_Parser* tp)
{
    if (tp->content)
        string_free(&tp->content);

    if (tp->message)
        string_free(&tp->message);

    da_foreach(Stage, stage, tp->stages)
        stage_free(stage);
    da_free(tp->stages);
}

static int is_alpha_or_us(char ch)
{
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           ch == '_';
}

static int is_ws(char ch)
{
    return ch == ' '  ||
           ch == '\t' ||
           ch == '\r' ||
           ch == '\n';
}

static char peek(Template_Parser* tp, int offset)
{
    return tp->content[tp->cur_index + offset];
}

static char consume(Template_Parser* tp)
{
    return tp->content[tp->cur_index++];
}

static void consume_ws(Template_Parser* tp)
{
    while (is_ws(peek(tp, 0)))
        consume(tp);
}

#define TP_ERROR(tp, m) \
    do {                                                \
        tp->status = TP_FAILURE;                        \
        tp->message = string_make("Template Error: "m); \
    } while (0)

static String get_identifier(Template_Parser* tp)
{
    consume_ws(tp);

    int start_idx = tp->cur_index;
    while (is_alpha_or_us(peek(tp, 0)))
        consume(tp);

    if (tp->cur_index <= start_idx)
        return NULL;

    return string_make_till_n(tp->content + start_idx, tp->cur_index - start_idx);
}

static void fill_stages(Template_Parser* tp, char end, DArray(Stage)* stages);

// Starts parsing after ->
static Stage get_list(Template_Parser* tp)
{
    Stage list = list_stage_make();

    String identifier = get_identifier(tp);
    if (!identifier)
    {
        TP_ERROR(tp, "Expected identifer after -> in list tag");
        return list;
    }

    consume_ws(tp);

    if (peek(tp, 0) != '{')
    {
        TP_ERROR(tp, "Expected template block (enclosed with {}) in list tag");
        return list;
    }

    list.list.it_name = identifier;
    fill_stages(tp, '}', &list.list.stages);

    if (peek(tp, 0) != '}')
        TP_ERROR(tp, "Template block must be closed with }");
    else
        consume(tp);

    return list;
}

static Stage get_cond(Template_Parser* tp)
{
    Stage cond = cond_stage_make();

    fill_stages(tp, '{', &cond.conditional.condition);
    
    if (tp->status == TP_FAILURE)
        return cond;

    if (da_size(cond.conditional.condition) == 0)
    {
        TP_ERROR(tp, "Expected condition inside if tag");
        return cond;
    }

    if (peek(tp, 0) != '{')
    {
        TP_ERROR(tp, "Expected template block (enclosed with {}) in if tag");
        return cond;
    }

    consume(tp);
    fill_stages(tp, '}', &cond.conditional.stages_if_true);
    
    if (peek(tp, 0) != '}')
        TP_ERROR(tp, "Template block must be closed with }");
    else
        consume(tp);

    if (tp->status == TP_FAILURE)
        return cond;

    String else_tag = get_identifier(tp);
    if (else_tag)
    {
        string_free(&else_tag);
        consume_ws(tp);
        
        if (peek(tp, 0) != '{')
        {
            TP_ERROR(tp, "Expected template block (enclosed with {}) after else tag");
            return cond;
        }

        consume(tp);
        fill_stages(tp, '}', &cond.conditional.stages_if_true);

        if (peek(tp, 0) != '}')
            TP_ERROR(tp, "Template block must be closed with }");
        else
            consume(tp);

        if (tp->status == TP_FAILURE)
            return cond;
    }

    if (peek(tp, 0) != '>')
        TP_ERROR(tp, "Expected > at the end of if tag");

    return cond;
}

// tp->cur_index is stopped at the first instance of end char
static void fill_stages(Template_Parser* tp, char end, DArray(Stage)* stages)
{
    int is_in_$tag = end == '{';
    int tokens_in_$tag = 0;

    int len = string_length(tp->content);
    while (tp->status != TP_FAILURE && tp->cur_index < len)
    {
        consume_ws(tp);

        if (peek(tp, 0) == end)
            break;

        if (!is_in_$tag)
        {
            // Collect html till <$ is found
            int start_idx = tp->cur_index;
            while (tp->cur_index < len &&
                   peek(tp, 0) != end)
            {
                if (peek(tp, 0) == '<' && peek(tp, 1) == '$')
                {
                    is_in_$tag = 1;
                    consume(tp);
                    consume(tp);
                    break;
                }

                consume(tp);
            }

            if (tp->cur_index > start_idx)
            {
                Stage html = html_stage_make();
                int length = tp->cur_index - start_idx - (is_in_$tag * 2);
                html.html.content = string_make_till_n(tp->content + start_idx, length);
                da_push_back((*stages), html);
            }

            continue;
        }

        consume_ws(tp);

        if (peek(tp, 0) == '-' && peek(tp, 1) == '>')
        {
            consume(tp); consume(tp);

            if (tokens_in_$tag == 0)
            {
                TP_ERROR(tp, "-> can only be used after a property");
                continue;
            }

            Stage list = get_list(tp);
            da_push_back((*stages), list);
            continue;            
        }

        if (peek(tp, 0) == '>')
        {
            consume(tp);

            if (tokens_in_$tag == 0)
            {
                TP_ERROR(tp, "$ tag cannot be empty");
                continue;
            }

            is_in_$tag = 0;
            tokens_in_$tag = 0;
            continue;
        }

        if (peek(tp, 0) == 'i' && peek(tp, 1) == 'f')
        {
            consume(tp); consume(tp);
            Stage cond = get_cond(tp);
            da_push_back((*stages), cond);
            tokens_in_$tag++;
            continue;
        }

        String parent_name = NULL;

        if (peek(tp, 0) == '.')
        {
            consume(tp);

            int last_elem_idx = da_size((*stages)) - 1;
            if (tokens_in_$tag == 0 ||
                (*stages)[last_elem_idx].type != STAGE_PROPERTY)
            {
                TP_ERROR(tp, ". can only be used after a property");
                continue;
            }

            string_copy(&parent_name, (*stages)[last_elem_idx].property.name);
        }

        Stage prop = prop_stage_make();
        String prop_name = get_identifier(tp);

        prop.property.name = prop_name;
        prop.property.parent = parent_name;
        da_push_back((*stages), prop);
        
        tokens_in_$tag++;
    }
}

void template_parser_parse(Template_Parser* tp)
{
    // Just in case
    tp->cur_index = 0;
    tp->status = TP_NO_PARSE;
    fill_stages(tp, '\0', &tp->stages);

    if (tp->status != TP_FAILURE)
        tp->status = TP_SUCCESS;
}

#undef TP_ERROR

static void print_stages(DArray(Stage) stages)
{
    da_foreach(Stage, s, stages)
    {
        printf(" -> ");
        switch (s->type)
        {
            case STAGE_HTML:
            {
                printf("[ html: %s ]\n", s->html.content);
            } break;

            case STAGE_PROPERTY:
            {
                printf("[ property: %s parent: %s ]\n", s->property.name, s->property.parent);
            } break;

            case STAGE_LIST:
            {
                printf("[ list: %s [\n", s->list.it_name);
                print_stages(s->list.stages);
                printf("\n]]\n");
            } break;

            case STAGE_CONDITIONAL:
            {
                printf("[ conditional: (\n");
                print_stages(s->conditional.condition);
                printf(") true -> [\n", s->conditional.condition);
                print_stages(s->conditional.stages_if_true);
                printf(" ] false [\n");
                print_stages(s->conditional.stages_if_false);
                printf(" \n]\n");
            } break;
        }
    }
}

Webpage_Status template_parser_test(Portfolio portfolio)
{
    String page_template = load_file(portfolio.page_template);
    if (!page_template)
        return WP_MISSING_TEMPLATE;

    Template_Parser tp = template_parser_make(page_template);
    template_parser_parse(&tp);

    if (tp.status == TP_FAILURE)
        printf("%s\n", tp.message);
    else
        print_stages(tp.stages);

    template_parser_free(&tp);
    return WP_SUCCESS;
}

// static void add_to_buffer(DArray(char)* buffer, String str)
// {
//     int len = string_length(str);
//     for (int i = 0; i < len; i++)
//         da_push_back((*buffer), str[i]);
// }

// static Generation_Status fill_home(DArray(Stage) stages, DArray(Persona) personas, Dict(Webpage_Variable)* vars, DArray(char)* buffer)
// {
//     da_foreach(Stage, stage, stages)
//     {
//         switch (stage->type)
//         {
//             case STAGE_HTML:
//             {
//                 add_to_buffer(buffer, stage->html.content);
//             } break;

//             case STAGE_PROPERTY:
//             {
//                 Dict_Bkt(Webpage_Variable) var = dict_find((*vars), stage->property.name);

//                 if (var)    // Variable already exists
//                 {
//                     if (var->value.type != WV_PROP) // Variable is of wrong type
//                         return GEN_FAILURE;

//                     add_to_buffer(buffer, var->value.prop.value);
//                 }
//                 else 
//                 {
//                     // Find out if it's a valid property
//                 }
//             } break;

//             case STAGE_LIST:
//             {
//                 Dict_Bkt(Webpage_Variable) var = dict_find((*vars), stage->list.it_name);
                
//                 if (var) // Variable already declared
//                     return GEN_FAILURE;

//                 if (string_cmp(stage->list.list_name, "personas"))
//                 {
                    
//                 }

//                 if (string_cmp(stage->list.list_name, "abilities"))
//                 {

//                 }

//                 if (string_cmp(stage->list.list_name, "projects"))
//                 {

//                 }

//                 } break;
//         }
//     }

//     return GEN_SUCCESS;
// }

// static String generate_home_page(DArray(Stage) stages, DArray(Persona) personas)
// {
//     DArray(char) buffer;
//     Dict(Webpage_Variable) vars;

//     fill_home(stages, personas, &vars, &buffer);

//     String content = string_make(buffer);
//     da_free(buffer);
//     dict_free(vars);
//     return content;
// }

// static String generate_persona_page(DArray(Stage) stages, DArray(Persona) personas, int index)
// {
//     DArray(char) buffer;

//     fill_persona(stages, personas, index, buffer);

//     String content = string_make(buffer);
//     da_free(buffer);
//     return content;
// }

// Webpage_Status generate_webpages(Portfolio portfolio)
// {
//     // Home Page

//     String home_template = load_file(portfolio.home_template);
//     if (!home_template)
//         return WP_MISSING_TEMPLATE;

//     Template_Parser home_parser = template_parser_make(home_template);
//     template_parser_parse(&home_parser);
    
//     if (home_parser.status != TP_SUCCESS)
//     {
//         template_parser_free(&home_parser);
//         return WP_TEMPLATE_ERROR;
//     }

//     String content = generate_home_page(home_parser.stages, portfolio.peronas);
//     char filename[256];
//     sprintf(filename, "%s/index.html", portfolio.outdir);
//     int write_success = write_file(filename, content);

//     template_parser_free(&home_parser);
//     string_free(&content);
    
//     if (!write_success)
//         return WP_WRITE_ERROR;

//     // Personas
    
//     String page_template = load_file(portfolio.page_template);
//     if (!page_template)
//         return WP_MISSING_TEMPLATE;

//     Template_Parser page_parser = template_parser_make(page_template);
//     template_parser_parse(&page_parser);

//     if (page_parser.status != TP_SUCCESS)
//     {
//         template_parser_free(&page_parser);
//         return WP_TEMPLATE_ERROR;
//     }

//     int num_portfolios = da_size(portfolio.peronas);
//     for (int i = 0; write_success && i < num_portfolios; i++)
//     {
//         content = generate_persona_page(page_parser.stages, portfolio.peronas, i);
//         sprintf(filename, "%s/%s.html", portfolio.outdir, portfolio.peronas[i].name);
        
//         write_success = write_file(filename, content);
//         string_free(&content);
//     }

//     template_parser_free(&page_parser);

//     if (!write_success)
//         return WP_WRITE_ERROR;

//     return WP_SUCCESS;
// }