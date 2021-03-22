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

    Parser parser = parser_make(lexer.tokens);
    Portfolio portfolio = parser_parse(&parser);

    template_parser_test(portfolio);

    parser_free(&parser);
    portfolio_free(&portfolio);
}