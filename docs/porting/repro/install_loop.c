// lhat repro: lhat_program_install never returns when a hostdata type has
// both a member answering the type and a member taking the type.
//
//   cl /I lhat/include install_loop.c lhat.lib lhatport.lib
//   install_loop.exe            -> hangs at "installing..."
//
// Drop either member and it finishes. The runtime type of T is built for
// `translate`'s answer, which walks T's members, reaches `apply`'s parameter
// T, and builds T again -- with nothing remembering that T is under way.
// Seen with love.math.Transform (lhat HEAD 763c137, 2026-08-23).

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

int main(void)
{
    LhatProgram *p = lhat_program_new(true, load, NULL);
    lhat_register_hostdata_type(p, "m", "T");
    // A member answering T ...
    lhat_register_member(p, "m", "T", "translate", "p^self^, number^, number^ -> m.T;", nop, NULL);
    // ... and a member taking T. Either alone is fine.
    lhat_register_member(p, "m", "T", "apply", "p^self^, m.T -> m.T;", nop, NULL);

    const LhatUnit *u = lhat_program_check(p, "main.lh");
    printf("check %s, compile %s, installing...", u && lhat_unit_ok(u) ? "ok" : "FAILED",
           lhat_program_compile(p) ? "ok" : "FAILED");
    fflush(stdout);
    LhatMachine *m = lhat_machine_new();
    bool inst = lhat_program_install(p, m);  // never returns
    printf(" %s\n", inst ? "ok" : "FAILED");
    lhat_machine_dispose(m);
    lhat_program_free(p);
    return 0;
}
