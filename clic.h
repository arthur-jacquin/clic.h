// clic.h - command line interface companion (0.1.0)
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
// the main program scope. These subcommand identifiers are used:
// * to tell for which subcommand (or absence of) a parameter/named argument
//   is valid/required,
// * to retrieve which subcommand (or absence of) has been invoked after
//   `clic_parse` has been called.
// An enum (with a first `MAIN_SCOPE = 0` constant, explicitely set to 0) is a
// good way to store subcommand identifiers.

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

// #include <stdio.h>
// #define CLIC_IMPL
// #include "clic.h"
//
// int
// main(int argc, char *argv[])
// {
//     int verbose;
//
//     clic_init("demo", "1.0.0", "GPLv3", "Dumb program showcasing clic.h", 0, 1);
//     clic_add_param_flag(0, 'v', "increase verbosity", &verbose, 0);
//     argv += clic_parse(argc, (const char **) argv, NULL);
//
//     printf("Verbosity is %s.\n", verbose ? "high" : "low");
//     printf("Arguments:\n");
//     for (int i = 1; argv[i]; i++) {
//         if (verbose) {
//             printf("%d: ", i);
//         }
//         printf("%s\n", argv[i]);
//     }
//
//     return 0;
// }


#ifndef CLIC_H
#define CLIC_H

void clic_init(const char *program, const char *version, const char *license,
    const char *description, int require_subcommand,
    int accept_unnamed_arguments);

void clic_add_subcommand(int subcommand_id, const char *name,
    const char *description, int accept_unnamed_arguments);

void clic_add_param_flag(int subcommand_id, char name, const char *description,
    int *variable, int mask);
void clic_add_param_bool(int subcommand_id, const char *name,
    const char *description, int default_value, int *variable, int mask);
void clic_add_param_int(int subcommand_id, const char *name,
    const char *description, int default_value, int *variable);
void clic_add_param_string(int subcommand_id, const char *name,
    const char *description, const char *default_value, const char **variable,
    int restrict_to_declared_options);
void clic_add_param_string_option(int subcommand_id, const char *param_name,
    const char *value);

void clic_add_arg_int(int subcommand_id, const char *name,
    const char *description, int *variable);
void clic_add_arg_string(int subcommand_id, const char *name,
    const char *description, const char **variable,
    int restrict_to_declared_options);
void clic_add_arg_string_option(int subcommand_id, const char *arg_name,
    const char *value);

int clic_parse(int argc, const char *argv[], int *subcommand_id);

#endif // CLIC_H


#ifdef CLIC_IMPL

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CLIC_PADDING_1
#define CLIC_PADDING_1          2
#endif
#ifndef CLIC_PADDING_2
#define CLIC_PADDING_2          20
#endif
#ifndef CLIC_PADDING_3
#define CLIC_PADDING_3          10
#endif
#ifndef CLIC_PADDING_4
#define CLIC_PADDING_4          4
#endif

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
struct clic_param_or_arg {
    struct clic_param_or_arg *next;
    const char *name, *description;
    enum clic_type {
        CLIC_FLAG,
        CLIC_BOOL,
        CLIC_INT,
        CLIC_STRING,
    } type;
    int is_required;
    union clic_type_specific_data {
        struct {
            int scalar_default_value, *scalar_variable, mask;
        };
        struct {
            const char *string_default_value, **string_variable;
            int restrict_to_declared_options;
            struct clic_list string_options;
        };
    } data;
};
struct clic_scope {
    struct clic_scope *next;
    int subcommand_id;
    const char *name, *description;
    struct clic_list params, args;
    int accept_unnamed_arguments;
};
struct clic_string_option {
    struct clic_string_option *next;
    const char *param_or_arg_name, *value;
};

static struct clic_elem *clic_add_list_elem(struct clic_list *list,
    size_t size);
static void clic_add_param_or_arg(int subcommand_id, const char *name,
    const char *description, enum clic_type type, int is_required,
    union clic_type_specific_data data);
static void clic_add_param_or_arg_string_option(int subcommand_id,
    int is_required, const char *param_or_arg_name, const char *value);
static void clic_check_initialized_and_not_parsed(void);
static void clic_check_name_correctness(const char *name);
static struct clic_param_or_arg *clic_check_param_or_arg_declaration(
    struct clic_list list, const char *param_or_arg_name,
    int should_be_declared);
static struct clic_scope *clic_check_subcommmand_declaration(int subcommand_id,
    const char *subcommand_name, int should_be_declared);
static void clic_fail(const char *error_message, ...);
static int clic_parse_param_or_arg(struct clic_param_or_arg param_or_arg,
    const char *arg1, const char *arg2);
static void clic_print_help(struct clic_scope scope);
static void clic_print_help_param_or_arg(struct clic_param_or_arg param_or_arg);
static void clic_print_options(void);
static void clic_print_synopsis(void);
static void clic_set_flag_or_bool(int *variable, int value, int mask);

static struct {
    int is_init, is_parsed;
    struct clic_list flag_names, subcommand_scopes;
    struct clic_metadata {
        const char *version, *license;
        int require_subcommand;
    } metadata;
    struct clic_scope main_scope;
} clic_globals;

void
clic_init(const char *program, const char *version, const char *license,
    const char *description, int require_subcommand,
    int accept_unnamed_arguments)
{
    clic_globals.is_init = 1;
    clic_globals.is_parsed = 0;
    clic_globals.metadata = (struct clic_metadata) {
        .version = version,
        .license = license,
        .require_subcommand = require_subcommand,
    };
    clic_globals.main_scope = (struct clic_scope) {
        .name = program,
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
    struct clic_scope *subcommand_scope = (struct clic_scope *)
        clic_add_list_elem(&clic_globals.subcommand_scopes,
            sizeof(*subcommand_scope));
    *subcommand_scope = (struct clic_scope) {
        .subcommand_id = subcommand_id,
        .name = name,
        .description = description,
        .accept_unnamed_arguments = accept_unnamed_arguments,
    };
}

void
clic_add_param_flag(int subcommand_id, char name, const char *description,
    int *variable, int mask)
{
    struct clic_flag_name *flag_name = (struct clic_flag_name *)
        clic_add_list_elem(&clic_globals.flag_names, sizeof(*flag_name));
    flag_name->name[0] = name;
    flag_name->name[1] = 0;
    clic_add_param_or_arg(subcommand_id, flag_name->name, description,
        CLIC_FLAG, 0, (union clic_type_specific_data) {
            .scalar_default_value = 0,
            .scalar_variable = variable,
            .mask = mask,
        });
    if (variable) {
        clic_set_flag_or_bool(variable, 0, mask);
    }
}

void
clic_add_param_bool(int subcommand_id, const char *name,
    const char *description, int default_value, int *variable, int mask)
{
    clic_add_param_or_arg(subcommand_id, name, description, CLIC_BOOL, 0,
        (union clic_type_specific_data) {
            .scalar_default_value = default_value,
            .scalar_variable = variable,
            .mask = mask,
        });
    if (variable) {
        clic_set_flag_or_bool(variable, default_value, mask);
    }
}

void
clic_add_param_int(int subcommand_id, const char *name, const char *description,
    int default_value, int *variable)
{
    clic_add_param_or_arg(subcommand_id, name, description, CLIC_INT, 0,
        (union clic_type_specific_data) {
            .scalar_default_value = default_value,
            .scalar_variable = variable,
        });
    if (variable) {
        *variable = default_value;
    }
}

void
clic_add_param_string(int subcommand_id, const char *name,
    const char *description, const char *default_value, const char **variable,
    int restrict_to_declared_options)
{
    clic_add_param_or_arg(subcommand_id, name, description, CLIC_STRING, 0,
        (union clic_type_specific_data) {
            .string_default_value = default_value,
            .string_variable = variable,
            .restrict_to_declared_options = restrict_to_declared_options,
        });
    if (variable) {
        *variable = default_value;
    }
}

void
clic_add_param_string_option(int subcommand_id, const char *param_name,
    const char *value)
{
    clic_add_param_or_arg_string_option(subcommand_id, 0, param_name, value);
}

void
clic_add_arg_int(int subcommand_id, const char *name, const char *description,
    int *variable)
{
    clic_add_param_or_arg(subcommand_id, name, description, CLIC_INT, 1,
        (union clic_type_specific_data) {
            .scalar_variable = variable,
        });
}

void
clic_add_arg_string(int subcommand_id, const char *name,
    const char *description, const char **variable,
    int restrict_to_declared_options)
{
    clic_add_param_or_arg(subcommand_id, name, description, CLIC_STRING, 1,
        (union clic_type_specific_data) {
            .string_variable = variable,
            .restrict_to_declared_options = restrict_to_declared_options,
        });
}

void
clic_add_arg_string_option(int subcommand_id, const char *arg_name,
    const char *value)
{
    clic_add_param_or_arg_string_option(subcommand_id, 1, arg_name, value);
}

int
clic_parse(int argc, const char *argv[], int *subcommand_id)
{
    int nb_processed_arguments = 0;

    clic_check_initialized_and_not_parsed();
    clic_globals.is_init = 0;
    clic_globals.is_parsed = 1;
    if (!clic_globals.main_scope.name) {
        clic_globals.main_scope.name = argv[0];
    }

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
            if (strcmp(argv[1], scope->name))
                continue;
            active_scope = *scope;
            nb_processed_arguments++;
            break;
        }
    }
    if (!active_scope.subcommand_id &&
        clic_globals.metadata.require_subcommand) {
        if (argc > 1 && !strcmp(argv[1], "--help")) {
            clic_print_help(clic_globals.main_scope);
        } else {
            clic_fail("subcommand not found");
        }
    }
    if (subcommand_id) {
        *subcommand_id = active_scope.subcommand_id;
    }

    // eat parameters
    while ((s = argv[1 + nb_processed_arguments])) {
        if (!strcmp(s, "--")) {
            nb_processed_arguments++;
            break;
        }
        if (s[0] == '-' && isalpha(s[1])) {
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
        clic_list_for(active_scope.params, param, clic_param_or_arg) {
            if (strcmp(name, param->name))
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
                clic_fail("unknown parameter '%s'", name);
            }
        }
    }

    // eat named arguments
    clic_list_for(active_scope.args, arg, clic_param_or_arg) {
        if (!(s = argv[1 + nb_processed_arguments])) {
            clic_fail("missing required argument '%s'", arg->name);
        }
        nb_processed_arguments += clic_parse_param_or_arg(*arg, s, NULL);
    }

    // check if there are unnamed arguments
    if (!active_scope.accept_unnamed_arguments &&
        1 + nb_processed_arguments < argc) {
        clic_fail("too many arguments");
    }

    // cleanup
    clic_list_safe_for(clic_globals.flag_names, flag_name, clic_flag_name) {
        free(flag_name);
    }
    clic_list_safe_for(clic_globals.subcommand_scopes, scope, clic_scope) {
        clic_list_safe_for(scope->params, param_or_arg, clic_param_or_arg) {
            clic_list_safe_for(param_or_arg->data.string_options, string_option,
                clic_string_option) {
                free(string_option);
            }
            free(param_or_arg);
        }
        clic_list_safe_for(scope->args, param_or_arg, clic_param_or_arg) {
            clic_list_safe_for(param_or_arg->data.string_options, string_option,
                clic_string_option) {
                free(string_option);
            }
            free(param_or_arg);
        }
        free(scope);
    }
#endif // CLIC_DUMP_*

    return nb_processed_arguments;
}

static struct clic_elem *
clic_add_list_elem(struct clic_list *list, size_t size)
{
    struct clic_elem *res = malloc(size);
    if (list->start) {
        list->end->next = res;
    } else {
        list->start = res;
    }
    list->end = res;
    return res;
}

static void
clic_add_param_or_arg(int subcommand_id, const char *name,
    const char *description, enum clic_type type, int is_required,
    union clic_type_specific_data data)
{
    clic_check_initialized_and_not_parsed();
    clic_check_name_correctness(name);
    struct clic_scope *scope = clic_check_subcommmand_declaration(subcommand_id,
        NULL, 1);
    struct clic_list *list = is_required ? &scope->args : &scope->params;
    clic_check_param_or_arg_declaration(*list, name, 0);
    struct clic_param_or_arg *param_or_arg = (struct clic_param_or_arg *)
        clic_add_list_elem(list, sizeof(*param_or_arg));
    *param_or_arg = (struct clic_param_or_arg) {
        .name = name,
        .description = description,
        .type = type,
        .is_required = is_required,
        .data = data,
    };
}

static void
clic_add_param_or_arg_string_option(int subcommand_id, int is_required,
    const char *param_or_arg_name, const char *value)
{
    clic_check_initialized_and_not_parsed();
    clic_check_name_correctness(param_or_arg_name);
    struct clic_scope *scope = clic_check_subcommmand_declaration(subcommand_id,
        NULL, 1);
    struct clic_list *list = is_required ? &scope->args : &scope->params;
    struct clic_param_or_arg *param_or_arg =
        clic_check_param_or_arg_declaration(*list, param_or_arg_name, 1);
    if (param_or_arg->type != CLIC_STRING ||
        !param_or_arg->data.restrict_to_declared_options) {
        clic_fail("parameter or argument '%s' is not a restricted-input string, "
            "cannot declare an option '%s' for it",
            param_or_arg_name, value);
    }
    struct clic_string_option *string_option = (struct clic_string_option *)
        clic_add_list_elem(&param_or_arg->data.string_options,
            sizeof(*string_option));
    *string_option = (struct clic_string_option) {
        .param_or_arg_name = param_or_arg_name,
        .value = value,
    };
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
        clic_fail("invalid name '%s'", name ? name : "NULL");
    }
    for (const char *c = name; *c; c++) {
        if (!(isalpha(*c) || *c == '-' || *c == '_')) {
            clic_fail("invalid name '%s'", name);
        }
    }
}

static struct clic_param_or_arg *
clic_check_param_or_arg_declaration(struct clic_list list,
    const char *param_or_arg_name, int should_be_declared)
{
    // if should_be_declared, return pointer
    // else, return NULL
    clic_check_name_correctness(param_or_arg_name);
    clic_list_for(list, param_or_arg, clic_param_or_arg) {
        if (!strcmp(param_or_arg_name, param_or_arg->name)) {
            if (!should_be_declared) {
                clic_fail("parameter/argument '%s' has already been declared in this scope",
                    param_or_arg_name);
            }
            return param_or_arg;
        }
    }
    if (should_be_declared) {
        clic_fail("parameter/argument '%s' has not been declared in this scope",
            param_or_arg_name);
    }
    return NULL;
}

static struct clic_scope *
clic_check_subcommmand_declaration(int subcommand_id,
    const char *subcommand_name, int should_be_declared)
{
    // if should_be_declared, only subcommand_id is checked, return pointer
    // else, return NULL
    if (subcommand_id) {
        if (should_be_declared) {
            clic_list_for(clic_globals.subcommand_scopes, scope, clic_scope) {
                if (subcommand_id == scope->subcommand_id) {
                    return scope;
                }
            }
            clic_fail("subcommand identifier %d has not been declared",
                subcommand_id);
        } else {
            clic_check_name_correctness(subcommand_name);
            clic_list_for(clic_globals.subcommand_scopes, scope, clic_scope) {
                if (subcommand_id == scope->subcommand_id ||
                    !strcmp(subcommand_name, scope->name)) {
                    clic_fail("subcommand identifier %d or name '%s' has already been declared",
                        subcommand_id, subcommand_name);
                }
            }
        }
    } else {
        if (should_be_declared) {
            return &clic_globals.main_scope;
        } else {
            clic_fail("0 is already implicitely used as the main scope identifier");
        }
    }
    return NULL;
}

static void
clic_fail(const char *error_message, ...)
{
    va_list ap;
    fprintf(stderr, "clic: ");
    va_start(ap, error_message);
    vfprintf(stderr, error_message, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

static int
clic_parse_param_or_arg(struct clic_param_or_arg param_or_arg, const char *arg1,
    const char *arg2)
{
    // arg1 and arg2 are command line arguments
    // returns the number of them used to parse param_or_arg
    // check type correctness, value correctness, store in variable

    const char *s = param_or_arg.is_required ? arg1 : arg2;
    int found;

    switch (param_or_arg.type) {
    case CLIC_FLAG:
        if (arg1[1] == '-') {
            clic_fail("bad syntax to set flag '%s'", param_or_arg.name);
        }
        if (param_or_arg.data.scalar_variable) {
            clic_set_flag_or_bool(param_or_arg.data.scalar_variable, 1,
                param_or_arg.data.mask);
        }
        return 1;
    case CLIC_BOOL:
        if (strncmp(arg1, "--", 2)) {
            clic_fail("bad syntax to set bool '%s'", param_or_arg.name);
        }
        if (param_or_arg.data.scalar_variable) {
            clic_set_flag_or_bool(param_or_arg.data.scalar_variable,
                strncmp(arg1, "--no-", 5), param_or_arg.data.mask);
        }
        return 1;
    case CLIC_INT:
    case CLIC_STRING:
        if (!param_or_arg.is_required && !arg2) {
            clic_fail("missing required value for parameter '%s'",
                param_or_arg.name);
        } else if (!param_or_arg.is_required && (strncmp(arg1, "--", 2) ||
            !strncmp(arg1, "--no-", 5))) {
            clic_fail("bad syntax to set %s '%s'",
                param_or_arg.type == CLIC_INT ? "integer" : "string",
                param_or_arg.name);
        }
        if (param_or_arg.type == CLIC_INT) {
            if (atoi(s) == 0 && strcmp(s, "0")) {
                clic_fail("expected an integer (%s), got '%s'",
                    param_or_arg.name, s);
            }
            if (param_or_arg.data.scalar_variable) {
                *param_or_arg.data.scalar_variable = atoi(s);
            }
        } else {
            if (param_or_arg.data.restrict_to_declared_options) {
                found = 0;
                clic_list_for(param_or_arg.data.string_options, string_option,
                    clic_string_option) {
                    if (strcmp(s, string_option->value))
                        continue;
                    found = 1;
                    break;
                }
                if (!found) {
                    clic_fail("'%s' is not an acceptable value for %s", s,
                        param_or_arg.name);
                }
            }
            if (param_or_arg.data.string_variable) {
                *param_or_arg.data.string_variable = s;
            }
        }
        return param_or_arg.is_required ? 1 : 2;
    }
    return 0;
}

static void
clic_print_help(struct clic_scope scope)
{
    const char *program_name, *s;

    // metadata
    printf("%s", program_name = clic_globals.main_scope.name);
    if ((s = clic_globals.metadata.version)) printf(" %s", s);
    if ((s = clic_globals.metadata.license)) printf(" (license: %s)", s);
    printf("\n");
    if ((s = clic_globals.main_scope.description)) printf("%s\n", s);

    // usage, subcommands
    printf("\nUSAGE\n");
    if (scope.subcommand_id || !clic_globals.metadata.require_subcommand) {
        printf("%*s%s", CLIC_PADDING_1, "", program_name);
        if (scope.subcommand_id) printf(" %s", scope.name);
        if (scope.params.start) printf(" [OPTIONS]");
        clic_list_for(scope.args, arg, clic_param_or_arg) {
            printf(" %s", arg->name);
        }
        if (scope.accept_unnamed_arguments) printf(" [ARGUMENTS]");
        printf("\n");
    }
    if (!scope.subcommand_id && clic_globals.subcommand_scopes.start) {
        printf("%*s%s SUBCOMMAND ... (see %s SUBCOMMAND --help)\n",
            CLIC_PADDING_1, "", program_name, program_name);
        printf("\nSUBCOMMANDS\n");
        clic_list_for(clic_globals.subcommand_scopes, subcommand, clic_scope) {
            printf("%*s%-*s", CLIC_PADDING_1, "", CLIC_PADDING_2,
                s = subcommand->name);
            if (subcommand->description) {
                if (strlen(s) >= CLIC_PADDING_2)
                    printf("\n%*s", CLIC_PADDING_1 + CLIC_PADDING_2, "");
                printf("%s", subcommand->description);
            }
            printf("\n");
        }
    }

    // named arguments, parameters
    if (scope.args.start) {
        printf("\nNAMED ARGUMENTS\n");
        clic_list_for(scope.args, arg, clic_param_or_arg) {
            clic_print_help_param_or_arg(*arg);
        }
    }
    if (scope.params.start) {
        printf("\nOPTIONS\n");
        clic_list_for(scope.params, param, clic_param_or_arg) {
            clic_print_help_param_or_arg(*param);
        }
    }

    exit(EXIT_SUCCESS);
}

static void
clic_print_help_param_or_arg(struct clic_param_or_arg param_or_arg)
{
    const char *s;
    enum clic_type type;
    int nb;

    // syntax
    printf("%*s", CLIC_PADDING_1, "");
    s = param_or_arg.name;
    nb = 0;
    switch (type = param_or_arg.type) {
    case CLIC_FLAG:
        nb += printf("-%s", s);
        break;
    case CLIC_BOOL:
        nb += printf("--%s, --no-%s", s, s);
        break;
    case CLIC_INT:
    case CLIC_STRING:
        nb += printf(param_or_arg.is_required ? "%s" : "--%s value", s);
        break;
    }

    // type and description
    if (nb >= CLIC_PADDING_2) {
        printf("\n%*s", CLIC_PADDING_1 + CLIC_PADDING_2, "");
    } else {
        printf("%*s", CLIC_PADDING_2 - nb, "");
    }
    printf("%-*s", CLIC_PADDING_3,
        type == CLIC_FLAG ? "flag" :
        type == CLIC_BOOL ? "boolean" :
        type == CLIC_INT ? "integer" :
        "string");
    if ((s = param_or_arg.description)) printf("%s", s);
    printf("\n");

    // acceptable and default values
    if (type == CLIC_STRING && param_or_arg.data.restrict_to_declared_options) {
        printf("%*soptions: ", CLIC_PADDING_1 + CLIC_PADDING_4, "");
        nb = 0;
        clic_list_for(param_or_arg.data.string_options, string_option,
            clic_string_option) {
            printf("%s%s", nb ? ", ": "", string_option->value);
            nb++;
        }
        printf("\n");
    }
    if (!param_or_arg.is_required && type != CLIC_FLAG) {
        printf("%*sdefault: ", CLIC_PADDING_1 + CLIC_PADDING_4, "");
        switch (type) {
        case CLIC_FLAG: // unreachable
            break;
        case CLIC_BOOL:
            printf("--%s%s",
                param_or_arg.data.scalar_default_value ? "" : "no-",
                param_or_arg.name);
            break;
        case CLIC_INT:
            printf("%d", param_or_arg.data.scalar_default_value);
            break;
        case CLIC_STRING:
            printf("%s", param_or_arg.data.string_default_value);
            break;
        }
        printf("\n");
    }
}

static void
clic_print_options(void)
{
    // TODO
    clic_fail("printing manual sections is unsupported yet");
    exit(EXIT_SUCCESS);
}

static void
clic_print_synopsis(void)
{
    // TODO
    clic_fail("printing manual sections is unsupported yet");
    exit(EXIT_SUCCESS);
}

static void
clic_set_flag_or_bool(int *variable, int value, int mask)
{
    if (mask) {
        if (value) {
            *variable |= mask;
        } else {
            *variable &= ~mask;
        }
    } else {
        *variable = value;
    };
}

#endif // CLIC_IMPL
