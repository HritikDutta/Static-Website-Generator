#include "webpage.h"

#include <stdio.h>
#include "filestuff.h"
#include "containers/hd_assert.h"
#include "containers/darray.h"

#define DICTIONARY_IMPL
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
    s.property.name         = NULL;
    s.property.parent_index = -1;
    return s;
}

Stage list_stage_make()
{
    Stage s;
    s.type = STAGE_LIST;
    s.list.it_name      = NULL;
    s.list.parent_index = -1;
    s.list.stages       = NULL;
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
static Stage get_list(Template_Parser* tp, DArray(Stage)* stages)
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

    list.list.parent_index = da_size(tp->stages) - 1;
    list.list.it_name = identifier;

    consume(tp);
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
        fill_stages(tp, '}', &cond.conditional.stages_if_false);

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
    // This is a bit hacky I guess but it'll reduce one
    // argument that would've had to be passed.
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

            Stage list = get_list(tp, stages);
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

        int parent_index = -1;

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

            parent_index = last_elem_idx;
        }

        Stage prop = prop_stage_make();
        String prop_name = get_identifier(tp);

        prop.property.name = prop_name;
        prop.property.parent_index = parent_index;
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
                printf("[ property: %s parent: ", s->property.name);
                if (s->property.parent_index != -1)
                    printf(stages[s->property.parent_index].property.name);
                else
                    printf("(null)");
                
                printf(" ]\n");
            } break;

            case STAGE_LIST:
            {
                printf("[ list: %s -> %s [\n", stages[s->list.parent_index].property.name, s->list.it_name);
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

static void add_to_buffer(Generator* gen, String str)
{
    int len = string_length(str) - 1;
    for (int i = 0; i < len; i++)
        da_push_back(gen->buffer, str[i]);
}

Generator generator_make(DArray(Stage) stages)
{
    Generator g = { 0 };
    g.stages = stages;
    da_make(g.buffer);
    dict_make(g.vars);
    return g;
}

void generator_free(Generator* generator)
{
    da_foreach(Stage, stage, generator->stages)
        stage_free(stage);
    da_free(generator->stages);

    dict_foreach(Webpage_Variable, var, generator->vars)
        if (var->key)
            string_free(&var->key);
    dict_free(generator->vars);

    da_free(generator->buffer);

    if (generator->message)
        string_free(&generator->message);
}

#define GEN_ERROR(gen, m) \
    do {                                                  \
        gen->status  = GEN_FAILURE;                       \
        gen->message = string_make("Generator Error: "m); \
    } while (0)

static void fill_persona(Generator* gen, Stage* stage, Persona persona)
{
    if (string_cmp(stage->property.name, "name"))
    {
        Webpage_Variable v = wv_make_prop(persona.name);
        dict_put(gen->vars, stage->property.name, v);
        add_to_buffer(gen, persona.name);
        return;
    }

    if (string_cmp(stage->property.name, "color"))
    {
        Webpage_Variable v = wv_make_prop(persona.color);
        dict_put(gen->vars, stage->property.name, v);
        add_to_buffer(gen, persona.color);
        return;
    }

    if (string_cmp(stage->property.name, "blerb"))
    {
        Webpage_Variable v = wv_make_prop(persona.blerb);
        dict_put(gen->vars, stage->property.name, v);
        add_to_buffer(gen, persona.blerb);
        return;
    }

    if (string_cmp(stage->property.name, "image"))
    {
        Webpage_Variable v = wv_make_prop(persona.image);
        dict_put(gen->vars, stage->property.name, v);
        add_to_buffer(gen, persona.image);
        return;
    }
    
    if (string_cmp(stage->property.name, "icon"))
    {
        Webpage_Variable v = wv_make_prop(persona.icon);
        dict_put(gen->vars, stage->property.name, v);
        add_to_buffer(gen, persona.icon);
        return;
    }

    if (string_cmp(stage->property.name, "abilities"))
    {
        Webpage_Variable v = wv_make_slist(persona.abilities);
        dict_put(gen->vars, stage->property.name, v);
        return;
    }

    if (string_cmp(stage->property.name, "projects"))
    {
        Webpage_Variable v = wv_make_proj_list(persona.projects);
        dict_put(gen->vars, stage->property.name, v);
        return;
    }

    GEN_ERROR(gen, "Property not found");
}

static void fill_property(Generator* gen, Stage* stage, Portfolio portfolio, int selected_index)
{
    if (string_cmp(stage->property.name, "personas"))
    {
        Webpage_Variable v = wv_make_persona_list(portfolio.personas);
        dict_put(gen->vars, "personas", v);
        return;
    }

    fill_persona(gen, stage, portfolio.personas[selected_index]);
}

static void fill_project(Generator* gen, Stage* stage, Project project)
{
    if (string_cmp(stage->property.name, "name"))
    {
        Webpage_Variable v = wv_make_prop(project.name);
        dict_put(gen->vars, stage->property.name, v);
        add_to_buffer(gen, project.name);
        return;
    }

    if (string_cmp(stage->property.name, "date"))
    {
        Webpage_Variable v = wv_make_prop(project.date);
        dict_put(gen->vars, stage->property.name, v);
        add_to_buffer(gen, project.date);
        return;
    }

    if (string_cmp(stage->property.name, "description"))
    {
        Webpage_Variable v = wv_make_prop(project.description);
        dict_put(gen->vars, stage->property.name, v);
        add_to_buffer(gen, project.description);
        return;
    }

    if (string_cmp(stage->property.name, "link"))
    {
        Webpage_Variable v = wv_make_prop(project.link);
        dict_put(gen->vars, stage->property.name, v);
        add_to_buffer(gen, project.link);
        return;
    }

    if (string_cmp(stage->property.name, "description"))
    {
        Webpage_Variable v = wv_make_prop(project.description);
        dict_put(gen->vars, stage->property.name, v);
        add_to_buffer(gen, project.description);
        return;
    }

    if (string_cmp(stage->property.name, "skills"))
    {
        Webpage_Variable v = wv_make_slist(project.skills);
        dict_put(gen->vars, stage->property.name, v);
        return;
    }

    GEN_ERROR(gen, "Property not found");
}

static int evaluate_condition(Generator* gen, DArray(Stage) stages)
{
    // The last element will be the condition
    int last_idx = da_size(stages) - 1;
    
    if (stages[last_idx].type != STAGE_PROPERTY)
    {
        GEN_ERROR(gen, "Condition can only be evaluated for a property");
        return 0;
    }

    if (string_cmp(stages[last_idx].property.name, "selected"))
    {
        Dict_Bkt(Webpage_Variable) var = dict_find(gen->vars, stages[stages[last_idx].property.parent_index].property.name);
        if (var->value.type != WV_PERSONA)
        {
            GEN_ERROR(gen, "selected is a sub property of Persona type");
            return 0;
        }

        return var->value.persona.selected;
    }

    GEN_ERROR(gen, "Condition couldn't be evaluated");
    return 0;
}

static void fill_buffer(Generator* gen, DArray(Stage) stages,
                        Portfolio portfolio, int selected_index)
{
    da_foreach(Stage, stage, stages)
    {
        if (gen->status == GEN_FAILURE)
            break;

        switch (stage->type)
        {
            case STAGE_HTML:
            {
                add_to_buffer(gen, stage->html.content);
            } break;

            case STAGE_PROPERTY:
            {
                if (stage->property.parent_index == -1)
                {
                    Dict_Bkt(Webpage_Variable) var = dict_find(gen->vars, stage->property.name);
                    
                    // If the variable was already cached
                    if (var != dict_end(gen->vars))
                    {
                        if (var->value.type == WV_PROP)
                            add_to_buffer(gen, var->value.prop.value);
    
                        break;
                    }

                    fill_property(gen, stage, portfolio, selected_index);
                    break;
                }

                String parent_name = stages[stage->property.parent_index].property.name;
                Dict_Bkt(Webpage_Variable) parent_var = dict_find(gen->vars, parent_name);
                
                if (parent_var == dict_end(gen->vars))
                {
                    GEN_ERROR(gen, "Parent variable is invalid");
                    break;
                }

                Dict_Bkt(Webpage_Variable) var = dict_find(gen->vars, stage->property.name);
                    
                // If the variable was already cached
                if (var != dict_end(gen->vars))
                {
                    if (var->value.type == WV_PROP)
                        add_to_buffer(gen, var->value.prop.value);

                    break;
                }

                switch (parent_var->value.type)
                {
                    case WV_PERSONA:
                    {
                        fill_persona(gen, stage, parent_var->value.persona.data);
                    } break;

                    case WV_PROJECT:
                    {
                        fill_project(gen, stage, parent_var->value.project.data);
                    } break;

                    case WV_PROP:
                    {
                        GEN_ERROR(gen, "Property doesn't have sub properties.");
                    } break;
                    case WV_STRING_LIST:
                    {
                        GEN_ERROR(gen, "String list doesn't have sub properties.");
                    } break;
                }
            } break;

            case STAGE_LIST:
            {
                if (stage->list.parent_index == -1)
                {
                    GEN_ERROR(gen, "List tag must have a parent property");
                    break;
                }

                String parent_name = stages[stage->list.parent_index].property.name;

                if (string_cmp(parent_name, "personas"))
                {
                    int size = da_size(portfolio.personas);
                    for (int i = 0; gen->status != GEN_FAILURE && i < size; i++)
                    {
                        Webpage_Variable v = wv_make_persona(portfolio.personas[i], selected_index == i);
                        dict_put(gen->vars, stage->list.it_name, v);
                        fill_buffer(gen, stage->list.stages, portfolio, selected_index);
                    }
                    break;
                }

                if (string_cmp(parent_name, "projects"))
                {
                    Dict_Bkt(Webpage_Variable) var = dict_find(gen->vars, "projects");
                    int size = da_size(var->value.proj_list.list);
                    for (int i = 0; gen->status != GEN_FAILURE && i < size; i++)
                    {
                        Webpage_Variable v = wv_make_project(var->value.proj_list.list[i]);
                        dict_put(gen->vars, stage->list.it_name, v);
                        fill_buffer(gen, stage->list.stages, portfolio, selected_index);
                    }
                    break;
                }

                if (string_cmp(parent_name, "abilities"))
                {
                    Dict_Bkt(Webpage_Variable) var = dict_find(gen->vars, "abilities");
                    int size = da_size(var->value.string_list.list);
                    for (int i = 0; gen->status != GEN_FAILURE && i < size; i++)
                    {
                        Webpage_Variable v = wv_make_prop(var->value.string_list.list[i]);
                        dict_put(gen->vars, stage->list.it_name, v);
                        fill_buffer(gen, stage->list.stages, portfolio, selected_index);
                    }
                    break;
                }

                if (string_cmp(parent_name, "skills"))
                {
                    Dict_Bkt(Webpage_Variable) var = dict_find(gen->vars, "skills");
                    int size = da_size(var->value.string_list.list);
                    for (int i = 0; gen->status != GEN_FAILURE && i < size; i++)
                    {
                        Webpage_Variable v = wv_make_prop(var->value.string_list.list[i]);
                        dict_put(gen->vars, stage->list.it_name, v);
                        fill_buffer(gen, stage->list.stages, portfolio, selected_index);
                    }
                    break;
                }
            } break;

            case STAGE_CONDITIONAL:
            {
                int result = evaluate_condition(gen, stage->conditional.condition);
                
                if (gen->status == GEN_FAILURE)
                    break;

                if (result)
                    fill_buffer(gen, stage->conditional.stages_if_true, portfolio, selected_index);
                else
                    fill_buffer(gen, stage->conditional.stages_if_false, portfolio, selected_index);
            } break;
        }
    }
}

#undef GEN_ERROR

void generate_persona_page(Generator* generator, Portfolio portfolio, int selected_index)
{
    // Just in case
    generator->cur_index = 0;
    generator->status = GEN_NO_GEN;

    fill_buffer(generator, generator->stages, portfolio, selected_index);

    if (generator->status != GEN_FAILURE)
        generator->status = GEN_SUCCESS;
}

Webpage_Status generate_webpages(Portfolio portfolio)
{
    String page_template = load_file(portfolio.page_template);
    if (!page_template)
        return WP_MISSING_TEMPLATE;

    Template_Parser tp = template_parser_make(page_template);
    template_parser_parse(&tp);

    if (tp.status == TP_FAILURE)
    {
        printf("%s\n", tp.message);
        return WP_TEMPLATE_ERROR;
    }

    Generator gen = generator_make(tp.stages);
    int num_personas = da_size(portfolio.personas);
    for (int i = 0; gen.status != GEN_FAILURE && i < num_personas; i++)
    {
        generate_persona_page(&gen, portfolio, i);
        
        if (gen.status == GEN_FAILURE)
        {
            printf("%s\n", gen.message);
            break;
        }

        String output = generator_output(gen);
        char filename[128];
        sprintf(filename, "%s/%s.html", portfolio.outdir, portfolio.personas[i].name);
        int res = write_file(filename, output);

        if (!res)
            return WP_WRITE_ERROR;

        generator_reset(&gen);
        string_free(&output);
    }

    // template_parser_free(&tp);
    generator_free(&gen);

    if (gen.status != GEN_SUCCESS)
        return WP_TEMPLATE_ERROR;

    return WP_SUCCESS;
}

void generator_reset(Generator* generator)
{
    da_free(generator->buffer);
    da_make(generator->buffer);

    dict_foreach(Webpage_Variable, var, generator->vars)
        if (var->key)
            string_free(&var->key);
    dict_free(generator->vars);
    dict_make(generator->vars);
}

String generator_output(Generator generator)
{
    if (generator.status != GEN_SUCCESS)
        return NULL;

    int end = da_size(generator.buffer) - 1;
    
    if (generator.buffer[end])
        da_push_back(generator.buffer, '\0');
    
    return string_make(generator.buffer);
}

Webpage_Variable wv_make_prop(String value)
{
    Webpage_Variable wv;

    wv.type = WV_PROP;
    wv.prop.value = value;

    return wv;
}

Webpage_Variable wv_make_persona(Persona data, int selected)
{
    Webpage_Variable wv;

    wv.type = WV_PERSONA;
    wv.persona.selected = selected;
    wv.persona.data = data;

    return wv;
}

Webpage_Variable wv_make_persona_list(DArray(Persona) list)
{
    Webpage_Variable wv;

    wv.type = WV_PERSONA_LIST;
    wv.persona_list.list = list;
    
    return wv;
}

Webpage_Variable wv_make_project(Project data)
{
    Webpage_Variable wv;

    wv.type = WV_PROJECT;
    wv.project.data = data;

    return wv;
}

Webpage_Variable wv_make_proj_list(DArray(Project) list)
{
    Webpage_Variable wv;

    wv.type = WV_PROJECT_LIST;
    wv.proj_list.list = list;
    
    return wv;
}

Webpage_Variable wv_make_slist(DArray(String) slist)
{
    Webpage_Variable wv;

    wv.type = WV_STRING_LIST;
    wv.string_list.list = slist;
    
    return wv;
}