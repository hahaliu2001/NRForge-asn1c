#include <stdlib.h>
void *n11_test_malloc(size_t);
void *n11_test_calloc(size_t, size_t);
void *n11_test_realloc(void *, size_t);
#define malloc n11_test_malloc
#define calloc n11_test_calloc
#define realloc n11_test_realloc
#include "asn1typed_extract.c"
