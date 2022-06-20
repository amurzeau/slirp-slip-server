// SPDX-License-Identifier: MIT

#include "glib.h"
#include <stdarg.h>
#include <ctype.h>

unsigned int g_strv_length(char **str_array)
{
    unsigned int i = 0;

    if (str_array == NULL) {
        return 0;
    }

    while (str_array[i])
        ++i;

    return i;
}

GRand *g_rand_new()
{
    return NULL;
}

int g_rand_int_range(GRand *grand, int begin, int end)
{
    return (rand() % (end - begin)) + begin;
}

int g_str_has_prefix(const char *from, const char *prefix)
{
    return strncmp(from, prefix, strlen(prefix)) == 0;
}


GString *g_string_new(void *arg)
{
    return calloc(sizeof(GString), 1);
}

void g_string_append_printf(GString *str, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    str->size += vsnprintf(str->data + str->size, sizeof(str->data) - str->size,
                           format, args);
    va_end(args);
}

char *g_string_free(GString *str, int free_segment)
{
    char *ret = NULL;
    if (free_segment)
        ret = strdup(str->data);
    free(str);
    return ret;
}


gchar *g_strstr_len(const gchar *haystack, size_t haystack_len,
                    const gchar *needle)
{
    g_return_val_if_fail(haystack != NULL, NULL);
    g_return_val_if_fail(needle != NULL, NULL);

    if (haystack_len < 0)
        return strstr(haystack, needle);
    else {
        const gchar *p = haystack;
        gsize needle_len = strlen(needle);
        gsize haystack_len_unsigned = haystack_len;
        const gchar *end;
        gsize i;

        if (needle_len == 0)
            return (gchar *)haystack;

        if (haystack_len_unsigned < needle_len)
            return NULL;

        end = haystack + haystack_len - needle_len;

        while (p <= end && *p) {
            for (i = 0; i < needle_len; i++)
                if (p[i] != needle[i])
                    goto next;

            return (gchar *)p;

        next:
            p++;
        }

        return NULL;
    }
}

static gboolean debug_key_matches(const gchar *key, const gchar *token,
                                  guint length)
{
    /* may not call GLib functions: see note in g_parse_debug_string() */
    for (; length; length--, key++, token++) {
        char k = (*key == '_') ? '-' : tolower(*key);
        char t = (*token == '_') ? '-' : tolower(*token);

        if (k != t)
            return false;
    }

    return *key == '\0';
}

guint g_parse_debug_string(const gchar *string, const GDebugKey *keys,
                           guint nkeys)
{
    guint i;
    guint result = 0;

    if (string == NULL)
        return 0;

    /* this function is used during the initialisation of gmessages, gmem
     * and gslice, so it may not do anything that causes memory to be
     * allocated or risks messages being emitted.
     *
     * this means, more or less, that this code may not call anything
     * inside GLib.
     */

    if (!g_ascii_strcasecmp(string, "help")) {
        /* using stdio directly for the reason stated above */
        fprintf(stderr, "Supported debug values:");
        for (i = 0; i < nkeys; i++)
            fprintf(stderr, " %s", keys[i].key);
        fprintf(stderr, " all help\n");
    } else {
        const gchar *p = string;
        const gchar *q;
        gboolean invert = false;

        while (*p) {
            q = strpbrk(p, ":;, \t");
            if (!q)
                q = p + strlen(p);

            if (debug_key_matches("all", p, q - p)) {
                invert = true;
            } else {
                for (i = 0; i < nkeys; i++)
                    if (debug_key_matches(keys[i].key, p, q - p))
                        result |= keys[i].value;
            }

            p = q;
            if (*p)
                p++;
        }

        if (invert) {
            guint all_flags = 0;

            for (i = 0; i < nkeys; i++)
                all_flags |= keys[i].value;

            result = all_flags & (~result);
        }
    }

    return result;
}
