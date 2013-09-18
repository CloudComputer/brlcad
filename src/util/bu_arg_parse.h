/*                            B U . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef BU_ARG_PARSE_H
#define BU_ARG_PARSE_H

#include "bu.h"

/* all in this part of the header MUST have "C" linkage */
#ifdef __cplusplus
extern "C" {
#endif

/* using ideas from Cliff */
/* types of parse arg results */
typedef enum {
  BU_ARG_PARSE_SUCCESS = 0,
  BU_ARG_PARSE_ERR
} bu_arg_parse_result_t;

/* types of arg values */
typedef enum {
  BU_ARG_BOOL,
  BU_ARG_DOUBLE,
  BU_ARG_LONG,
  BU_ARG_STRING
} bu_arg_valtype_t;

/* TCLAP arg types */
typedef enum {
  BU_ARG_MultiArg,
  BU_ARG_MultiSwitchArg,
  BU_ARG_SwitchArg,
  BU_ARG_UnlabeledMultiArg,
  BU_ARG_UnlabeledValueArg,
  BU_ARG_ValueArg
} bu_arg_t;

typedef enum {
  BU_ARG_NOT_REQUIRED = 0,
  BU_ARG_REQUIRED = 1
} bu_arg_req_t;

typedef struct bu_arg_value {
  bu_arg_valtype_t typ;
  union u_typ {
    /* important that first field is integral type */
    long l; /* also use as bool */
    bu_vls_t s;
    double d;
  } u;
} bu_arg_value_t;

/* TCLAP::Arg */
typedef struct bu_arg_vars_type {
  bu_arg_t arg_type;        /* enum: type of TCLAP arg                   */
  bu_vls_t flag;            /* the "short" option, may be empty ("")     */
  bu_vls_t name;            /* the "long" option                         */
  bu_vls_t desc;            /* a brief description                       */
  bu_arg_req_t req;         /* bool: is arg required?                    */
  bu_arg_req_t valreq;      /* bool: is value required?                  */
  bu_arg_value_t val;       /* type plus union: can hold all value types */
} bu_arg_vars;

/* initialization */
bu_arg_vars *
bu_arg_SwitchArg(const char *flag,
                 const char *name,
                 const char *desc,
                 const char *def_val);
bu_arg_vars *
bu_arg_UnlabeledValueArg(const char *name,
                         const char *desc,
                         const char *def_val,
                         const bu_arg_req_t required,
                         const bu_arg_valtype_t val_typ);
/* the getters */
long bu_arg_get_bool_value(bu_arg_vars *arg);
long bu_arg_get_long_value(bu_arg_vars *arg);
double bu_arg_get_double_value(bu_arg_vars *arg);
const char *bu_arg_get_string_value(bu_arg_vars *arg);

/* the action: all in one function */
int bu_arg_parse(bu_ptbl_t *args, int argc, char * const argv[]);

/* free arg memory for any strings */
void bu_arg_free(bu_ptbl_t *args);

/* all in this header MUST have "C" linkage */
#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* BU_ARG_PARSE_H */