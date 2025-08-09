#include "../src/arena.h"
#include "../src/json.h"
#include "../src/ylib.h"

#include <stddef.h>
#include <stdio.h>

static Arena arena = {0};

int main() {
    FILE* fptr = fopen("test/test.json", "r");

    if (!fptr) {
        return 1;
    }

    fseek(fptr, 0, SEEK_END);
    size_t len = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);

    char* buffer = arena_alloc(&arena, len + 1);

    if (!buffer) {
        ERROR("Alloc of json failed!");
        arena_free(&arena);
        return 1;
    }

    fread(buffer, 1, len, fptr);
    fclose(fptr);
    buffer[len] = '\0';

    Json json = parse_json(&arena, buffer);
    print_json(&json);

    arena_free(&arena);
}
