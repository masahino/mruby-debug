/*
** mrb_debug.c - Debug class
**
** Copyright (c) masahino 2022
**
** See Copyright Notice in LICENSE
*/

#include <string.h>
#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/irep.h"
#include "mruby/hash.h"
#include "mruby/opcode.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/debug.h"
#include "mrb_debug.h"

const char *prev_filename = NULL;
const char *filename = NULL;
const char *bp_filename = NULL;
int32_t prev_line = -1;
int32_t prev_ciidx = 999;
int32_t bp_ciidx = 999;
int32_t line = -1;
int32_t bp_line = -1;
int32_t ciidx = 999;
int32_t prev_callinfo_size = 0;
mrb_callinfo *bp_ci = NULL;

/* Instance variable table structure */
typedef struct iv_tbl {
  int size, alloc;
  mrb_value *ptr;
} iv_tbl;

#define IV_DELETED (1UL<<31)
#define IV_KEY_P(k) (((k)&~((uint32_t)IV_DELETED))!=0)

static int
local_size(const struct mrb_irep *irep){
    return irep->nlocals;
}

volatile int
md_strcmp(const char *s1, const char *s2)
{
  return strcmp(s1, s2);
}

static mrb_int
mrb_debug_get_callinfosize(mrb_state *mrb)
{
  mrb_int len = 0;
  mrb_callinfo *ci;
  mrb_int ciidx;
  int i;
  int lineno = -1;

  ciidx = mrb->c->ci - mrb->c->cibase;

  for (i = ciidx; i >= 0; i--) {
    ci = &mrb->c->cibase[i];
    if (ci->proc == NULL || MRB_PROC_CFUNC_P(ci->proc)) {
      continue;
    } else {
      const mrb_irep *irep = ci->proc->body.irep;
      const mrb_code *pc;

      if (mrb->c->cibase[i].pc) {
        pc = mrb->c->cibase[i].pc;
      } else if (i+1 <= ciidx) {
        pc = mrb->c->cibase[i+1].pc - 1;
      } else {
        pc = ci->pc; //continue;
      }
      lineno = mrb_debug_get_line(mrb, irep, pc - irep->iseq);
    }
    if (lineno == -1) {
      continue;
    }
    len++;
  }
  return len;
}

void
mrb_break(mrb_state *mrb)
{
  bp_filename = filename;
  bp_line = line;
  bp_ci = mrb->c->ci;
  bp_ciidx = mrb_debug_get_callinfosize(mrb);
}

volatile int
mrb_check_next(mrb_state *mrb)
{
  if (filename == NULL || (bp_filename == filename && bp_line == line)) {
    return 0;
  }
  if ((intptr_t)(bp_ci) < (intptr_t)(mrb->c->ci)) {
    return 0;
  }
  bp_ci = NULL;
  bp_filename = NULL;
  bp_line = -1;
  bp_ciidx = 999;
  return 1;
}

volatile int
mrb_check_stepout(mrb_state *mrb)
{
  if (filename == NULL || (bp_filename == filename && bp_line == line)) {
    return 0;
  }
  if (bp_ciidx <= mrb_debug_get_callinfosize(mrb)) {
    return 0;
  }
  bp_ci = NULL;
  bp_filename = NULL;
  bp_line = -1;
  bp_ciidx = 999;
  return 1;
}

/* dummy function for fnuctionBreakpoint */
void mrb_debug_breakpoint_function(struct mrb_state *mrb) {
  return;
}

static void
mrb_debug_code_fetch(mrb_state *mrb, const mrb_irep *irep, const mrb_code *pc, mrb_value *regs)
{
  filename = mrb_debug_get_filename(mrb, irep, pc - irep->iseq);
  line = mrb_debug_get_line(mrb, irep, pc - irep->iseq);
  if (filename == NULL || line == -1) {
    return;
  }
//  if (prev_filename && filename && strcmp(prev_filename, filename) == 0 && prev_line == line) {
  if (prev_filename == filename && prev_line == line) {
    return;
  }
  mrb_debug_breakpoint_function(mrb);
  if (filename && line >= 0) {
//    ciidx = (int)mrb_debug_get_callinfosize(mrb);
    prev_filename = filename;
    prev_line = line;
//    prev_ciidx = ciidx;
  }
}

static const char *
mrb_debug_get_local_variables(struct mrb_state* mrb) {
  mrb_value locals;
  const char *symname;
  char *locals_cstr;

  int i = 0;
  const struct mrb_irep *irep;
  int local_len = 0;
    
  if(mrb == NULL){
    return "[]";
  }
  irep = mrb->c->ci->proc->body.irep;
  local_len = local_size(irep);

  mrb->code_fetch_hook = NULL;

  locals = mrb_ary_new(mrb);
  for (i = 0; i + 1 < local_len; i++) {
    if (irep->lv[i] == 0){
      continue;
    }
    mrb_sym sym = irep->lv[i];if (!sym){ continue;}
    symname = mrb_sym2name(mrb, sym);
    if (strcmp(symname, "*") == 0 || strcmp(symname, "&") == 0 ) {
      continue;
    }
    mrb_ary_push(mrb, locals, mrb_sym_str(mrb, sym));
  }
  locals_cstr = mrb_str_to_cstr(mrb, mrb_funcall(mrb, locals, "inspect", 0));
  mrb->code_fetch_hook = mrb_debug_code_fetch;
  return locals_cstr;
}

static const char *
mrb_debug_get_local_variable(struct mrb_state* mrb, const char* symname) {
  mrb_value var;
  mrb_sym sym2 = mrb_intern_cstr(mrb, symname);

  char *locals_cstr;

  int i = 0;
  const struct mrb_irep *irep;
  int local_len = 0;

  if(mrb == NULL){
    return "{}";
  }
  irep = mrb->c->ci->proc->body.irep;
  local_len = local_size(irep);

  mrb->code_fetch_hook = NULL;

  var = mrb_hash_new(mrb);
  for (i = 0; i + 1 < local_len; i++) {
    if (irep->lv[i] == 0){
      continue;
    }
    uint16_t reg = i + 1;
    mrb_sym sym = irep->lv[i];if (!sym){ continue;}
    if (sym == sym2) {
      mrb_value v2 = mrb->c->ci->stack[reg];
      const char *v2_classname = mrb_obj_classname(mrb, v2);
      mrb_hash_set(mrb, var, mrb_str_new_cstr(mrb, "name"), mrb_sym_str(mrb, sym));
      mrb_value v2_value = mrb_funcall(mrb, v2, "inspect", 0);

      mrb_hash_set(mrb, var, mrb_str_new_cstr(mrb, "value"), v2_value);
      mrb_hash_set(mrb, var, mrb_str_new_cstr(mrb, "type"), mrb_str_new_cstr(mrb, v2_classname));
      break;
    }
  }
  locals_cstr = mrb_str_to_cstr(mrb, mrb_funcall(mrb, var, "inspect", 0));
  mrb->code_fetch_hook = mrb_debug_code_fetch;
  return locals_cstr;
}

static const char *
mrb_debug_get_variables_all(struct mrb_state *mrb, iv_tbl *t) {
  mrb_value vars;
  int i;

  if (t == NULL || t->alloc == 0 || t->size == 0) {
    return "[]";
  }

  vars = mrb_ary_new(mrb);

  mrb_sym *keys = (mrb_sym*)&t->ptr[t->alloc];
  mrb_value *vals = t->ptr;
  for (i = 0; i < t->alloc; i++) {
    if (IV_KEY_P(keys[i])) {
      mrb_value tmp_v = mrb_hash_new(mrb);
      mrb_hash_set(mrb, tmp_v, mrb_str_new_cstr(mrb, "name"), mrb_sym_str(mrb, keys[i]));
      if (mrb_nil_p(vals[i]) || mrb_true_p(vals[i]) || mrb_false_p(vals[i]) || mrb_string_p(vals[i]) || mrb_fixnum_p(vals[i])) {
        mrb_hash_set(mrb, tmp_v, mrb_str_new_cstr(mrb, "value"), mrb_funcall(mrb, vals[i], "inspect", 0));
      } else {
        mrb_hash_set(mrb, tmp_v, mrb_str_new_cstr(mrb, "value"), mrb_any_to_s(mrb, vals[i]));
      }
      const char *v2_classname = mrb_obj_classname(mrb, vals[i]);
      mrb_hash_set(mrb, tmp_v, mrb_str_new_cstr(mrb, "type"), mrb_str_new_cstr(mrb, v2_classname));
      mrb_ary_push(mrb, vars, tmp_v);
    }
  }
  return mrb_str_to_cstr(mrb, mrb_funcall(mrb, vars, "inspect", 0));
}

static const char *
mrb_debug_get_variables(struct mrb_state *mrb, iv_tbl *t) {
  mrb_value vars;
  int i;

  if (t == NULL || t->alloc == 0 || t->size == 0) {
    return "[]";
  }

  vars = mrb_ary_new(mrb);

  mrb_sym *keys = (mrb_sym*)&t->ptr[t->alloc];
  for (i = 0; i < t->alloc; i++) {
    if (IV_KEY_P(keys[i])) {
      mrb_ary_push(mrb, vars, mrb_sym_str(mrb, keys[i]));
    }
  }
  return mrb_str_to_cstr(mrb, mrb_funcall(mrb, vars, "inspect", 0));
}

static const char *
mrb_debug_get_variable(struct mrb_state *mrb, iv_tbl *t, const char *symname) {
  mrb_value var;
  mrb_sym sym2 = mrb_intern_cstr(mrb, symname);
  int i;

  if (t == NULL || t->alloc == 0 || t->size == 0) {
    return "[]";
  }

  var = mrb_hash_new(mrb);

  mrb_sym *keys = (mrb_sym*)&t->ptr[t->alloc];
  mrb_value *vals = t->ptr;
  for (i = 0; i < t->alloc; i++) {
    if (IV_KEY_P(keys[i])) {
      if (keys[i] == sym2) {
        mrb_hash_set(mrb, var, mrb_str_new_cstr(mrb, "name"), mrb_sym_str(mrb, keys[i]));
        if (mrb_nil_p(vals[i]) || mrb_true_p(vals[i]) || mrb_false_p(vals[i]) || mrb_string_p(vals[i]) || mrb_fixnum_p(vals[i])) {
          mrb_hash_set(mrb, var, mrb_str_new_cstr(mrb, "value"), mrb_funcall(mrb, vals[i], "inspect", 0));
        } else {
          mrb_hash_set(mrb, var, mrb_str_new_cstr(mrb, "value"), mrb_any_to_s(mrb, vals[i]));
        }
        const char *v2_classname = mrb_obj_classname(mrb, vals[i]);
        mrb_hash_set(mrb, var, mrb_str_new_cstr(mrb, "type"), mrb_str_new_cstr(mrb, v2_classname));
      }
    }
  }
  return mrb_str_to_cstr(mrb, mrb_funcall(mrb, var, "inspect", 0));
}

static const char *
mrb_debug_get_global_variables(struct mrb_state* mrb) {
  const char *vars_cstr;
  iv_tbl *t;

  if(mrb == NULL){
    return "[]";
  }
  t = mrb->globals;

  mrb->code_fetch_hook = NULL;
  vars_cstr = mrb_debug_get_variables(mrb, t);
  mrb->code_fetch_hook = mrb_debug_code_fetch;

  return vars_cstr;
}

static const char *
mrb_debug_get_global_variable(struct mrb_state* mrb, const char *symname) {
  const char *vars_cstr;
  iv_tbl *t;

  if(mrb == NULL){
    return "{}";
  }
  t = mrb->globals;

  mrb->code_fetch_hook = NULL;
  vars_cstr = mrb_debug_get_variable(mrb, t, symname);
  mrb->code_fetch_hook = mrb_debug_code_fetch;

  return vars_cstr;
}

static mrb_bool
obj_iv_p(mrb_value obj)
{
  switch (mrb_type(obj)) {
    case MRB_TT_OBJECT:
    case MRB_TT_CLASS:
    case MRB_TT_MODULE:
    case MRB_TT_SCLASS:
    case MRB_TT_HASH:
    case MRB_TT_DATA:
    case MRB_TT_EXCEPTION:
      return TRUE;
    default:
      return FALSE;
  }
}

static const char *
mrb_debug_get_instance_variables(struct mrb_state* mrb) {
  mrb_value self_obj;
  const char *vars_cstr;
  iv_tbl *t;

  if (mrb == NULL) {
    return "[]";
  }

  self_obj = mrb->c->ci->stack[0];
  if (!obj_iv_p(self_obj)) {
    return "[]";
  }
  t = mrb_obj_ptr(self_obj)->iv;
  mrb->code_fetch_hook = NULL;
  vars_cstr = mrb_debug_get_variables(mrb, t);

  mrb->code_fetch_hook = mrb_debug_code_fetch;
  return vars_cstr;
}

static const char *
mrb_debug_get_instance_variable(struct mrb_state* mrb, const char *symname) {
  mrb_value self_obj;
  const char *vars_cstr;
  iv_tbl *t;

  if (mrb == NULL) {
    return "{}";
  }

  self_obj = mrb->c->ci->stack[0];
  if (!obj_iv_p(self_obj)) {
    return "{}";
  }
  t = mrb_obj_ptr(self_obj)->iv;
  mrb->code_fetch_hook = NULL;
  vars_cstr = mrb_debug_get_variable(mrb, t, symname);

  mrb->code_fetch_hook = mrb_debug_code_fetch;
  return vars_cstr;
}

static mrb_value
mrb_debug_get_local_variables_m(struct mrb_state* mrb, mrb_value self)
{
  const char *str = mrb_debug_get_local_variables(mrb);
  return mrb_str_new_cstr(mrb, str);
}

static mrb_value
mrb_debug_get_global_variables_m(struct mrb_state* mrb, mrb_value self)
{
  const char *str = mrb_debug_get_global_variables(mrb);
  return mrb_str_new_cstr(mrb, str);
}

static mrb_value
mrb_debug_get_instance_variables_m(struct mrb_state* mrb, mrb_value self)
{
  const char *str = mrb_debug_get_instance_variables(mrb);
  return mrb_str_new_cstr(mrb, str);
}

static mrb_value
mrb_debug_get_global_variable_m(struct mrb_state* mrb, mrb_value self)
{
  mrb_sym sym;
  const char *symname;
  const char *value;
  mrb_get_args(mrb, "n", &sym);
  symname = mrb_sym2name(mrb, sym);
  value = mrb_debug_get_global_variable(mrb, symname);
  return mrb_str_new_cstr(mrb, value);
}

static mrb_value
mrb_debug_get_instance_variable_m(struct mrb_state* mrb, mrb_value self)
{
  mrb_sym sym;
  const char *symname;
  const char *value;
  mrb_get_args(mrb, "n", &sym);
  symname = mrb_sym2name(mrb, sym);
  value = mrb_debug_get_instance_variable(mrb, symname);
  return mrb_str_new_cstr(mrb, value);
}

static mrb_value
mrb_debug_get_local_variable_m(struct mrb_state* mrb, mrb_value self)
{
  mrb_sym sym;
  const char *symname;
  const char *value;
  mrb_get_args(mrb, "n", &sym);
  symname = mrb_sym2name(mrb, sym);
  value = mrb_debug_get_local_variable(mrb, symname);
  return mrb_str_new_cstr(mrb, value);
}

void mrb_mruby_debug_gem_init(mrb_state *mrb)
{
  struct RClass *debug;

  mrb->code_fetch_hook = mrb_debug_code_fetch;

  debug = mrb_define_module(mrb, "Debug");
  mrb_define_class_method(mrb, debug, "local_variables", mrb_debug_get_local_variables_m, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, debug, "local_variable", mrb_debug_get_local_variable_m, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, debug, "global_variables", mrb_debug_get_global_variables_m, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, debug, "global_variable", mrb_debug_get_global_variable_m, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, debug, "instance_variables", mrb_debug_get_instance_variables_m, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, debug, "instance_variable", mrb_debug_get_instance_variable_m, MRB_ARGS_REQ(1));

}

void mrb_mruby_debug_gem_final(mrb_state *mrb)
{
}

