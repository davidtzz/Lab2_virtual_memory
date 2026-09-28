#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdio.h>
#include "vmm.h"

unsigned long commands_run(vmm_t *vm, FILE *input, int quiet);

#endif