#ifndef JSON_H
#define JSON_H

#include "arena.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IS_NUMBER(c) (char_map[(char)(c)] & 1)
#define IS_ALPHA(c) (char_map[(char)(c)] & 2)
#define IS_WHITESPACE(c) (char_map[(char)(c)] & 4)
#define IS_STRING_DELIM(c) (char_map[(char)(c)] & 8)
#define IS_IDENTIFIER(c) (char_map[(char)(c)] & 16)
#define IS_DELIM(c) (char_map[(char)(c)] & 32)

/*
*
*  String, number, bool, block, array
*  key is always a string and always has value
*
*/

typedef enum {
    JSON_STRING,
    JSON_NUMBER,
    JSON_BLOCK,
    // JSON_ARRAY,      todo: add array support 
    JSON_BOOL,   
    JSON_NULL    
} JsonType;

typedef struct Pair Pair;

typedef struct {
    const char* ptr;
    size_t len;
} JsonString;

typedef struct {
    Pair* pairs;
    size_t count;
    size_t capacity;
} Block;

typedef struct Pair {
    JsonType type;
    JsonString key;
    union {
        double number;
        JsonString str;
        Block block;
        bool boolean;
    } value;
} Pair;

typedef struct {
    const char* json;
    Arena* arena;
    Pair* pairs;
    size_t count;
    size_t capacity;
    size_t curr;
    size_t len;
    char c;
} Lexer;

typedef struct {
    Pair* pairs;
    size_t count;
} Json;

Json parse_json(Arena* arena, const char* json);
void print_json(Json* json);

#endif // !JSON_H
