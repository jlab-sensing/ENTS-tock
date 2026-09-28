#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

char* __cxa_demangle(const char* mangled_name, char* output_buffer,
                      size_t* length, int* status) {
    if (status) *status = -1; // signal "demangling failed"
    return NULL;              // caller falls back to printing the mangled name
}

#ifdef __cplusplus
}
#endif
