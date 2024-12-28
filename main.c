/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * main.c
 */

#include "system.h"
#include "scheduler.h"

static void
thread(void *arg) {
    const char *name;
    int i;

    name = (const char *) arg;
    for (i = 0; i < 100; ++i) {
        printf("%s %d\n", name, i);
        us_sleep(20000);
        /scheduler_yield(14);/
    }
}

int
main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    if (scheduler_create(thread, "hello") ||
        scheduler_create(thread, "world") ||
        scheduler_create(thread, "love") ||
        scheduler_create(thread, "this") ||
        scheduler_create(thread, "course!")) {
        TRACE(0);
        return -1;
    }
    scheduler_execute();
    return 0;
}