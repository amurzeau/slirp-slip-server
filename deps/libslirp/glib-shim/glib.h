#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define G_GNUC_PRINTF(...)
#define G_STATIC_ASSERT(...)
#define g_assert_not_reached(...) assert(0 && "unreachable")
#define g_assert(...)
#define G_UNLIKELY(...) __VA_ARGS__
#define g_malloc(...) malloc(__VA_ARGS__)
#define g_malloc0(size) calloc(size, 1)
#define g_realloc(...) realloc(__VA_ARGS__)
#define g_free(...) free(__VA_ARGS__)
#define g_debug(...) do { printf(__VA_ARGS__); puts(""); } while(0)
#define g_warning(...) do { printf(__VA_ARGS__); puts(""); } while(0)
#define g_error(...) do { printf(__VA_ARGS__); puts(""); } while(0)
#define g_critical(...) do { printf(__VA_ARGS__); puts(""); } while(0)
#define g_getenv(...) getenv(__VA_ARGS__)
#define g_strdup(...) strdup(__VA_ARGS__)
#define g_snprintf(...) snprintf(__VA_ARGS__)
#define g_vsnprintf(...) vsnprintf(__VA_ARGS__)
#define g_ascii_strcasecmp(...) stricmp(__VA_ARGS__)
#define g_strerror(...) strerror(__VA_ARGS__)
#define MIN(a, b) (((a) <= (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define G_N_ELEMENTS(a) (sizeof(a) / sizeof((a)[0]))
#define g_return_if_fail(...) \
    do {                      \
        if (!(__VA_ARGS__))   \
            return;           \
    } while (0)
#define g_return_val_if_fail(cond, value) \
    do {                                  \
        if (!(cond))                      \
            return (value);               \
    } while (0)
#define g_warn_if_reached()
#define g_warn_if_fail(...)
#define g_new(size, count) malloc(sizeof(size) * (count))
#define g_new0(size, count) calloc(sizeof(size), count)

#define GUINT16_FROM_BE(a) (uint16_t)(htons(a))
#define GUINT16_TO_BE(a) (uint16_t)(htons(a))
#define GINT16_FROM_BE(a) (int16_t)(htons(a))
#define GINT16_TO_BE(a) (int16_t)(htons(a))
#define GUINT32_FROM_BE(a) (uint32_t)(htonl(a))
#define GUINT32_TO_BE(a) (uint32_t)(htons(a))
#define GINT32_FROM_BE(a) (int32_t)(htonl(a))
#define GINT32_TO_BE(a) (int32_t)(htonl(a))

#define G_LITTLE_ENDIAN 1234
#define G_BIG_ENDIAN 4321

typedef int GRand;
typedef char **GStrv;
typedef void *gpointer;
typedef int gint;
typedef int guint;
typedef bool gboolean;
typedef char gchar;
typedef size_t gsize;

#define GLIB_CHECK_VERSION(...) 1

typedef struct {
    const char *key;
    int value;
} GDebugKey;

unsigned int g_strv_length(char **str_array);

GRand *g_rand_new();
#define g_rand_free(...)

int g_rand_int_range(GRand *grand, int begin, int end);

int g_str_has_prefix(const char *from, const char *prefix);
guint g_parse_debug_string(const gchar *string, const GDebugKey *keys,
                           guint nkeys);

typedef struct {
    char data[65536];
    int size;
} GString;

GString *g_string_new(void *arg);

void g_string_append_printf(GString *str, const char *format, ...);

char *g_string_free(GString *str, int something);


gchar *g_strstr_len(const gchar *haystack, size_t haystack_len,
                    const gchar *needle);
