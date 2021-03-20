#include <stdio.h>

#include "generator/filestuff.h"
#include "generator/parser.h"
#include "generator/portfolio.h"

#include "containers/darray.h"
#include "containers/string.h"

// #define DEBUG

int main(int argc, char* argv[])
{
    #ifdef DEBUG
    String contents = load_file("portfolio/portfolio.txt");
    #else    
    if (argc < 2)
    {
        printf("Error: No file provided\n");
        return 1;
    }

    String contents = load_file(argv[1]);
    #endif    

    Lexer lexer = lexer_make(contents);
    lexer_lex(&lexer);

    Parser parser = parser_make(lexer.tokens);
    Portfolio portfolio = parser_parse(&parser);

    if (parser.status == PARSER_FAILURE)
        printf("%s\n", parser.message);
    else
    {
        printf("Home Template: %s\n", portfolio.home_template);
        printf("Page Template: %s\n", portfolio.page_template);
        printf("Output Directory: %s\n", portfolio.outdir);
        printf("\n");

        da_foreach(Persona, persona, portfolio.peronas)
        {
            printf("Name: %s\n", persona->name);
            printf("Color: %s\n", persona->color);
            printf("Image: %s\n", persona->image);
            printf("Icon: %s\n", persona->icon);

            printf("Abilities:\n");
            da_foreach(String, ab, persona->abilities)
                printf("    %s\n", *ab);

            printf("Blerb: %s\n", persona->blerb);

            printf("Projects:\n");
            da_foreach(Project, pj, persona->projects)
            {
                printf("    Name: %s\n", pj->name);
                printf("    Date: %s\n", pj->date);
                printf("    Desc: %s\n", pj->description);
                printf("    Link: %s\n", pj->link);

                printf("    Skills:\n");
                da_foreach(String, skill, pj->skills)
                    printf("        %s\n", *skill);
            }

            printf("\n");
        }
    }

    string_free(&contents);
    lexer_free(&lexer);
    parser_free(&parser);
    portfolio_free(&portfolio);
}