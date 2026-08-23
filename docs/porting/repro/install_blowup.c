// lhat repro: lhat_program_install builds a runtime type anew for every
// place a checked type is named, so hostdata types that name each other
// (World/Body/Shape/Joint/Contact in love.physics) cost millions of live
// objects -- and every later collection walks them all, which is what makes
// a nested lhat_machine_call take 370 ms in lhatove.
//
//   cl /MD /I lhat/include /I lhat/build/release/include install_blowup.c lhat.lib lhatport.lib
//   install_blowup.exe [types] [members]   -> live objects after install
//
// With 5 types of 8 members each naming the other types (the physics
// shape), the machine holds 6.1 million live objects for 40 registrations;
// 5 x 4 holds 131 thousand, 4 x 4 holds 20 thousand. The same 40
// registrations with the cross references replaced by number^ hold 140.
// Seen at lhat HEAD 3a4376c (2026-08-23).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lhat.h"

static char *load(void *ctx, const char *path, size_t *length)
{
    (void)ctx;
    (void)path;
    const char *t = "return^ 1\n";
    size_t n = strlen(t);
    char *c = malloc(n + 1);
    memcpy(c, t, n + 1);
    *length = n;
    return c;
}

static LhatValue nop(struct LhatMachine *m, void *c, const LhatValue *a, size_t n)
{
    (void)m; (void)c; (void)a; (void)n;
    return lhat_nil();
}

static const char *names[] = {"A", "B", "C", "D", "E", "F", "G", "H"};

int main(int argc, char **argv)
{
    int types = argc > 1 ? atoi(argv[1]) : 5;
    int members = argc > 2 ? atoi(argv[2]) : 8;
    bool cross = argc > 3 ? atoi(argv[3]) != 0 : true;
    if (types < 1 || types > 8)
        types = 5;

    LhatProgram *p = lhat_program_new(true, load, NULL);
    for (int t = 0; t < types; t++)
        lhat_register_hostdata_type(p, "m", names[t]);
    // Each member of T answers another type and takes a third, the way
    // Body.getShapes / Shape.getBody / Joint.getBodies / World.getContacts
    // do.
    for (int t = 0; t < types; t++)
    {
        for (int i = 0; i < members; i++)
        {
            char name[32];
            char signature[128];
            snprintf(name, sizeof name, "m%d", i);
            if (cross)
                snprintf(signature, sizeof signature, "p^self^, m.%s -> m.%s;", names[(t + i + 1) % types], names[(t + i + 2) % types]);
            else
                snprintf(signature, sizeof signature, "p^self^, number^ -> number^;");
            lhat_register_member(p, "m", names[t], name, signature, nop, NULL);
        }
    }

    const LhatUnit *u = lhat_program_check(p, "main.lh");
    if (u == NULL || !lhat_unit_ok(u) || !lhat_program_compile(p))
    {
        printf("check/compile FAILED\n");
        return 1;
    }
    LhatMachine *m = lhat_machine_new();
    if (!lhat_program_install(p, m))
    {
        printf("install FAILED\n");
        return 1;
    }
    LhatRunResult ran = lhat_run(m, lhat_unit_proto(u));
    printf("%d types x %d members, cross references %s: status %d, live objects %zu\n",
           types, members, cross ? "on" : "off", (int) ran.status, ran.live);
    lhat_machine_dispose(m);
    lhat_program_free(p);
    return 0;
}
