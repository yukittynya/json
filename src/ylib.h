#ifndef YLIB_H
#define YLIB_H

#include <stdio.h>

#define ERROR(msg) printf("\e[1merror:\e[0m %s in %s at %d\n", (msg), __FILE__, __LINE__)
#define INPUT_ERROR(msg) printf("\e[1merror:\e[0m %s\n", (msg))
#define TODO(msg) printf("\e[1mtodo:\e[0m %s in %s at %d\n", (msg), __FILE__, __LINE__)

#endif // !YLIB_H
