#include "json.h"
#include "arena.h"
#include "ylib.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t char_map[256] = {
    ['0' ... '9'] = 1,

    ['a' ... 'z'] = 2,
    ['A' ... 'Z'] = 2,
    ['_'] = 2,

    [' '] = 4, 
    ['\t'] = 4, 
    ['\n'] = 4,

    ['"'] = 8,

    [':'] = 16,

    [','] = 32,
    [']'] = 32,
    ['}'] = 32,
};

static void parse_value(Lexer* lexer, Pair* pair); 

static Lexer* new_lexer(Arena* arena, const char* json) {
    Lexer* lexer = arena_alloc(arena, sizeof(*lexer));

    lexer -> arena = arena;
    lexer -> json = arena_strdup(arena, json);
    lexer -> capacity = 16;
    lexer -> count = 0;
    lexer -> pairs = arena_array(arena, Pair, lexer -> capacity);
    lexer -> len = _strlen(lexer -> json);
    lexer -> curr = 0;
    lexer -> c = lexer -> json[0];

    return lexer;
}

static void push_pair(Lexer* lexer, Pair pair) {
    size_t size = sizeof(Pair);

    if (lexer -> count >= lexer -> capacity) {
        lexer -> pairs = arena_realloc(lexer -> arena, lexer -> pairs, size * lexer -> capacity, size * lexer -> capacity * 2);
        if (!lexer -> pairs) {
            arena_free(lexer -> arena);
            ERROR("Realloc of lexer -> pairs failed!");
            exit(1);
        }
        lexer -> capacity *= 2;
    }

    lexer -> pairs[lexer -> count++] = pair;
} 

static void push_pair_to_block(Arena* arena, Block* block, Pair pair) {
    size_t size = sizeof(Pair);

    if (block -> count >= block -> capacity) {
        block -> pairs = arena_realloc(arena, block -> pairs, size * block -> capacity, size * block -> capacity * 2);
        if (!block -> pairs) {
            arena_free(arena);
            ERROR("Realloc of block -> pairs failed!");
            exit(1);
        }
        block -> capacity *= 2;
    }

    block -> pairs[block -> count++] = pair;
}

static inline void _lexer_advance(Lexer* lexer) {
    if (lexer -> curr >= lexer -> len) {
        lexer -> c= '\0';
        return;
    }

    lexer -> curr++;
    if (lexer -> curr >= lexer -> len) {
        lexer -> c = '\0';
    } else {
        lexer -> c = lexer -> json[lexer -> curr];
    }
}

static inline void _skip_whitespace(Lexer* lexer) {
    while (IS_WHITESPACE(lexer -> c)) {
        if (lexer -> curr >= lexer -> len) return;
        _lexer_advance(lexer);
    }
}

static inline void _consume_delimiter(Lexer* lexer) {
    _skip_whitespace(lexer);
    if (IS_DELIM(lexer -> c)){
        _lexer_advance(lexer);
        _skip_whitespace(lexer);
    }
}

static void parse_key(Lexer* lexer, Pair* pair) {
    char temp_buffer[256];
    int i = 0;

    _skip_whitespace(lexer);
    if (!IS_STRING_DELIM(lexer ->c)) {
        INPUT_ERROR("Could not find starting string delimiter!");
        arena_free(lexer -> arena);
        exit(1);
    }

    _lexer_advance(lexer);

    while (!IS_STRING_DELIM(lexer -> c)) {
        temp_buffer[i++] = lexer -> c; 
        _lexer_advance(lexer);
    }
    _lexer_advance(lexer);

    temp_buffer[i] = '\0';
    pair -> key = arena_strdup(lexer -> arena, temp_buffer);
}

static Block parse_block(Lexer* lexer) {
    Block block = {0};
    block.count = 0;
    block.capacity = 8;
    block.pairs = arena_array(lexer -> arena, Pair, block.capacity);

    _skip_whitespace(lexer);
    if (lexer -> c != '{') {
        INPUT_ERROR("Expected '{'!");
        arena_free(lexer -> arena);
        exit(1);
    }
    _lexer_advance(lexer);

    _skip_whitespace(lexer);
    while (lexer -> c != '}') {
        Pair pair = {0};
        parse_key(lexer, &pair);
        parse_value(lexer, &pair);
        push_pair_to_block(lexer -> arena, &block, pair);

        _skip_whitespace(lexer);
        if (lexer -> c == ',') {
            _lexer_advance(lexer);
            _skip_whitespace(lexer);
        } else if (lexer -> c != '}') {
            INPUT_ERROR("Exepcted '}' or ','!");
            arena_free(lexer -> arena);
            exit(1);
        }
    }

    _lexer_advance(lexer);
    return block;
}

static void parse_value(Lexer* lexer, Pair* pair) {
    char temp_buffer[256];
    int i = 0;

    _skip_whitespace(lexer);
    if (!IS_IDENTIFIER(lexer -> c)) {
        INPUT_ERROR("Expected indentifier, ':'!");
        arena_free(lexer -> arena);
        exit(1);
    }
    _lexer_advance(lexer);
    _skip_whitespace(lexer);

    if (IS_STRING_DELIM(lexer -> c)) {
        JsonType type = JSON_STRING;
        _lexer_advance(lexer);

        while (!IS_STRING_DELIM(lexer -> c)) {
            temp_buffer[i++] = lexer -> c; 
            _lexer_advance(lexer);
        }
        _lexer_advance(lexer);
        _consume_delimiter(lexer);

        temp_buffer[i] = '\0';
        pair -> type = type;
        pair -> value.str = arena_strdup(lexer -> arena, temp_buffer);
        return;
    } else if (IS_NUMBER(lexer -> c)) {
        JsonType type = JSON_NUMBER;

        while (!IS_DELIM(lexer -> c) && lexer -> curr < lexer -> len) {
            temp_buffer[i++] = lexer -> c;
            _lexer_advance(lexer);
        }
        _lexer_advance(lexer);
        _consume_delimiter(lexer);

        temp_buffer[i] = '\0';
        pair -> type = type;
        pair -> value.number = atof(temp_buffer);
        return;
    } 

    switch (lexer -> c) {
        case '{':
            pair -> type = JSON_BLOCK;
            pair -> value.block = parse_block(lexer);
            break;

        case 'n':
            while (!IS_DELIM(lexer -> c) && lexer -> curr < lexer -> len) {
                temp_buffer[i++] = lexer -> c;
                _lexer_advance(lexer);
            }
            _lexer_advance(lexer);
            _consume_delimiter(lexer);

            temp_buffer[i] = '\0';
            if (strcmp(temp_buffer, "null") == 0) {
                pair -> type = JSON_NULL;
                pair -> value.str = arena_strdup(lexer -> arena, temp_buffer);
            } else {
                INPUT_ERROR("Unknown type");
                arena_free(lexer -> arena);
                exit(1);
            }
            break;
        
        case 't':
            while (!IS_DELIM(lexer -> c) && lexer -> curr < lexer -> len) {
                temp_buffer[i++] = lexer -> c;
                _lexer_advance(lexer);
            }
            _lexer_advance(lexer);
            _consume_delimiter(lexer);

            temp_buffer[i] = '\0';
            if (strcmp(temp_buffer, "true") == 0) {
                pair -> type = JSON_BOOL;
                pair -> value.boolean =  true;
            } else {
                INPUT_ERROR("Unknown type");
                arena_free(lexer -> arena);
                exit(1);
            }
            break;

        case 'f':
            while (!IS_DELIM(lexer -> c) && lexer -> curr < lexer -> len) {
                temp_buffer[i++] = lexer -> c;
                _lexer_advance(lexer);
            }
            _lexer_advance(lexer);
            _consume_delimiter(lexer);

            temp_buffer[i] = '\0';
            if (strcmp(temp_buffer, "false") == 0) {
                pair -> type = JSON_BOOL;
                pair -> value.boolean = false; 
            } else {
                INPUT_ERROR("Unknown type");
                arena_free(lexer -> arena);
                exit(1);
            }
            break;
    }
}

void parse_json(Arena* arena, const char* json) {
    Lexer* lexer = new_lexer(arena, json);

    if (lexer -> c == '{') {
        _lexer_advance(lexer);
    }

    while (lexer -> curr < lexer -> len) {
        Pair pair;

        parse_key(lexer, &pair);
        parse_value(lexer, &pair);
        push_pair(lexer, pair);
        printf("Pair key: %s\n", pair.key);
    }
}
