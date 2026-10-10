/* Test-only allocation injection for owned extraction transactions. */
#include <stdlib.h>
void *n9_test_malloc(size_t);
void *n9_test_calloc(size_t, size_t);
void *n9_test_realloc(void *, size_t);
#define malloc n9_test_malloc
#define calloc n9_test_calloc
#define realloc n9_test_realloc
#include "asn1typed_extract.c"
