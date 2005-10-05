#ifndef UTIL_H
#define UTIL_H

#include "pset.h"
#include "defs.h"

void out_of_memory(const char *func);
const struct name_value *nv_find_value(const struct name_value nv_array[],const char *name);
const struct name_value *nv_find_name(const struct name_value nv_array[],int value);
const char *nv_get_name(const struct name_value nv_array[],int value);
char **argv_alloc(unsigned count);
status_e copy_pset(const pset_h from,pset_h *to,unsigned size);
void no_control_tty(void);
status_e write_buf(int fd,const char *buf,int len);
void tabprint(int fd, int tab_level, const char *fmt, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 3, 4)));
#else
 ;
#endif
void drain(int sd);
int parse_int(const char *, int , int , int *);
int parse_uint(const char *, int , int , unsigned int *);
int parse_ull(const char *, int , int , unsigned long long *);
int parse_base10(const char *, int *);
int parse_ubase10(const char *, unsigned int *);
bool_int parse_all_digits(const char *ptr);

#endif
