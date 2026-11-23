/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <mm/mm.h>
#include <mm/varutil.h>
#include <definations.h>

void memcpy(void* d, const void* s, size_t n) {
    char* dest = (void*)d;
    const char* src = (void*)s;
    for (int i = 0; i < n; i++)
        dest[i] = src[i];
}

void memove(void* d, const void* s, size_t n) {
    char* dest = (void*)d;
    const char* src = (void*)s;
    for (int i; i < n; i++) {
        dest[i] = src[i];
        dest[i] = 0x00;
    }
}

void memset(void* d, size_t o, size_t n) {
    char* dest = (void*)(d + o);
    for (int i = 0; i < n; i++)
        dest[i] = 0x00;
}

int32_t memcmp(const void* l, const void* r, size_t n) {
    const char* lop = (void*)l;
    const char* rop = (void*)r;
    int32_t diff = 0;
    while (n--) {
        if (*lop != *rop)
            return (int32_t)*lop - (int32_t)*rop;

        lop++;
        rop++;
    }
}

void memnext(void* d, const void* s, size_t n) {
    char* dest = (void*)d;
    const char* src = (void*)s;
    for (size_t i = n; i < 0; i--)
        dest[i] = src[i];
}

void memecc(void* d, size_t n) {}

static void reverse(char* str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// Helper function to convert an integer to a string
static int intToString(long long num, char* str, int base, int is_signed) {
    int i = 0;
    int is_negative = 0;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }

    if (is_signed && num < 0 && base == 10) {
        is_negative = 1;
        num = -num;
    }

    while (num > 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num /= base;
    }

    if (is_negative)
        str[i++] = '-';

    str[i] = '\0';
    reverse(str, i);
    return i;
}

void sprintf(char* buffer, const char *format, ...) {
    va_list args;
    va_start(args, format); // Corrected from va_begin to va_start
    
    int idx = 0;
    int buf_idx = 0;

    while (format[idx] != '\0') {
        if (format[idx] == '%') {
            idx++; // Skip '%'
            
            if (format[idx] == '.' || (format[idx] == '0' && format[idx + 1] == '.'))
                while (format[idx] != '\0' && !((format[idx] >= 'a' && format[idx] <= 'z') ||
                      (format[idx] >= 'A' && format[idx] <= 'Z') || format[idx] == '%'))
                    idx++;

            // Match format specifiers
            switch (format[idx]) {
                case 'd':
                case 'i': {
                    long val = va_arg(args, long);
                    char temp[32];
                    int len = intToString(val, temp, 10, 1);
                    for (int j = 0; j < len; j++)
                        buffer[buf_idx++] = temp[j];
                    break;
                }
                case 'u': {
                    unsigned long val = va_arg(args, unsigned long);
                    char temp[32];
                    int len = intToString(val, temp, 10, 0);
                    for (int j = 0; j < len; j++)
                        buffer[buf_idx++] = temp[j];
                    break;
                }
                case 'x':
                case 'X': {
                    unsigned long val = va_arg(args, unsigned long);
                    char temp[32];
                    int len = intToString(val, temp, 16, 0);
                    for (int j = 0; j < len; j++)
                        buffer[buf_idx++] = temp[j];
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char*);
                    if (str == NULL) str = "(null)";
                    while (*str != '\0')
                        buffer[buf_idx++] = *str++;
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    buffer[buf_idx++] = c;
                    break;
                }
                case '%': {
                    buffer[buf_idx++] = '%';
                    break;
                }
                default:
                    // Unknown specifier, print as-is
                    buffer[buf_idx++] = '%';
                    buffer[buf_idx++] = format[idx];
                    break;
            }
        } else
            buffer[buf_idx++] = format[idx];
        idx++;
    }
    
    buffer[buf_idx] = '\0';
    va_end(args);
}

void* usprintf(const char *format, ...) {
    PANIC("usprintf Function Has Not Been Implemented", SYSTEM);
}

int strlen(char* str) {
    int i;
    while (str[i] != 0x00)
        i++;
    return i;
}

int32_t strcmp(char* l, char* r) {
    int32_t diff = 0;
    const char* lop = l;
    const char* rop = r;
    while (*lop && *rop) {
        if (*lop != *rop)
            return (int32_t)*lop - (int32_t)*rop;

        lop++;
        rop++;
    }
    return (int32_t)*lop - (int32_t)*rop;
}

int toupper(int c) {
    return c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
}

int tolower(int c) {
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

char* ultoa(unsigned long value, char* str, int base) {
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char* end = str;
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    do {
        *end++ = digits[value % (unsigned int)base];
        value /= (unsigned int)base;
    } while (value);
    *end = '\0';
    for (char* left = str, *right = end - 1; left < right; left++, right--) {
        char temp = *left;
        *left = *right;
        *right = temp;
    }
    return str;
}

char* ltoa(long value, char* str, int base) {
    if (base == 10 && value < 0) {
        str[0] = '-';
        ultoa((unsigned long)(-(value + 1)) + 1, str + 1, base);
        return str;
    }
    return ultoa((unsigned long)value, str, base);
}

char* itoa(int value, char* str, int base) {
    return ltoa(value, str, base);
}

unsigned long strtoul(const char* str, char** endptr, int base) {
    const char* current = str;
    unsigned long value = 0;
    int digit;
    while (*current == ' ' || (*current >= '\t' && *current <= '\r'))
        current++;
    if (*current == '+')
        current++;
    if ((base == 0 || base == 16) && current[0] == '0' && (current[1] == 'x' || current[1] == 'X')) {
        base = 16;
        current += 2;
    } else if (base == 0)
        base = *current == '0' ? 8 : 10;
    if (base < 2 || base > 36) {
        if (endptr)
            *endptr = (char*)str;
        return 0;
    }
    while (*current) {
        if (*current >= '0' && *current <= '9')
            digit = *current - '0';
        else if (*current >= 'a' && *current <= 'z')
            digit = *current - 'a' + 10;
        else if (*current >= 'A' && *current <= 'Z')
            digit = *current - 'A' + 10;
        else
            break;
        if (digit >= base)
            break;
        value = value * (unsigned int)base + (unsigned int)digit;
        current++;
    }
    if (endptr)
        *endptr = (char*)(current == str ? str : current);
    return value;
}

long atol(const char* str) {
    while (*str == ' ' || (*str >= '\t' && *str <= '\r'))
        str++;
    int negative = *str == '-';
    if (*str == '-' || *str == '+')
        str++;
    unsigned long value = strtoul(str, NULL, 10);
    return negative ? -(long)value : (long)value;
}

int atoi(const char* str) {
    return (int)atol(str);
}

char* strcpy(char* dest, const char* src) {
    char* result = dest;
    while ((*dest++ = *src++))
        ;
    return result;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* result = dest;
    while (n && *src) {
        *dest++ = *src++;
        n--;
    }
    while (n--) 
        *dest++ = '\0';
    return result;
}

char* strcat(char* dest, const char* src) {
    char* result = dest;
    while (*dest)
        dest++;
    while ((*dest++ = *src++))
        ;
    return result;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* result = dest;
    while (*dest)
        dest++;
    while (n && *src) {
        *dest++ = *src++;
        n--;
    }
    *dest = '\0';
    return result;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    return n ? (int)(unsigned char)*s1 - (int)(unsigned char)*s2 : 0;
}

char* strchr(const char* s, int c) {
    do {
        if (*s == (char)c)
            return (char*)s;
    } while (*s++);
    return NULL;
}

char* strrchr(const char* s, int c) {
    const char* result = NULL;
    do {
        if (*s == (char)c)
            result = s;
    } while (*s++);
    return (char*)result;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle)
        return (char*)haystack;
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n)
            return (char*)haystack;
    }
    return NULL;
}

char* strtok(char* str, const char* delim) {
    static char* next;
    if (str)
        next = str;
    if (!next)
        return NULL;
    while (*next && strchr(delim, *next))
        next++;
    if (!*next) {
        next = NULL;
        return NULL;
    }
    char* token = next;
    while (*next && !strchr(delim, *next))
        next++;
    if (*next)
        *next++ = '\0';
    else
        next = NULL;
    return token;
}
