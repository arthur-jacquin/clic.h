// clic.h - command line interface companion (0.1.0-beta)
// GPLv3 license - Copyright 2024 Arthur Jacquin <arthur@jacquin.xyz>
// https://jacquin.xyz/clic

// clic.h is a single file header library for command line arguments parsing and
// automated help messages generation. It can also be used to generate SYNOPSIS
// and OPTIONS manual sections.


// USAGE

// Like many single file header libraries, no installation is required, just
// put this clic.h file in the codebase and define CLIC_IMPL (before including
// clic.h) in exactly one of the translation unit.

// Commands are understood according to the following structure (all uppercase
// parts being optionnal):
//     program SUBCOMMAND PARAMETERS NAMED_ARGUMENTS UNNAMED_ARGUMENTS

// Using clic.h should be done at program startup, and is a three-part process:
// 1. initialization with `clic_init`
// 2. subcommands, parameters and named arguments declaration with `clic_add_*`
// 3. actual parsing, variable assignments and cleanup with `clic_parse` (stops
//    after named arguments, returns the number of argv elements read for later
//    parsing of unnamed arguments)

// Optionnally, the macros `CLIC_DUMP_SYNOPSIS` and `CLIC_DUMP_OPTIONS` can be
// defined to print out the corresponding manual section and exit on the
// `clic_parse` call. It should be done with a compiler flag (`-DCLIC_DUMP_*`)
// rather than in source code.

// Each subcommand must be associated with a non-null integer, while 0 refers to
// the main program scope. These scope identifiers are used:
// * to tell for which subcommand (or absence of) a parameter/named argument
//   is valid/required,
// * to retrieve which subcommand (or absence of) has been invoked after
//   `clic_parse` has been called.
// An enum (with a first `MAIN_SCOPE = 0` constant, explicitely set to 0) is a
// good way to store scope identifiers.

// Parameters are of one of the following types, with the corresponding command
// line syntax:
// - flag: -n
// - bool: --name, --no-name
// - int or string: --name value
// For flags and booleans, a bit mask can be specified. If so, the associated
// variable will only be modified on the mask bits, allowing to pack multiple
// flags/booleans in the same variable.
// For strings, a list of acceptable values can be specified to restrict input.

// Note: Functions using `const char *` parameters only store the pointer to the
// data, and don't duplicate it. Therefore, it should be given constant data.


// EXAMPLE

// TODO


#ifndef CLIC_H
#define CLIC_H

void clic_init(const char *program, const char *version, const char *license,
    const char *description, int accept_unnamed_arguments);

void clic_add_subcommand(int subcommand_id, const char *name,
    const char *description, int accept_unnamed_arguments);

void clic_add_param_flag(int subcommand_id, char name, const char *description,
    int *variable, int mask);
void clic_add_param_bool(int subcommand_id, const char *name,
    const char *description, int default_value, int *variable, int mask);
void clic_add_param_int(int subcommand_id, const char *name,
    const char *description, int default_value, int *variable);
void clic_add_param_string(int subcommand_id, const char *name,
    const char *description, const char *default_value, char **variable,
    int restrict_to_declared_options);

void clic_add_arg_int(int subcommand_id, const char *name,
    const char *description, int *variable);
void clic_add_arg_string(int subcommand_id, const char *name,
    const char *description, char **variable, int restrict_to_declared_options);

void clic_add_string_option(int subcommand_id, const char *param_or_arg_name,
    const char *value);

int clic_parse(int argc, const char *argv[], int *subcommand_id);

#endif // CLIC_H


#ifdef CLIC_IMPL

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct clic_elem {
    struct clic_elem *next;
};
struct clic_list {
    struct clic_elem *start, *end;
};
#define clic_list_for(list, p, tag) \
    for (struct tag *(p) = (struct tag *) (list).start; (p); (p) = (p)->next)
#define clic_list_safe_for(list, p, tag) \
    for (struct tag *(p) = (struct tag *) (list).start, *next; (p) && \
        (next = (p)->next, 1); (p) = next)

struct clic_flag_name {
    struct clic_flag_name *next;
    char name[2];
};
enum clic_type {
    CLIC_FLAG,
    CLIC_BOOL,
    CLIC_INT,
    CLIC_STRING,
};
struct clic_param_or_arg {
    struct clic_param_or_arg *next;
    // consistent fields (defined for all types)
    int subcommand_id;
    const char *name, *description;
    enum clic_type type;
    int is_required;
    // inconsistent fields (defined for some types only)
    int scalar_default_value, *scalar_variable, mask;
    const char *string_default_value;
    char **string_variable;
    int restrict_to_declared_options;
};
struct clic_scope {
    struct clic_scope *next;
    int subcommand_id;
    const char *subcommand_name, *description;
    int accept_unnamed_arguments;
};
struct clic_string_option {
    struct clic_string_option *next;
    int subcommand_id;
    const char *param_or_arg_name, *value;
};

static void clic_add_param_or_arg(int subcommand_id, const char *name,
    const char *description, enum clic_type type, int is_required,
    struct clic_param_or_arg inconsistent_data);
static void clic_add_to_list(struct clic_list *list, struct clic_elem *elem);
static void clic_check_initialized_and_not_parsed(void);
static void clic_check_name_correctness(const char *name);
static void clic_check_param_or_arg_declaration(int subcommand_id,
    const char *param_or_arg_name, int should_be_declared);
static void clic_check_subcommmand_declaration(int subcommand_id,
    const char *subcommand_name, int should_be_declared);
static void clic_fail(const char *error_message);
static int clic_parse_param_or_arg(struct clic_param_or_arg param_or_arg,
    const char *arg1, const char *arg2);
static void clic_print_help(struct clic_scope scope);
static void clic_print_options(void);
static void clic_print_synopsis(void);

static struct {
    int is_init, is_parsed;
    struct clic_list flag_names, params_and_args, string_options,
        subcommand_scopes;
    struct clic_metadata {
        const char *version, *license;
    } metadata;
    struct clic_scope main_scope;
} clic_globals;

void
clic_init(const char *program, const char *version, const char *license,
    const char *description, int accept_unnamed_arguments)
{
    clic_globals.is_init = 1;
    clic_globals.metadata = (struct clic_metadata) {
        .version = version,
        .license = license,
    };
    clic_globals.main_scope = (struct clic_scope) {
        .subcommand_name = program,
        .description = description,
        .accept_unnamed_arguments = accept_unnamed_arguments,
    };
}

void
clic_add_subcommand(int subcommand_id, const char *name,
    const char *description, int accept_unnamed_arguments)
{
    clic_check_initialized_and_not_parsed();
    clic_check_name_correctness(name);
    clic_check_subcommmand_declaration(subcommand_id, name, 0);
    struct clic_scope *subcommand_scope = malloc(sizeof(*subcommand_scope));
    *subcommand_scope = (struct clic_scope) {
        .subcommand_id = subcommand_id,
        .subcommand_name = name,
        .description = description,
        .accept_unnamed_arguments = accept_unnamed_arguments,
    };
    clic_add_to_list(&clic_globals.subcommand_scopes,
        (struct clic_elem *) subcommand_scope);
}

void
clic_add_param_flag(int subcommand_id, char name, const char *description,
    int *variable, int mask)
{
    struct clic_flag_name *flag_name = malloc(sizeof(*flag_name));
    flag_name->name[0] = name;
    flag_name->name[1] = 0;
    clic_add_to_list(&clic_globals.flag_names, (struct clic_elem *) flag_name);
    clic_add_param_or_arg(subcommand_id, flag_name->name, description,
        CLIC_FLAG, 0, (struct clic_param_or_arg) {
            .scalar_default_value = 0,
            .scalar_variable = variable,
            .mask = mask,
        });
}

void
clic_add_param_bool(int subcommand_id, const char *name,
    const char *description, int default_value, int *variable, int mask)
{
    clic_add_param_or_arg(subcommand_id, name, description, CLIC_BOOL, 0,
        (struct clic_param_or_arg) {
            .scalar_default_value = default_value,
            .scalar_variable = variable,
            .mask = mask,
        });
}

void
clic_add_param_int(int subcommand_id, const char *name, const char *description,
    int default_value, int *variable)
{
    clic_add_param_or_arg(subcommand_id, name, description, CLIC_INT, 0,
        (struct clic_param_or_arg) {
            .scalar_default_value = default_value,
            .scalar_variable = variable,
        });
}

void
clic_add_param_string(int subcommand_id, const char *name,
    const char *description, const char *default_value, char **variable,
    int restrict_to_declared_options)
{
    clic_add_param_or_arg(subcommand_id, name, description, CLIC_STRING, 0,
        (struct clic_param_or_arg) {
            .string_default_value = default_value,
            .string_variable = variable,
            .restrict_to_declared_options = restrict_to_declared_options,
        });
}

void
clic_add_arg_int(int subcommand_id, const char *name, const char *description,
    int *variable)
{
    clic_add_param_or_arg(subcommand_id, name, description, CLIC_INT, 1,
        (struct clic_param_or_arg) {
            .scalar_variable = variable,
        });
}

void
clic_add_arg_string(int subcommand_id, const char *name,
    const char *description, char **variable, int restrict_to_declared_options)
{
    clic_add_param_or_arg(subcommand_id, name, description, CLIC_STRING, 1,
        (struct clic_param_or_arg) {
            .string_variable = variable,
            .restrict_to_declared_options = restrict_to_declared_options,
        });
}

void
clic_add_string_option(int subcommand_id, const char *param_or_arg_name,
    const char *value)
{
    clic_check_initialized_and_not_parsed();
    clic_check_subcommmand_declaration(subcommand_id, NULL, 1);
    clic_check_param_or_arg_declaration(subcommand_id, param_or_arg_name, 1);
    struct clic_string_option *string_option = malloc(sizeof(*string_option));
    *string_option = (struct clic_string_option) {
        .subcommand_id = subcommand_id,
        .param_or_arg_name = param_or_arg_name,
        .value = value,
    };
    clic_add_to_list(&clic_globals.string_options,
        (struct clic_elem *) string_option);
}

int
clic_parse(int argc, const char *argv[], int *subcommand_id)
{
    int nb_processed_arguments = 0;

    clic_check_initialized_and_not_parsed();
    clic_globals.is_parsed = 1;

#if defined(CLIC_DUMP_SYNOPSIS)
    clic_print_synopsis();
#elif defined(CLIC_DUMP_OPTIONS)
    clic_print_options();
#else
    const char *s, *name;
    int found;
    struct clic_scope active_scope = clic_globals.main_scope;

    // detect subcommand
    if (argc > 1) {
        clic_list_for(clic_globals.subcommand_scopes, scope, clic_scope) {
            if (strcmp(argv[1], scope->subcommand_name))
                continue;
            active_scope = *scope;
            nb_processed_arguments++;
            break;
        }
    }
    *subcommand_id = active_scope.subcommand_id;

    // eat parameters
    while ((s = argv[1 + nb_processed_arguments])) {
        if (!strcmp(s, "--")) {
            nb_processed_arguments++;
            break;
        }
        if (s[0] == '-' && isalpha(s[1]) && strlen(s) == 2) {
            name = s + 1;
        } else if (!strncmp(s, "--no-", 5)) {
            name = s + 5;
        } else if (!strncmp(s, "--", 2)) {
            name = s + 2;
        } else {
            // not a parameter
            break;
        }
        found = 0;
        clic_list_for(clic_globals.params_and_args, param, clic_param_or_arg) {
            if (active_scope.subcommand_id != param->subcommand_id ||
                strcmp(name, param->name))
                continue;
            nb_processed_arguments += clic_parse_param_or_arg(*param, s,
                argv[1 + nb_processed_arguments + 1]);
            found = 1;
            break;
        }
        if (!found) {
            // TODO: also handle configuration files (--conf FILE) ?
            if (!strcmp(s, "--help")) {
                clic_print_help(active_scope);
            } else if (!strcmp(s, "--version") &&
                clic_globals.metadata.version) {
                printf("%s\n", clic_globals.metadata.version);
                exit(EXIT_SUCCESS);
            } else {
                // TODO: problem ("parameter not recognised: %s", name)
            }
        }
    }

    // eat named arguments
    clic_list_for(clic_globals.params_and_args, arg, clic_param_or_arg) {
        if (active_scope.subcommand_id != arg->subcommand_id ||
            !arg->is_required)
            continue;
        if (!(s = argv[1 + nb_processed_arguments])) {
            // TODO: problem ("missing required argument %s", arg->name)
        }
        nb_processed_arguments += clic_parse_param_or_arg(*arg, s, NULL);
    }

    // check if there are unnamed arguments
    if (!active_scope.accept_unnamed_arguments &&
        1 + nb_processed_arguments < argc) {
        // TODO: problem ("too many arguments")
    }

    // cleanup
    clic_list_safe_for(clic_globals.flag_names, p, clic_flag_name)
        free(p);
    clic_list_safe_for(clic_globals.params_and_args, p, clic_param_or_arg)
        free(p);
    clic_list_safe_for(clic_globals.string_options, p, clic_string_option)
        free(p);
    clic_list_safe_for(clic_globals.subcommand_scopes, p, clic_scope)
        free(p);
#endif // CLIC_DUMP_*

    return nb_processed_arguments;
}

static void
clic_add_param_or_arg(int subcommand_id, const char *name,
    const char *description, enum clic_type type, int is_required,
    struct clic_param_or_arg inconsistent_data)
{
    clic_check_initialized_and_not_parsed();
    clic_check_name_correctness(name);
    clic_check_subcommmand_declaration(subcommand_id, NULL, 1);
    clic_check_param_or_arg_declaration(subcommand_id, name, 0);
    struct clic_param_or_arg *param_or_arg = malloc(sizeof(*param_or_arg));
    *param_or_arg = inconsistent_data;
    param_or_arg->subcommand_id = subcommand_id;
    param_or_arg->name = name;
    param_or_arg->description = description;
    param_or_arg->type = type;
    param_or_arg->is_required = is_required;
    clic_add_to_list(&clic_globals.params_and_args,
        (struct clic_elem *) param_or_arg);
}

static void
clic_add_to_list(struct clic_list *list, struct clic_elem *elem)
{
    if (list->start) {
        list->end->next = elem;
    } else {
        list->start = elem;
    }
    list->end = elem;
}

static void
clic_check_initialized_and_not_parsed(void)
{
    if (!clic_globals.is_init) {
        clic_fail("clic has not been initialized");
    } else if (clic_globals.is_parsed) {
        clic_fail("clic has already parsed command line arguments");
    }
}

static void
clic_check_name_correctness(const char *name)
{
    if (!name || !isalpha(*name)) {
        clic_fail("clic: invalid parameter/argument name");
    }
    for (const char *c = name; *c; c++) {
        if (!(isalpha(*c) || *c == '-' || *c == '_')) {
            clic_fail("clic: invalid parameter/argument name");
        }
    }
}

static void
clic_check_param_or_arg_declaration(int subcommand_id,
    const char *param_or_arg_name, int should_be_declared)
{
    clic_check_name_correctness(param_or_arg_name);
    int is_declared = 0;
    clic_list_for(clic_globals.params_and_args, param_or_arg,
        clic_param_or_arg) {
        if (subcommand_id == param_or_arg->subcommand_id &&
            !strcmp(param_or_arg_name, param_or_arg->name)) {
            is_declared = 1;
            break;
        }
    }
    if (should_be_declared && !is_declared) {
        clic_fail("clic: parameter/argument has not been declared in this scope");
    } else if (!should_be_declared && is_declared) {
        clic_fail("clic: parameter/argument has already been declared in this scope");
    }
}

static void
clic_check_subcommmand_declaration(int subcommand_id,
    const char *subcommand_name, int should_be_declared)
{
    // if should_be_declared, only subcommand_id is checked
    if (subcommand_id) {
        if (should_be_declared) {
            clic_list_for(clic_globals.subcommand_scopes, scope, clic_scope) {
                if (subcommand_id == scope->subcommand_id) {
                    return;
                }
            }
            clic_fail("clic: subcommand has not been declared");
        } else {
            clic_check_name_correctness(subcommand_name);
            clic_list_for(clic_globals.subcommand_scopes, scope, clic_scope) {
                if (subcommand_id == scope->subcommand_id &&
                    !strcmp(subcommand_name, scope->subcommand_name)) {
                    clic_fail("clic: subcommand has already been declared");
                }
            }
        }
    } else if (!should_be_declared) {
        clic_fail("clic: 0 is already implicitely used as the main scope identifier");
    }
}

static int
clic_parse_param_or_arg(struct clic_param_or_arg param_or_arg, const char *arg1,
    const char *arg2)
{
    // arg1 and arg2 are command line arguments
    // returns the number of them used to parse param_or_arg
    // TODO: check type correctness, correct value, store in variable
    return 0;
}

static void
clic_fail(const char *error_message)
{
    perror(error_message);
    exit(EXIT_FAILURE);
}

static void
clic_print_help(struct clic_scope scope)
{
    // TODO
    exit(EXIT_SUCCESS);
}

static void
clic_print_options(void)
{
    // TODO
    exit(EXIT_SUCCESS);
}

static void
clic_print_synopsis(void)
{
    // TODO
    exit(EXIT_SUCCESS);
}

#endif // CLIC_IMPL
