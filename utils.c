#include "utils.h"

const char *
lal_content_type_to_string(enum lal_content_type t)
{
	char *ct = t == TEXT ? "text/plain"
	: t == HTML ? "text/html"
	: t == JSON ? "application/json"
	: NULL;
	int len = strlen(ct);
	char *ret = malloc(sizeof(char) * len + 1);
	memcpy(ret, ct, len);
	*(ret + len) = '\0';
	return ret;
}

void
lal_url_decode(char *dst, const char *src)
{
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a'-'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a'-'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

void
lal_parse_url_encoded_entries(struct lal_entry *entry, const char *buffer)
{
    int content_length = strlen(buffer);
    char body[content_length + 1];
    char *keyptr, *valptr;
    char *key, *val;

    lal_url_decode(body, buffer);

    keyptr = valptr = body;
    while (keyptr - body < content_length) {
        key = keyptr;

        while (*valptr != '=' && *valptr != '\0')
            valptr++;
        if (!*valptr)
            break;
        *valptr++ = '\0';
        val = valptr;

        while (*keyptr != '&' && (keyptr - body < content_length))
            keyptr++;
        *keyptr++ = '\0';

        valptr = keyptr;

        lal_append_to_entries(entry, key, val);
    }
}

struct lal_entry *
lal_new_entry()
{
    struct lal_entry *entry = (struct lal_entry *)malloc(sizeof(struct lal_entry));
    entry->key = NULL;
    entry->keylen = 0;
    entry->val = NULL;
    entry->vallen = 0;
    entry->next = NULL;
    return entry;
}

struct lal_entry *
lal_append_to_entries(struct lal_entry *entry, const char *key, const char *val)
{
    if (entry->keylen) {
        while (entry->next) {
            if (!strcmp(entry->key, key))
                break;
            entry = entry->next;
        }
        if (strcmp(entry->key, key))
            entry = entry->next = lal_new_entry();
        else {
            free(entry->key);
            free(entry->val);
        }
    }

    entry->keylen = strlen(key);
    entry->key = malloc(sizeof(char) * (entry->keylen + 1));
    strcpy(entry->key, key);

    entry->vallen = strlen(val);
    entry->val = malloc(sizeof(char) * (entry->vallen + 1));
    strcpy(entry->val, val);

    return entry;
}

char *
lal_get_entry(struct lal_entry *entry, const char *key)
{
    while (entry && entry->key) {
        if (!strcmp(entry->key, key))
            return entry->val;
        entry = entry->next;
    }
    return NULL;
}

struct lal_body_part *
lal_new_body_part()
{
	struct lal_body_part *part = (struct lal_body_part *) malloc(sizeof(struct lal_body_part));
	part->val = NULL;
	part->len = 0;
	part->next = NULL;
	part->prev = NULL;
	return part;
}

struct lal_body_part *
lal_create_body_part(const char *init)
{
	struct lal_body_part *part = (struct lal_body_part *) malloc(
		sizeof(struct lal_body_part)
	);
	int len = strlen(init);
	char *pt = malloc(sizeof(char) * len);

	memcpy(pt, init, len);

	part->val = pt;
	part->len = len;
	part->next = NULL;
	part->prev = NULL;
	return part;
}

struct lal_body_part *
lal_append_to_body(struct lal_body_part *part, const char *src)
{
    	while (part->next) {
    	    part = part->next;
    	}
    	part->next = lal_create_body_part(src);
    	part->next->prev = part;
    	part = part->next;
	part->next = NULL;

	return part;
}

struct lal_body_part *
lal_nappend_to_body(struct lal_body_part *part, const uint8_t *src, size_t len)
{
	if (part->len) {
		while (part->next) {
			part = part->next;
		}
		part->next = lal_new_body_part();
		part->next->prev = part;
		part = part->next;
	}

	part->len = len;
	part->val = malloc(len);
	memcpy((void *)part->val, src, len);
	part->next = NULL;

	return part;
}

struct lal_body_part *
lal_prepend_to_body(struct lal_body_part *part, const char *src)
{
	if (part->len) {
		while (part->prev) {
			part = part->prev;
		}
		part->prev = lal_create_body_part(src);
		part->prev->next = part;
		part = part->prev;
	}
	part->prev = NULL;

	return part;
}

size_t
lal_body_len(struct lal_body_part *part)
{
    int len = 0;
    while (part) {
        len += part->len;
        part = part->next;
    }
    return len;
}

char *
lal_join_body(
	struct lal_body_part *body_part,
	const char *separator)
{
	/*
	 * Here `separator` needs to be converted into a `char *` in 
	 * order to calculate its length, etc. using `string.h` tools.
	 * */
	const char *sep = separator ? separator : "";
	int len = 0, count = 0, seplen = strlen(sep);
	char *ptr, *body;
	struct lal_body_part *part = body_part;

	/* 
	 * Add each `part->len` to get the total memory required
	 * to concatenate all of the strings.
	 * */
	while (part) {
		count++;
		len += part->len;
		part = part->next;
	}

	/*
	 * The length of `separator` is multiplied by the `count -1` because we
	 * don't want a trailing `separator` at the end.
	 * */
	len = len + (--count * seplen);
	ptr = body = (char *)calloc(len + 1, sizeof(char));
	*ptr = '\0';

	/* i.e., rewind */
	part = body_part;
	while (part) {
		if (part->val) {
			memcpy(ptr, part->val, part->len);
			ptr += part->len;
			if (part->next) {
				ptr = (char *)memcpy(ptr, sep, seplen);
				ptr += seplen;
			}
		}
		part = part->next;
	}
	/* `Null`-terminated C string */
	body[len] = '\0';
	return body;
}

void
lal_destroy_entries(struct lal_entry *entry)
{
    struct lal_entry *prev;

    while (entry) {
        free(entry->key);
        free(entry->val);
        prev = entry;
        entry = entry->next;
        free(prev);
    }
}

void
lal_destroy_body(struct lal_body_part *body)
{
    struct lal_body_part *prev;
    int i = 0;

    while (body) {
        free((void *)body->val);
        prev = body;
        body = body->next;
        free(prev);
        i++;
    }
}
