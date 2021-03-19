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

            printf("Abilities:\n");
            da_foreach(String, ab, persona->abilities)
                printf("%s\n", *ab);

            printf("Blerb: %s\n", persona->blerb);

            printf("Projects:\n");
            da_foreach(String, pj, persona->projects)
                printf("%s\n", *pj);

            printf("\n");
        }
    }

    parser_free(&parser);
}