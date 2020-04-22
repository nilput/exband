#ifndef EXB_INI_READER_H
#define EXB_INI_READER_H
#include <ctype.h>
#include "../exb.h"
#include "../exb_str.h"
#include "../exb_utils.h"
struct ini_section {
    struct exb_str_slice name;
    struct ini_pair **entries; /*only owns the array of pointers, not the pointed to structs*/
    int len;
    int cap;
};
struct ini_pair {
    struct exb_str_slice key;
    struct exb_str_slice value;
};
struct ini_config {
    struct exb_str input; //owned
    struct ini_pair **entries; /*owned*/
    int entries_len;
    int entries_cap;
    struct ini_section *sections;
    int sections_len;
    int sections_cap;
};
static void ini_destroy(struct exb *exb, struct ini_config *c);
static int is_lws(int ch) {
    return ch == ' ' || ch == '\t';
}
static int is_alphanum(int ch) {
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') ||
           (ch >= 'A' && ch <= 'Z') ||
            ch == '_';
}
static struct ini_config *ini_parse(struct exb *exb, FILE *f) {
    struct ini_config *c = exb_malloc(exb, sizeof(struct ini_config));
    if (!c)
        return NULL;
    memset(c, 0, sizeof *c);
    c->entries = NULL;
    c->sections = NULL;
    c->sections_len = 0;
    c->entries_len  = 0;
    
    int rv = exb_str_init(exb, &c->input);
    if (rv != EXB_OK)
        goto err;
    exb_str_set_cap(exb, &c->input, 512);
    if (rv != EXB_OK)
        goto err;
    while (1) {
        int available = c->input.cap - c->input.len - 1;
        if (available == 0) {
            exb_str_set_cap(exb, &c->input, c->input.cap * 2);
            if (rv != EXB_OK)
                goto err_1;
            available = c->input.cap - c->input.len - 1;
        }
        size_t read_bytes = fread(c->input.str + c->input.len, 1, available, f);
        if (read_bytes == 0) {
            if (feof(f)) {
                c->input.str[c->input.len] = 0;
                break;
            }
            else {
                goto err_1;
            }
        }
        c->input.len += read_bytes;
    }

    c->sections_len = 1;
    c->sections_cap = 1;

    c->sections = exb_realloc(exb, c->sections, sizeof(c->sections[0]) * c->sections_cap);
    if (!c->sections) {
        goto err_1;
    }
    memset(&c->sections[0], 0, sizeof c->sections[0]);
    c->sections[0].entries = NULL;
    c->sections[0].name.index = 0;
    c->sections[0].name.len = 0;

    {
    int current_section = 0;
    int cursor = 0;
    #define cur_ch() (c->input.str[cursor])
    #define n_left() (c->input.len - cursor)
    while (cursor < c->input.len) {
        if (cur_ch() == '[') {
                int section_begin;
                int section_end;
                cursor++;
                while (n_left() && is_lws(cur_ch()))
                    cursor++;
                if ((n_left() < 1) || ((!is_alphanum(cur_ch())) && cur_ch() != ']'))
                    goto err_2;
                section_begin = cursor;
                while (is_alphanum(cur_ch()) && n_left())
                    cursor++;
                section_end = cursor;
                while (n_left() && is_lws(cur_ch()))
                    cursor++;
                if (cur_ch() != ']')
                    goto err_2;
                
                cursor++;
                while (section_end > section_begin && is_lws(c->input.str[section_end-1]))
                    section_end--;

                int duplicate = 0;
                for (int i=0; i<c->sections_len; i++) {
                    struct exb_str_slice *name = &c->sections[i].name;
                    if (exb_strl_eq(c->input.str + name->index, name->len, c->input.str + section_begin, section_end - section_begin)) {
                        duplicate = 1;
                        c->sections[i].name.index = section_begin;
                        c->sections[i].name.len   = section_end - section_begin;
                        current_section = i;
                        break;
                    }
                }
                if (!duplicate) {
                    if (c->sections_len == c->sections_cap) {
                        int newcap = c->sections_cap == 0 ? 16 : c->sections_cap * 2;
                        void *p = exb_realloc(exb, c->sections, sizeof c->sections[0] * newcap);
                        if (!p)
                            goto err_2;
                        c->sections = p;
                        c->sections_cap = newcap;
                        
                    }
                    struct ini_section *sec = &c->sections[c->sections_len];
                    sec->name.index = section_begin;
                    sec->name.len   = section_end - section_begin;
                    sec->entries = 0;
                    sec->cap = 0;
                    sec->len = 0;
                    current_section = c->sections_len;
                    c->sections_len++;
                }
                
        }
        else if (is_lws(cur_ch())) {
            cursor++;
        }
        else if (cur_ch() == '\r' || cur_ch() == '\n') {
            cursor++;
            while (n_left() && is_lws(cur_ch()))
                cursor++;
            if (n_left() && (cur_ch() == ';' || cur_ch() == '#')) {
                while (n_left() && cur_ch() != '\n') {
                    cursor++;
                }
            }
        }
        else if (is_alphanum(cur_ch())) {
            int varname_begin = cursor;
            while (n_left() && is_alphanum(cur_ch())) {
                cursor++;
            }
            int varname_end = cursor;
            
            while (n_left() && is_lws(cur_ch()))
                cursor++;
            if (cur_ch() != '=')
                goto err_2;
            cursor++;
            while (n_left() && is_lws(cur_ch()))
                cursor++;
            int value_begin = cursor;
            while (n_left() && cur_ch() != '\n') {
                cursor++;
            }
            int value_end = cursor;
            if (cur_ch() == '\n' && (value_end - value_begin > 0) && c->input.str[value_end - 1] == '\r')
                value_end--;
            struct ini_section *sec = &c->sections[current_section];
            int duplicate = 0;
            for (int i=0; i<sec->len; i++) {
                struct exb_str_slice *key = &sec->entries[i]->key;
                if (exb_strl_eq(c->input.str + key->index, key->len, c->input.str + varname_begin, varname_end - varname_begin)) {
                    duplicate = 1;
                    sec->entries[i]->value.index = value_begin;
                    sec->entries[i]->value.len   = value_end - value_begin;
                    break;
                }
            }
            if (!duplicate) {
                if (c->entries_len == c->entries_cap) {
                    int newcap = c->entries_cap == 0 ? 16 : c->entries_cap * 2;
                    void *p = exb_realloc(exb, c->entries, sizeof c->entries[0] * newcap);
                    if (!p)
                        goto err_2;
                    c->entries = p;
                    c->entries_cap = newcap;
                }
                struct ini_pair *entry = exb_malloc(exb, sizeof(struct ini_pair));;
                if (!entry)
                    goto err_2;
                c->entries[c->entries_len] = entry;
                entry->key.index = varname_begin;
                entry->key.len = varname_end - varname_begin;
                entry->value.index = value_begin;
                entry->value.len   = value_end - value_begin;
                c->entries_len++;
                
                if (sec->len == sec->cap) {
                    int newcap = sec->cap == 0 ? 16 : sec->cap * 2;
                    void *p = exb_realloc(exb, sec->entries, sizeof sec->entries[0] * newcap);
                    if (!p)
                        goto err_2;
                    sec->entries = p;
                    sec->cap = newcap;
                }
                sec->entries[sec->len] = entry;
                sec->len++;
            }
        }
        else {
            goto err_2;
        }

    }
    }
    return c;
err_2:
    ini_destroy(exb, c);
    return NULL;
err_1:
    exb_str_deinit(exb, &c->input);
err:
    exb_free(exb, c);
    return NULL;
}
static struct ini_section *ini_get_section(struct ini_config *c, const char *section_name) {
    int klen = strlen(section_name);
    for (int i=0; i<c->sections_len; i++) {
        struct ini_section *s = &c->sections[i];
        if (exb_strl_eq(c->input.str + s->name.index, s->name.len, section_name, klen)) {
            return s;
        }
    }
    return NULL;
}
static struct ini_pair *ini_get_value(struct ini_config *c, const char *key_name) {
    int klen = strlen(key_name);
    for (int i=0; i<c->entries_len; i++) {
        struct ini_pair *p = c->entries[i];
        if (exb_strl_eq(c->input.str + p->key.index, p->key.len, key_name, klen)) {
            return p;
        }
    }
    return NULL;
}
static void ini_dump(struct exb *exb, struct ini_config *c) {
    for (int i=0; i<c->sections_len; i++) {
        struct ini_section *s = &c->sections[i];
        struct exb_str tmp[2];
        for (int k=0; k<2; k++) {
            exb_str_init_empty(tmp+k);
        }
        exb_str_slice_to_copied_str(exb, s->name, c->input.str, tmp+0);
        if (s->name.len > 0)
            printf("[%s]\n", tmp[0].str);
        for (int j=0; j<s->len; j++) {
            struct ini_pair *p = s->entries[j];
            exb_str_slice_to_copied_str(exb, p->key, c->input.str, tmp+0);
            exb_str_slice_to_copied_str(exb, p->value, c->input.str, tmp+1);
            printf("%s = %s\n", tmp[0].str, tmp[1].str);
        }
        exb_str_deinit(exb, tmp+0);
        exb_str_deinit(exb, tmp+1);
    }
}

static void ini_destroy(struct exb *exb, struct ini_config *c) {
    for (int i=0; i<c->sections_len; i++) {
        struct ini_section *s = &c->sections[i];
        exb_free(exb, s->entries);
    }
    for (int i=0; i<c->entries_len; i++)
        exb_free(exb, c->entries[i]);
    exb_free(exb, c->entries);
    exb_free(exb, c->sections);
    
    exb_str_deinit(exb, &c->input);
    exb_free(exb, c);
}
#endif //EXB_INI_READER_H