/* demo.c -- drive page.html through the C client: type a query, press Enter,
 * assert the page echoes it. The real input+fetch path, not a static DOM dump.
 * Started by demo.sh (which launches Chrome and passes it here).
 *   argv: demo CHROME_HOST CHROME_PORT PAGE_URL
 * Exit 0 = all passed, 1 = a failure, 2 = usage. */
#define CDP_IMPLEMENTATION
#include "cdp.h"
#include <stdio.h>
#include <stdlib.h>

static int pass = 0, fail = 0;
static void ok(const char *label, int cond) {
    if (cond) { pass++; printf("  ok   %s\n", label); }
    else      { fail++; printf("  FAIL %s\n", label); }
}

#define ECHOED "document.getElementById('out').innerText.indexOf('echo: mincdp works')>=0"

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s CHROME_HOST CHROME_PORT PAGE_URL\n", argv[0]); return 2; }
    const char *host = argv[1]; int port = atoi(argv[2]); const char *url = argv[3];

    cdp *c = cdp_open(host, port);
    if (!c) { fprintf(stderr, "demo: cannot attach to Chrome at %s:%s\n", host, argv[2]); return 1; }

    ok("navigate to page", cdp_navigate(c, url) == 0);
    ok("page loaded (#q present)", cdp_wait_bool(c, "!!document.getElementById('q')", 5000) == 0);

    int before = -1;
    cdp_eval_bool(c, ECHOED, &before);
    ok("nothing echoed before typing (control)", before == 0);

    int typed = cdp_eval_bool(c, "(function(){document.getElementById('q').focus();return true})()", &(int){0}) == 0
             && cdp_insert_text(c, "mincdp works") == 0
             && cdp_wait_bool(c, "document.getElementById('q').value==='mincdp works'", 2000) == 0;
    ok("focus + type into #q", typed);

    ok("press Enter", cdp_key(c, "Enter") == 0);
    ok("page echoed the input after Enter", cdp_wait_bool(c, ECHOED, 5000) == 0);

    cdp_close(c);
    printf("demo: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
