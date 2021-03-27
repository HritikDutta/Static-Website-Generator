#include <stdio.h>

#include "generator/filestuff.h"
#include "generator/parser.h"
#include "generator/portfolio.h"
#include "generator/webpage.h"

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

    if (lexer.status == LEXER_FAILURE)
    {
        printf("%s\n", lexer.message);
        return 1;
    }

    Parser parser = parser_make(lexer.tokens);
    Portfolio portfolio = parser_parse(&parser);

    if (parser.status == PARSER_FAILURE)
    {
        printf("%s\n", parser.message);
        return 1;
    }

    // template_parser_test(portfolio);

    Webpage_Status status = generate_webpages(portfolio);
    switch (status)
    {
        case WP_MISSING_TEMPLATE:
        {
            printf("Template not found.\n");
        } break;

        case WP_WRITE_ERROR:
        {
            printf("Couldn't write to file.\n");
        } break;

        case WP_TEMPLATE_ERROR:
        {
            printf("Error generating webpage.\n");
        } break;
    }

    parser_free(&parser);
    portfolio_free(&portfolio);
}