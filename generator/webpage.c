#include "webpage.h"

#include <stdio.h>
#include "filestuff.h"
#include "containers/hd_assert.h"
#include "containers/darray.h"

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
    s.property.name = NULL;
    return s;
}

Stage list_stage_make()
{
    Stage s;
    s.type = STAGE_LIST;
    s.list.list_name = NULL;
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
        } break;

        case STAGE_LIST:
        {
            if (stage->list.list_name)
                string_free(&stage->list.list_name);

            if (stage->list.it_name)
                string_free(&stage->list.it_name);

            da_foreach(Stage, s, stage->list.stages)
                stage_free(s);
            da_free(stage->list.stages);
        } break;

        case STAGE_CONDITIONAL:
        {
            if (stage->conditional.condition)
                string_free(&stage->conditional.condition);

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

static int is_alpha_or_dot(char ch)
{
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           ch == '.';
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

static void fill_stages(Template_Parser* tp, char end, DArray(Stage)* stages);

#define TP_ERROR(tp, m) \
    do {                                                \
        tp->status = TP_FAILURE;                        \
        tp->message = string_make("Template Error: "m); \
    } while (0)

static String get_identifier(Template_Parser* tp)
{
    consume_ws(tp);

    int start_idx = tp->cur_index;
    while (is_alpha_or_dot(peek(tp, 0)))
        consume(tp);

    if (tp->cur_index <= start_idx)
        return NULL;

    return string_make_till_n(tp->content + start_idx, tp->cur_index - start_idx);
}

// Starts parsing after ->
static Stage get_list(Template_Parser* tp, String name)
{
    Stage list = list_stage_make();
    list.list.list_name = name;

    String it_name = get_identifier(tp);
    if (!it_name)
    {
        TP_ERROR(tp, "Expected an iterator name in list tag.");
        return list;
    }

    list.list.it_name = it_name;

    consume_ws(tp);
    if (peek(tp, 0) != '{')
    {
        TP_ERROR(tp, "Expected '{' in list tag.");
        return list;
    }

    consume(tp);
    fill_stages(tp, '}', &(list.list.stages));

    if (peek(tp, 0) == '}')
        consume(tp);
    else
    {
        TP_ERROR(tp, "List block was never closed.");
        return list;
    }

    consume_ws(tp);
    if (peek(tp, 0) == '>')
        consume(tp);
    else
        TP_ERROR(tp, "List tag was never closed.");

    return list;
}

static Stage get_conditional(Template_Parser* tp)
{
    Stage cond = cond_stage_make(STAGE_CONDITIONAL);

    String condition = get_identifier(tp);
    if (!condition)
        TP_ERROR(tp, "Expected a condition after if tag.");

    cond.conditional.condition = condition;

    consume_ws(tp);

    if (peek(tp, 0) != '{')
    {
        TP_ERROR(tp, "Expected '{' in if tag.");
        return cond;
    }

    consume(tp);
    fill_stages(tp, '}', &(cond.conditional.stages_if_true));
    if (peek(tp, 0) != '}')
    {
        TP_ERROR(tp, "If block wasn't closed with a '}'.");
        return cond;
    }

    consume(tp);
    consume_ws(tp);

    if (peek(tp, 0) == '>')
        consume(tp);
    else
    {
        String indentifier = get_identifier(tp);
        if (string_cmp(indentifier, "else"))
        {
            consume_ws(tp);

            if (peek(tp, 0) != '{')
            {
                TP_ERROR(tp, "Expected '{' after else in if tag.");
                return cond;
            }

            consume(tp);
            fill_stages(tp, '}', &(cond.conditional.stages_if_false));
            if (peek(tp, 0) != '}')
            {
                TP_ERROR(tp, "Else block wasn't closed with a '}'.");
                return cond;
            }

            consume(tp);
            consume_ws(tp);

            if (peek(tp, 0) == '>')
                consume(tp);
            else
                TP_ERROR(tp, "If tag was never closed.");
        }
        else
            TP_ERROR(tp, "If tag was never closed.");

        string_free(&indentifier);
    }

    return cond;
}

// tp->cur_index is stopped at the first instance of end char
static void fill_stages(Template_Parser* tp, char end, DArray(Stage)* stages)
{
    int len = string_length(tp->content);
    while (tp->status != TP_FAILURE && tp->cur_index < len)
    {
        consume_ws(tp);

        // Collect all the plain html
        int start_idx = tp->cur_index;
        while (tp->cur_index < len &&
               peek(tp, 0) != end  &&
               (peek(tp, 0) != '<' || peek(tp, 1) != '$'))
            consume(tp);

        if (tp->cur_index > start_idx)
        {
            Stage html = html_stage_make();
            html.html.content = string_make_till_n(tp->content + start_idx, tp->cur_index - start_idx);
            da_push_back((*stages), html);
        }

        if (peek(tp, 0) == end)
            break;

        // Consume < and $
        consume(tp); consume(tp);

        // Determine which type of stage it is
        String identifier = get_identifier(tp);
        if (!identifier)
        {
            TP_ERROR(tp, "Expected an idenifier or \"if\" inside $ tag.");
            continue;
        }

        if (string_cmp(identifier, "if"))
        {
            Stage conditional = get_conditional(tp);
            da_push_back((*stages), conditional);
        }
        else
        {
            consume_ws(tp);

            if (peek(tp, 0) == '-' && peek(tp, 1) == '>')
            {
                consume(tp); consume(tp);
                Stage list = get_list(tp, identifier);
                da_push_back((*stages), list);
            }
            else if (peek(tp, 0) == '>')
            {
                Stage prop = prop_stage_make();
                prop.property.name = identifier;
                da_push_back((*stages), prop);
            }
            else
            {
                TP_ERROR(tp, "$ tag must be closed with a >");
                string_free(&identifier);
                continue;
            }
        }
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
                printf("[ html ]\n");
            } break;

            case STAGE_PROPERTY:
            {
                printf("[ property: %s ]\n", s->property.name);
            } break;

            case STAGE_LIST:
            {
                printf("[ list: %s -> %s [\n", s->list.list_name, s->list.it_name);
                print_stages(s->list.stages);
                printf("\n]]\n");
            } break;

            case STAGE_CONDITIONAL:
            {
                printf("[ conditional: (%s) true -> [\n", s->conditional.condition);
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

//     String content = fill_home_template(home_parser.stages, portfolio.peronas);
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
//         content = fill_page_template(page_parser.stages, portfolio.peronas, i);
//         sprintf(filename, "%s/%s.html", portfolio.outdir, portfolio.peronas[i].name);
        
//         write_success = write_file(filename, content);
//         string_free(&content);
//     }

//     template_parser_free(&page_parser);

//     if (!write_success)
//         return WP_WRITE_ERROR;

//     return WP_SUCCESS;
// }