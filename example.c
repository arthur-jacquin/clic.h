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
