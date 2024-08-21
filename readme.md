# clic.h

clic.h is a single file header library for command line arguments parsing and
automated help messages generation. It can also be used to generate SYNOPSIS
and OPTIONS manual sections.


### Synopsis

```c
#include <stdio.h>
#define CLIC_IMPL
#include "clic.h"

int
main(int argc, char *argv[])
{
    int verbose;

    clic_init("demo", "1.0.0", "GPLv3", "Dumb program showcasing clic.h", 0, 1);
    clic_add_param_flag(0, 'v', "increase verbosity", &verbose, 0);
    argv += clic_parse(argc, (const char **) argv, NULL);

    printf("Verbosity is %s.\n", verbose ? "high" : "low");
    printf("Arguments:\n");
    for (int i = 1; argv[i]; i++) {
        if (verbose) {
            printf("%d: ", i);
        }
        printf("%s\n", argv[i]);
    }

    return 0;
}
```


### API

Consult the header file itself for the complete documentation, or examples to
get started.

```c
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
```
