/* demo.c -- drive page.html through the C client. Doubles as the uiut suite and
 * a cookbook of the two eyeless test kinds:
 *   - page internals: inspect the rendered DOM (presence, attribute, text,
 *     computed style, geometry) with no interaction;
 *   - interaction ("hands"): type, press Enter, assert the DOM changed.
 * The "eyes" (a screenshot) live in tests/shot.sh. Started by demo.sh.
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

/* the workhorse for page-internals checks: is this JS expression true? */
static int jstrue(cdp *c, const char *js) {
    int v = 0;
    return cdp_eval_bool(c, js, &v) == 0 && v;
}

#define ECHOED "document.getElementById('out').innerText.indexOf('echo: mincdp works')>=0"

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s CHROME_HOST CHROME_PORT PAGE_URL\n", argv[0]); return 2; }
    const char *host = argv[1]; int port = atoi(argv[2]); const char *url = argv[3];

    cdp *c = cdp_open(host, port);
    if (!c) { fprintf(stderr, "demo: cannot attach to Chrome at %s:%s\n", host, argv[2]); return 1; }

    ok("navigate to page", cdp_navigate(c, url) == 0);
    ok("page loaded (#q present)", cdp_wait_bool(c, "!!document.getElementById('q')", 5000) == 0);

    /* --- page internals: inspect the rendered DOM (no interaction) --- */
    ok("title is right",           jstrue(c, "document.title==='mincdp demo'"));
    ok("attribute (placeholder)",  jstrue(c, "document.getElementById('q').getAttribute('placeholder')==='type, then Enter'"));
    ok("text content (#go)",       jstrue(c, "document.getElementById('go').textContent==='go'"));
    ok("visible (computed style)", jstrue(c, "getComputedStyle(document.getElementById('go')).display!=='none'"));
    ok("laid out (geometry)",      jstrue(c, "document.getElementById('go').getBoundingClientRect().width>0"));

    /* --- interaction: drive the page and assert the result (hands) --- */
    int before = -1;
    cdp_eval_bool(c, ECHOED, &before);
    ok("nothing echoed before typing (control)", before == 0);

    int typed = jstrue(c, "(function(){document.getElementById('q').focus();return true})()")
             && cdp_insert_text(c, "mincdp works") == 0
             && cdp_wait_bool(c, "document.getElementById('q').value==='mincdp works'", 2000) == 0;
    ok("focus + type into #q", typed);

    ok("press Enter", cdp_key(c, "Enter") == 0);
    ok("page echoed the input after Enter", cdp_wait_bool(c, ECHOED, 5000) == 0);

    cdp_close(c);
    printf("demo: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
