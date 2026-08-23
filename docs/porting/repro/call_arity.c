// lhat check (passes): lhat_machine_call from inside a host function, as
// love.physics's contact callbacks do it -- 3 and 7 arguments, fresh
// hostdata values among them, the callee reachable only through a table
// parked at L^.modules.h.registry, the host function itself called from a
// resumed coroutine three frames deep, 100,000 frames with a collection
// in between. Written while chasing what looked like a hang in
// World.update; the cause was install_blowup.c, not the call.
//
//   cl /MD /I lhat/include /I lhat/build/release/include call_arity.c lhat.lib lhatport.lib
//   call_arity.exe            -> hits 200000, live wrappers 0 at the end
//
// lhat HEAD 3a4376c (2026-08-23).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lhat.h"

static const char *script =
    "module^ m\n"
    "import^ h\n"
    "var^ hits = 0\n"
    "let^ d0 = p^ { h.call(1, 3) h.call(2, 7) }\n"
    "let^ d1 = p^ { d0() }\n"
    "let^ d2 = p^ { d1() }\n"
    "public^let^ run = p^ {\n"
    "    # The callbacks live in the registry only, as lhatove keeps them.\n"
    "    h.park(1, p^a:h.T, b:h.T, c:h.T { hits := hits + 1 })\n"
    "    h.park(2, p^a:h.T, b:h.T, c:h.T, d:number^, e:number^, f:number^, g:number^ { hits := hits + 1 })\n"
    "    yield^\n"
    "    for^ i from^ 1 to^ 100000 {\n"
    "        d2()\n"
    "        yield^\n"
    "        if^ i % 10000 = 0 { print($\"at {i} hits {hits}\") }\n"
    "    }\n"
    "    print($\"hits {hits}\")\n"
    "}\n";

static char *load(void *ctx, const char *path, size_t *length)
{
    (void)ctx;
    (void)path;
    size_t n = strlen(script);
    char *c = malloc(n + 1);
    memcpy(c, script, n + 1);
    *length = n;
    return c;
}

static const LhatHostDataTag *tag;
static LhatTable *registry;  // parked at L^.modules.h.registry, as lhatove parks callbacks
static int live = 0;
static int thing = 42;

static LhatValue dispose(struct LhatMachine *m, void *c, const LhatValue *a, size_t n)
{
    (void)m; (void)c; (void)a; (void)n;
    live--;
    return lhat_nil();
}

// h.park(slot, fn): keeps fn in the registry.
static LhatValue park(struct LhatMachine *m, void *c, const LhatValue *a, size_t n)
{
    (void)c;
    if (n < 2)
        return lhat_nil();
    bool refused = false;
    lhat_machine_table_set(m, registry, a[0], a[1], &refused);
    return lhat_nil();
}

// h.call(slot, n): calls the parked fn with 3 fresh hostdata values and
// n-3 numbers, from inside a host function.
static LhatValue call(struct LhatMachine *m, void *c, const LhatValue *a, size_t n)
{
    (void)c;
    if (n < 2)
        return lhat_nil();
    size_t count = (size_t) lhat_as_integer(a[1]);
    LhatValue args[8];
    for (size_t i = 0; i < count && i < 8; i++)
    {
        if (i < 3)
        {
            lhat_machine_make_hostdata(m, tag, &thing, &args[i]);
            live++;
        }
        else
            args[i] = lhat_real((double) i);
    }
    LhatValue fn = lhat_table_get(registry, a[0]);
    if (!lhat_is_object_kind(fn, LHAT_OBJECT_SUBROUTINE))
    {
        printf("slot %d holds no subroutine\n", (int) lhat_as_integer(a[0]));
        return lhat_nil();
    }
    LhatRunResult ran = lhat_machine_call(m, fn, args, count);
    if (ran.status != LHAT_RUN_OK)
        printf("call with %zu arguments: status %d\n", count, (int) ran.status);
    return lhat_nil();
}

static LhatValue print_(struct LhatMachine *m, void *c, const LhatValue *a, size_t n)
{
    (void)m; (void)c;
    if (n > 0 && lhat_is_object_kind(a[0], LHAT_OBJECT_STRING))
        printf("%s (live wrappers %d)\n", ((const LhatString *) lhat_as_object(a[0]))->text, live);
    fflush(stdout);
    return lhat_nil();
}

int main(void)
{
    LhatProgram *p = lhat_program_new(true, load, NULL);
    tag = lhat_register_hostdata_type(p, "h", "T");
    lhat_register_member(p, "h", "T", "dispose", "p^self^;", dispose, NULL);
    lhat_register_func(p, "h", "park", "p^number^, any^;", park, NULL);
    lhat_register_func(p, "h", "call", "p^number^, number^;", call, NULL);
    lhat_register_global(p, "print", "p^...;", print_, NULL);
    lhat_bind_initial(p, "print", "L^.print");

    const LhatUnit *u = lhat_program_check(p, "main.lh");
    if (u == NULL || !lhat_unit_ok(u))
    {
        printf("check FAILED\n");
        for (size_t i = 0; u != NULL && i < lhat_unit_diagnostic_count(u); i++)
        {
            char room[512];
            lhat_unit_diagnostic_write(u, i, true, room, sizeof room);
            printf("%s\n", room);
        }
        return 1;
    }
    if (!lhat_program_compile(p))
    {
        printf("compile FAILED\n");
        return 1;
    }
    LhatMachine *m = lhat_machine_new();
    lhat_program_install(p, m);
    {
        LhatValue table;
        lhat_machine_make_table(m, &table);
        lhat_machine_register(m, "h", NULL, "registry", table);
        registry = (LhatTable *) lhat_as_object(table);
    }
    LhatRunResult ran = lhat_run(m, lhat_unit_proto(u));
    printf("run status %d\n", (int) ran.status);
    LhatValue runp = lhat_nil();
    {
        LhatValue key = lhat_nil();
        lhat_machine_make_string(m, "run", 3, &key);
        runp = lhat_table_get((const LhatTable *) lhat_as_object(ran.value), key);
    }
    // A yieldable procedure answers its coroutine; resume drives it.
    LhatRunResult out = lhat_machine_call(m, runp, NULL, 0);
    printf("outer status %d (coroutine %d)\n", (int) out.status, (int) lhat_is_object_kind(out.value, LHAT_OBJECT_COROUTINE));
    // One resume per frame, as a game loop drives it.
    LhatRunResult step;
    size_t resumes = 0;
    do
    {
        step = lhat_machine_resume(m, out.value, NULL, 0);
        resumes++;
        if (step.status != LHAT_RUN_OK)
        {
            printf("resume %zu status %d\n", resumes, (int) step.status);
            break;
        }
    } while (!lhat_machine_coroutine_done(out.value));
    printf("resumed %zu times, last status %d\n", resumes, (int) step.status);
    lhat_machine_dispose(m);
    lhat_program_free(p);
    printf("live wrappers at the end %d\n", live);
    return 0;
}
