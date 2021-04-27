#pragma once

#include "containers/string.h"
#include "containers/darray.h"
#include "portfolio.h"

typedef enum
{
    // Multi character tokens
    TOKEN_INDENTIFIER,        // Alphabetic values
    TOKEN_STRING,             // Enclosed with '"'s
    TOKEN_FORMATTED_STRING,   // Enclosed with '`'s

    // Single character tokens
    TOKEN_DOLLAR           = '$',
    TOKEN_L_BRACKET        = '[',
    TOKEN_R_BRACKET        = ']',
    TOKEN_L_BRACE          = '{',
    TOKEN_R_BRACE          = '}',
    TOKEN_COLON            = ':',
    TOKEN_SEMI_COLON       = ';',
    TOKEN_COMMA            = ','
} Token_Type;

typedef struct
{
    Token_Type type;
    String value;
    int lineNumber;
} Token;

typedef enum
{
    LEXER_NO_LEX,
    LEXER_FAILURE,
    LEXER_SUCCESS
} Lexer_Status;

typedef struct
{
    String contents;
    int index;
    int currentLine;
    DArray(Token) tokens;
    Lexer_Status status;
    String message;
} Lexer;

Lexer lexer_make(String contents);
void lexer_free(Lexer* lexer);
void lexer_lex(Lexer* lexer);

typedef enum
{
    PARSER_NO_PARSE,
    PARSER_FAILURE,
    PARSER_SUCCESS
} Parser_Status;

typedef struct
{
    DArray(Token) tokens;
    int current_token_idx;
    Parser_Status status;
    String message;
} Parser;

Parser parser_make(DArray(Token) tokens);
void parser_free(Parser* parser);
Portfolio parser_parse(Parser* parser);