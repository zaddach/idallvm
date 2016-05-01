#ifndef _IDALLVM_STRING_H
#define _IDALLVM_STRING_H

#include <string.h>

#if !defined(__cplusplus)
#include <stdbool.h>
#endif /* !defined(__cplusplus) */

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/**
 * Check if @str1 starts with @str2.
 */
static bool startswith(const char* str1, const char* str2)
{
    return strncmp(str1, str2, strlen(str2)) == 0;
}

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _IDALLVM_STRING_H */
