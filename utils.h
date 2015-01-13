#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <stdint.h>
#include <unistd.h>

#ifndef UTILS_H_
#define UTILS_H_
struct lal_entry {
    char* key;
    size_t keylen;
    char *val;
    size_t vallen;
    struct lal_entry *next;
};

struct lal_body_part {
    const char *val;
    size_t len;
    struct lal_body_part *next;
    struct lal_body_part *prev;
};

enum lal_content_type {
	TEXT,
	HTML,
	JSON,
	NONE
};

const char *
lal_content_type_to_string(enum lal_content_type t);

void
lal_url_decode(char *dst, const char *src);

void
lal_parse_url_encoded_entries(struct lal_entry *entry, const char *body);

struct lal_entry *
lal_new_entry();

struct lal_entry *
lal_append_to_entries(struct lal_entry *entry, const char *key, const char *val);

char *
lal_get_entry(struct lal_entry *entry, const char *key);

struct lal_body_part *
lal_new_body_part();

struct lal_body_part *
lal_create_body_part(const char *init);

struct lal_body_part *
lal_append_to_body(struct lal_body_part *part, const char *src);

struct lal_body_part *
lal_nappend_to_body(struct lal_body_part *part, const uint8_t *src, size_t len);

struct lal_body_part *
lal_prepend_to_body(struct lal_body_part *part, const char *src);

size_t
lal_body_len(struct lal_body_part *part);

char *
lal_join_body(struct lal_body_part *body, const char *separator);

void
lal_destroy_entries(struct lal_entry *entries);

void
lal_destroy_body(struct lal_body_part *body);
#endif // UTILS_H_
