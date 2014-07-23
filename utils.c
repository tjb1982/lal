#include "utils.h"

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
lal_create_entry()
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
            entry = entry->next = lal_create_entry();
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
lal_create_body_part()
{
    struct lal_body_part *part = (struct lal_body_part *)malloc(sizeof(struct lal_body_part));
    part->part = NULL;
    part->len = 0;
    part->next = NULL;
    part->prev = NULL;
    return part;
}

struct lal_body_part *
lal_append_to_body(struct lal_body_part *part, const char *src)
{
    if (part->len) {
        while (part->next) {
            part = part->next;
        }
	part->next = lal_create_body_part();
	part->next->prev = part;
	part = part->next;
    }

    part->len = strlen(src);

    part->part = (char *)malloc((strlen(src) + 1) * sizeof(char));
    strcpy(part->part, src);

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
		part->next = lal_create_body_part();
		part->next->prev = part;
		part = part->next;
	}

	part->len = len;

	part->part = malloc(len + 1);
	memcpy(part->part, src, len);
	part->part[len] = '\0';

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
		part->prev = lal_create_body_part();
		part->prev->next = part;
		part = part->prev;
	}
	part->len = strlen(src);

	part->part = (char *)malloc((strlen(src) + 1) * sizeof(char));
	strcpy(part->part, src);
	
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
	int len = 0, count = 0, total_len = 0;
	const char *sep = separator ? separator : "";
	char *ptr, *body;
	struct lal_body_part *part = body_part;

	while (part) {
		count++;
		len += part->len;
		part = part->next;
	}

	total_len = len + (--count * (strlen(sep)));
	ptr = body = (char *)calloc(total_len + 1, sizeof(char));
	*ptr = '\0';

	part = body_part;
	while (part) {
		if (part->part) {
			memcpy(ptr, part->part, part->len);
			ptr += part->len;
			if (part->next) {
				ptr = (char *)memcpy(ptr, sep, strlen(sep));
				ptr += strlen(sep);
			}
		}
		part = part->next;
	}
	body[total_len] = '\0';
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
        free(body->part);
        prev = body;
        body = body->next;
        free(prev);
        i++;
    }
}
