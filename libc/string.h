#ifndef STRING_H
#define STRING_H

// In a freestanding environment, define size_t using GCC's built-in type
#ifndef __SIZE_TYPE__
typedef unsigned long size_t;
#else
typedef __SIZE_TYPE__ size_t;
#endif

// Function prototypes
size_t strlen(const char* str);
char *strcat(char *dest, const char *src);
char *strcpy(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

#endif