#include "parser.h"

#include <stdio.h>

#include "containers/hd_assert.h"

#define STRING_IMPL
#include "containers/string.h"

#define DARRAY_IMPL
#include "containers/darray.h"
#include "portfolio.h"

Lexer lexer_make(String contents)
{
    DArray(Token) tokens = NULL;
    da_make(tokens);
    return (Lexer){ contents, 0, tokens, LEXER_NO_LEX, NULL };
}

void lexer_free(Lexer* lexer)
{
    if (lexer->contents)
        string_free(&lexer->contents);
    
    if (lexer->message)
        string_free(&lexer->message);
    
    if (lexer->tokens)
    {
        da_foreach(Token, token, lexer->tokens)
        {
            if (token->value)
                string_free(&token->value);
        }

        da_free(lexer->tokens);
    }
    
    lexer->index = 0;
    lexer->status = LEXER_NO_LEX;
}

static int is_alpha_or_us(char ch)
{
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           ch == '_';
}

static char peek(Lexer* lexer, int offset)
{
    return lexer->contents[lexer->index + offset];
}

static char consume(Lexer* lexer)
{
    return lexer->contents[lexer->index++];
}

void lexer_lex(Lexer* lexer)
{
    #define LEX_ERROR(m) \
        do {                                                \
            lexer->status = LEXER_FAILURE;                  \
            lexer->message = string_make("Lexer Error: "m); \
        } while (0)

    // Just in case
    lexer->index = 0;
    lexer->status = LEXER_NO_LEX;

    int len = string_length(lexer->contents);
    while (lexer->status != LEXER_FAILURE && lexer->index < len)
    {
        char ch = peek(lexer, 0);

        switch (ch)
        {
            // Single character tokens
            case TOKEN_DOLLAR:
            case TOKEN_L_BRACKET:
            case TOKEN_R_BRACKET:
            case TOKEN_L_BRACE:
            case TOKEN_R_BRACE:
            case TOKEN_COLON:
            case TOKEN_SEMI_COLON:
            case TOKEN_COMMA:
            {
                Token t = { ch, NULL };
                da_push_back(lexer->tokens, t);
                consume(lexer);
            } break;

            case '`':       // @Todo: Change this so that the string actually
                            //        gets formatted.
            case '"':
            {
                // @Todo: Change this after implementing format
                //        strings
                char end_char = ch;

                consume(lexer);
                int start_index = lexer->index;
                int end_index = start_index;

                int string_is_valid = 1;
                while (peek(lexer, 0) != end_char)
                {
                    if (peek(lexer, 0) == '\0')
                    {
                        string_is_valid = 0;
                        break;
                    }

                    consume(lexer);
                    end_index++;
                }

                if (string_is_valid)
                {
                    int length = end_index - start_index;
                    String str = string_make_till_n(lexer->contents + start_index, length);
                    Token t = { TOKEN_STRING, str };
                    da_push_back(lexer->tokens, t);
                    consume(lexer);
                }
                else
                    LEX_ERROR("Couldn't find closing '\"' for string.");
            } break;

            case '/':
            {
                // Comments aren't pushed as tokens
                // to reduce confusion
                if (peek(lexer, 1) == '/')
                {
                    // Ignore the rest of the line
                    while (peek(lexer, 0) && peek(lexer, 0) != '\n')
                        consume(lexer);
                }
                else if (peek(lexer, 1) == '*')
                {
                    // Ignore till */ is encoutered
                    int comment_is_valid = 0;
                    while (peek(lexer, 0))
                    {
                        if (peek(lexer, 0) == '*' && peek(lexer, 1) == '/')
                        {
                            consume(lexer); consume(lexer);
                            comment_is_valid = 1;
                            break;
                        }

                        consume(lexer);
                    }

                    if (!comment_is_valid)
                        LEX_ERROR("Block comment doesn't end.");
                }
                else
                    LEX_ERROR("Expected 2 '/'s for comment. Got single '/'.");
            } break;

            default:
            {
                // Identifiers
                if (is_alpha_or_us(ch))
                {
                    int start_index = lexer->index;
                    int end_index = start_index;

                    while (is_alpha_or_us(peek(lexer, 0)))
                    {
                        end_index++;
                        consume(lexer);
                    }

                    int length = end_index - start_index;
                    String str = string_make_till_n(lexer->contents + start_index, length);
                    Token t = { TOKEN_INDENTIFIER, str };
                    da_push_back(lexer->tokens, t);
                }
                else consume(lexer);
            } break;
        }
    }

    if (lexer->status != LEXER_FAILURE)
    {
        lexer->status = LEXER_SUCCESS;
        lexer->message = NULL;
    }

    #undef LEX_ERROR
}

Parser parser_make(DArray(Token) tokens)
{
    return (Parser){ tokens, 0, PARSER_NO_PARSE, NULL };
}

void parser_free(Parser* parser)
{
    if (parser->message)
        string_free(&parser->message);
    
    if (parser->tokens)
    {
        da_foreach(Token, token, parser->tokens)
        {
            if (token->value)
                string_free(&token->value);
        }

        da_free(parser->tokens);
    }
    
    parser->current_token_idx = 0;
    parser->status = PARSER_NO_PARSE;
}

static int expect_next_token(Parser* parser, Token_Type type)
{
    return parser->tokens[parser->current_token_idx + 1].type == type;
}

static int curr_token_is_type(Parser* parser, Token_Type type)
{
    return parser->tokens[parser->current_token_idx].type == type;
}

static Token curr_token(Parser* parser)
{
    return parser->tokens[parser->current_token_idx];
}

static void advance_token(Parser* parser)
{
    parser->current_token_idx++;
}

#define PARSE_ERROR(m) \
    do {                                                \
        parser->status = PARSER_FAILURE;                \
        parser->message = string_make("Parse Error: "m);\
    } while (0)

#define CHECK_STATEMENT_END() \
    do {                                                    \
        if (!curr_token_is_type(parser, TOKEN_SEMI_COLON))  \
            PARSE_ERROR("Statements must end with a ';'."); \
        else                                                \
            advance_token(parser);                          \
    } while (0)

static void fill_string_array(Parser* parser, DArray(String)* arr)
{
    // Checked for '[' in parse_persona()
    advance_token(parser);
    int num_tokens = da_size(parser->tokens);
    while (parser->status != PARSER_FAILURE)
    {
        if (parser->current_token_idx >= num_tokens)
        {
            PARSE_ERROR("String array was never closed with ']'.");
            continue;
        }

        if (curr_token_is_type(parser, TOKEN_STRING))
        {
            da_push_back((*arr), string_make(curr_token(parser).value));
            advance_token(parser);

            if (curr_token_is_type(parser, TOKEN_COMMA))
            {
                advance_token(parser);
                continue;
            }
        }

        if (curr_token_is_type(parser, TOKEN_R_BRACKET))
        {
            advance_token(parser);
            break;
        }

        PARSE_ERROR("Unexpected token found in string array.");
    }
}

static Project parser_project(Parser* parser)
{
    Project project = project_make();

    // Checked for '{' in fill_projects()
    advance_token(parser);
    int num_tokens = da_size(parser->tokens);
    while (parser->status != PARSER_FAILURE && !curr_token_is_type(parser, TOKEN_R_BRACE))
    {
        if (parser->current_token_idx >= num_tokens)
        {
            PARSE_ERROR("Project object was never closed with '}'.");
            continue;
        }

        if (!curr_token_is_type(parser, TOKEN_INDENTIFIER))
        {
            PARSE_ERROR("Expected an attribute inside object.");
            continue;
        }

        String attribute = curr_token(parser).value;

        advance_token(parser);
        if (!curr_token_is_type(parser, TOKEN_COLON))
        {
            PARSE_ERROR("Expected ':' after attribute.");
            continue;
        }

        advance_token(parser);

        if (string_cmp(attribute, "name"))
        {
            if (!curr_token_is_type(parser, TOKEN_STRING))
            {
                PARSE_ERROR("Name attribute of a project should be equal to a string.");
                continue;
            }

            project.name = string_make(curr_token(parser).value);
            advance_token(parser);

            CHECK_STATEMENT_END();
            continue;
        }

        if (string_cmp(attribute, "date"))
        {
            if (!curr_token_is_type(parser, TOKEN_STRING))
            {
                PARSE_ERROR("Date attribute of a project should be equal to a string.");
                continue;
            }

            project.date = string_make(curr_token(parser).value);
            advance_token(parser);

            CHECK_STATEMENT_END();
            continue;
        }

        if (string_cmp(attribute, "desc"))
        {
            if (!curr_token_is_type(parser, TOKEN_STRING))
            {
                PARSE_ERROR("Desc attribute of a project should be equal to a string.");
                continue;
            }

            project.description = string_make(curr_token(parser).value);
            advance_token(parser);

            CHECK_STATEMENT_END();
            continue;
        }

        if (string_cmp(attribute, "skills"))
        {
            if (!curr_token_is_type(parser, TOKEN_L_BRACKET))
            {
                PARSE_ERROR("Skills attribute of a project should be equal to an array of strings.");
                continue;
            }

            fill_string_array(parser, &project.skills);

            if (parser->status != PARSER_FAILURE)
                CHECK_STATEMENT_END();
            
            continue;
        }        
    }

    if (parser->status != PARSER_FAILURE)
        advance_token(parser);

    return project;
}

static void fill_projects(Parser* parser, DArray(Project)* arr)
{
    // Checked for '[' in parse_persona()
    advance_token(parser);
    int num_tokens = da_size(parser->tokens);
    while (parser->status != PARSER_FAILURE)
    {
        if (parser->current_token_idx >= num_tokens)
        {
            PARSE_ERROR("Projects array was never closed with ']'.");
            continue;
        }

        if (curr_token_is_type(parser, TOKEN_L_BRACE))
        {
            Project project = parser_project(parser);
            if (parser->status != PARSER_FAILURE)
            {
                da_push_back((*arr), project);

                if (curr_token_is_type(parser, TOKEN_COMMA))
                {
                    advance_token(parser);
                    continue;
                }
            }
            else
                project_free(&project);
        }

        if (parser->status != PARSER_FAILURE)
        {
            if (curr_token_is_type(parser, TOKEN_R_BRACKET))
            {
                advance_token(parser);
                break;
            }

            PARSE_ERROR("Unexpected token found in string array.");
        }
    }
}

static Persona parse_persona(Parser* parser)
{
    Persona persona = persona_make();

    // Already checked for string in parser_parse()
    persona.name = string_make(parser->tokens[parser->current_token_idx].value);
    advance_token(parser);

    if (!curr_token_is_type(parser, TOKEN_L_BRACE))
        return persona;

    advance_token(parser);
    int num_tokens = da_size(parser->tokens);
    while (parser->status != PARSER_FAILURE && !curr_token_is_type(parser, TOKEN_R_BRACE))
    {
        if (parser->current_token_idx >= num_tokens)
        {
            PARSE_ERROR("Persona object was never closed with '}'.");
            continue;
        }

        if (!curr_token_is_type(parser, TOKEN_INDENTIFIER))
        {
            PARSE_ERROR("Expected an attribute inside object.");
            continue;
        }

        String attribute = curr_token(parser).value;

        advance_token(parser);
        if (!curr_token_is_type(parser, TOKEN_COLON))
        {
            PARSE_ERROR("Expected ':' after attribute.");
            continue;
        }

        advance_token(parser);

        if (string_cmp(attribute, "color"))
        {
            if (!curr_token_is_type(parser, TOKEN_STRING))
            {
                PARSE_ERROR("Color attribute of a persona should be equal to a string.");
                continue;
            }

            persona.color = string_make(curr_token(parser).value);
            advance_token(parser);

            CHECK_STATEMENT_END();
            continue;
        }

        if (string_cmp(attribute, "blerb"))
        {
            if (!curr_token_is_type(parser, TOKEN_STRING))
            {
                PARSE_ERROR("Blerb attribute of a persona should be equal to a string.");
                continue;
            }

            persona.blerb = string_make(curr_token(parser).value);
            advance_token(parser);

            CHECK_STATEMENT_END();
            continue;
        }

        if (string_cmp(attribute, "abilities"))
        {
            if (!curr_token_is_type(parser, TOKEN_L_BRACKET))
            {
                PARSE_ERROR("Abilties attribute of a persona should be equal to an array of strings.");
                continue;
            }

            fill_string_array(parser, &persona.abilities);

            if (parser->status != PARSER_FAILURE)
                CHECK_STATEMENT_END();
            
            continue;
        }

        // @Todo: For now projects are just strings.
        //        Later they must be parsed as objects.
        if (string_cmp(attribute, "projects"))
        {
            if (!curr_token_is_type(parser, TOKEN_L_BRACKET))
            {
                PARSE_ERROR("Projects attribute a persona should be equal to an array of strings.");
                continue;
            }

            fill_projects(parser, &persona.projects);

            if (parser->status != PARSER_FAILURE)
                CHECK_STATEMENT_END();
            
            continue;
        }
    }

    if (parser->status != PARSER_FAILURE)
        advance_token(parser);

    return persona;
}

Portfolio parser_parse(Parser* parser)
{
    Portfolio portfolio = portfolio_make();

    // Just in case
    parser->current_token_idx = 0;
    parser->status = PARSER_NO_PARSE;

    int num_tokens = da_size(parser->tokens);
    while (parser->status != PARSER_FAILURE && parser->current_token_idx < num_tokens)
    {
        if (curr_token_is_type(parser, TOKEN_DOLLAR))
        {
            advance_token(parser);
            if (!curr_token_is_type(parser, TOKEN_INDENTIFIER))
            {
                PARSE_ERROR("Expected an identifier after '$'");
                continue;
            }

            String i_name = curr_token(parser).value;
            advance_token(parser);

            if (!curr_token_is_type(parser, TOKEN_STRING))
            {
                PARSE_ERROR("Expected a string after $ property.");
                continue;
            }

            if (string_cmp(i_name, "home_template"))
            {
                portfolio.home_template = string_make(curr_token(parser).value);
                advance_token(parser);
                continue;
            }

            if (string_cmp(i_name, "page_template"))
            {
                portfolio.page_template = string_make(curr_token(parser).value);
                advance_token(parser);
                continue;
            }

            if (string_cmp(i_name, "outdir"))
            {
                portfolio.outdir = string_make(curr_token(parser).value);
                advance_token(parser);
                continue;
            }

            if (string_cmp(i_name, "persona"))
            {
                Persona persona = parse_persona(parser);

                if (parser->status != PARSER_FAILURE)
                    da_push_back(portfolio.peronas, persona);
                else
                    persona_free(&persona);

                continue;
            }
        } else
            PARSE_ERROR("Expected identifier after '$'.");
    }

    if (parser->status != PARSER_FAILURE)
    {
        parser->status = PARSER_SUCCESS;
        parser->message = NULL;
    }

    return portfolio;
}

#undef CHECK_STATEMENT_END
#undef PARSE_ERROR