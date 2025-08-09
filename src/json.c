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

static void input_error(Lexer* lexer, const char* msg) {
    int line = 1; 
    int col = 1;

    for (int i = 0; i < lexer -> curr; i++) {
        if (lexer -> json[i] == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }

    printf("\e[1merror:\e[0m %s Line %d Column %d\n", msg, line, col);
}

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

static void parse_key(Lexer* lexer, Pair* pair) {
    char temp_buffer[256];
    int i = 0;

    _skip_whitespace(lexer);
    if (!IS_STRING_DELIM(lexer ->c)) {
        input_error(lexer, "Expected starting string delimiter '\"'.");
        arena_free(lexer -> arena);
        exit(1);
    }

    _lexer_advance(lexer);

    while (!IS_STRING_DELIM(lexer -> c)) {
        if (IS_IDENTIFIER(lexer -> c)) {
            input_error(lexer, "Expected closing string delimier '\"'.");
            arena_free(lexer -> arena);
            exit(1);
        }
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
        input_error(lexer, "Expected '{'.");
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
            input_error(lexer, "Exepcted '}' or ','.");
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
        input_error(lexer, "Expected indentifier, ':'.");
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

            temp_buffer[i] = '\0';
            if (strcmp(temp_buffer, "null") == 0) {
                pair -> type = JSON_NULL;
                pair -> value.str = arena_strdup(lexer -> arena, temp_buffer);
            } else {
                input_error(lexer, "Unknown type.");
                arena_free(lexer -> arena);
                exit(1);
            }
            break;
        
        case 't':
            while (!IS_DELIM(lexer -> c) && lexer -> curr < lexer -> len) {
                temp_buffer[i++] = lexer -> c;
                _lexer_advance(lexer);
            }

            temp_buffer[i] = '\0';
            if (strcmp(temp_buffer, "true") == 0) {
                pair -> type = JSON_BOOL;
                pair -> value.boolean =  true;
            } else {
                input_error(lexer, "Unknown type.");
                arena_free(lexer -> arena);
                exit(1);
            }
            break;

        case 'f':
            while (!IS_DELIM(lexer -> c) && lexer -> curr < lexer -> len) {
                temp_buffer[i++] = lexer -> c;
                _lexer_advance(lexer);
            }

            temp_buffer[i] = '\0';
            if (strcmp(temp_buffer, "false") == 0) {
                pair -> type = JSON_BOOL;
                pair -> value.boolean = false; 
            } else {
                input_error(lexer, "Unknown type.");
                arena_free(lexer -> arena);
                exit(1);
            }
            break;

        default:
            input_error(lexer, "Expected string delimiter '\"'.");
            arena_free(lexer -> arena);
            exit(1);
            break;
    }
}

static void print_json_value(Pair* pair, int indent_level) {
    for (int i = 0; i < indent_level; i++) {
        printf("    ");
    }
    
    printf("\"%s\": ", pair->key);
    
    switch (pair->type) {
        case JSON_STRING:
            printf("\"%s\"", pair->value.str);
            break;
            
        case JSON_NUMBER:
            printf("%.2f", pair->value.number);
            break;
            
        case JSON_BOOL:
            printf("%s", pair->value.boolean ? "true" : "false");
            break;
            
        case JSON_NULL:
            printf("null");
            break;
            
        case JSON_BLOCK:
            printf("{\n");
            for (int i = 0; i < pair->value.block.count; i++) {
                print_json_value(&pair->value.block.pairs[i], indent_level + 1);
                if (i < pair->value.block.count - 1) {
                    printf(",");
                }
                printf("\n");
            }
            for (int i = 0; i < indent_level; i++) {
                printf("  ");
            }
            printf("}");
            break;
            
        default:
            printf("unknown");
            break;
    }
}

void print_json(Lexer* lexer) {
    printf("{\n");
    for (int i = 0; i < lexer->count; i++) {
        print_json_value(&lexer->pairs[i], 1);
        if (i < lexer->count - 1) {
            printf(",");
        }
        printf("\n");
    }
    printf("}\n");
}

void parse_json(Arena* arena, const char* json) {
    Lexer* lexer = new_lexer(arena, json);

    _skip_whitespace(lexer);
    if (lexer -> c != '{') {
        input_error(lexer, "Expected '{'.");
        arena_free(lexer -> arena);
        exit(1);
    }
    _lexer_advance(lexer);
    _skip_whitespace(lexer);

    while (lexer -> c != '}') {
        Pair pair = {0};

        parse_key(lexer, &pair);
        parse_value(lexer, &pair);
        push_pair(lexer, pair);
        printf("Pair key: %s\n", pair.key);

        _skip_whitespace(lexer);
        if (lexer -> c == ',') {
            _lexer_advance(lexer);
            _skip_whitespace(lexer);
        } else if (lexer -> c != '}') {
            input_error(lexer, "Expected '}'.");
            arena_free(lexer -> arena);
            exit(1);
        }
    }

    printf("Parsed JSON:\n");
    print_json(lexer);
}
