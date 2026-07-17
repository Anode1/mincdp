/* agent.c -- give Claude eyes and hands in a browser, on top of mincdp.
 *
 * The whole agent loop in one screen:
 *     screenshot (eyes)  ->  ask Claude (brain)  ->  replay one action (hands)  ->  repeat
 *
 * mincdp supplies eyes and hands over the DevTools Protocol (cdp.h): a PNG of
 * the live page, and navigate / type / press-key / run-JS. The brain is the
 * Claude API, reached with curl -- the one thing raw POSIX sockets can't do here
 * is the TLS to api.anthropic.com, so we shell out, exactly as demo.sh already
 * shells out to curl to poll the debug port. No new dependency mincdp doesn't
 * already assume.
 *
 * Claude replies with exactly one line, the next action:
 *     NAV  <url>     navigate
 *     TYPE <text>    insert text into the focused element
 *     KEY  <name>    press a key ("Enter")
 *     JS   <expr>    run a JS expression that returns true (click/focus/scroll)
 *     DONE <note>    goal reached, stop
 *
 * JSON is written by hand and read by targeted substring checks -- the same bet
 * cdp.h makes, and it holds for the same reason: every reply is one short line.
 *
 * Build (single translation unit -- reuses cdp.h's internal b64/JSON escaper):
 *     cc -std=c99 -Wall -Wextra -O2 -o c/agent c/agent.c
 * Run (Chrome must already be listening; see agent.sh for the launcher):
 *     export ANTHROPIC_API_KEY=sk-ant-...
 *     c/agent 127.0.0.1 <port> <start-url> "search for cats and press enter"
 * Exit 0 on DONE, 1 on error, 2 on usage, 77 if curl / the API key is missing. */
#define CDP_IMPLEMENTATION
#include "cdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AGENT_MODEL   "claude-opus-4-8"
#define AGENT_STEPS   12                 /* max actions before we give up      */
#define AGENT_SHOT    "/tmp/mincdp-agent.png"
#define AGENT_REQ     "/tmp/mincdp-agent-req.json"

/* The brain's contract: look at the screenshot, emit ONE action line. Kept
 * terse on purpose -- the C-side parser only reads the first line's first word. */
static const char *AGENT_SYSTEM =
    "You are driving a headless Chrome browser toward a goal. Each turn you get a "
    "screenshot of the current page. Reply with EXACTLY ONE line, no prose, no "
    "code fences, chosen from:\n"
    "NAV <url>     -- navigate to a URL\n"
    "TYPE <text>   -- type text into the focused field\n"
    "KEY <name>    -- press a key, e.g. KEY Enter\n"
    "JS <expr>     -- run a JS expression that returns true; use to click/focus, "
    "e.g. JS document.querySelector('button.search').click()||true\n"
    "DONE <note>   -- the goal is satisfied; stop\n"
    "Prefer JS with focus() before TYPE. One concrete step at a time.";

/* --- brain: POST the screenshot + goal to Claude, return its action line ----
 * Builds the request body in a heap buffer (a full-page PNG base64s to well over
 * a megabyte), writes it to a temp file, and pipes it to curl. Returns 0 and
 * fills line[] with the model's first reply line; -1 on any failure. */
static int agent_ask(const char *goal, int step, const char *shot_png,
                     char *line, size_t linesz)
{
    /* 1. read the PNG and base64 it (cdp_b64 is cdp.h's own encoder -- same TU) */
    FILE *f = fopen(shot_png, "rb");
    if (!f) { fprintf(stderr, "agent: cannot read %s\n", shot_png); return -1; }
    fseek(f, 0, SEEK_END); long png_n = ftell(f); fseek(f, 0, SEEK_SET);
    if (png_n <= 0) { fclose(f); return -1; }
    unsigned char *png = malloc((size_t)png_n);
    char *b64 = malloc((size_t)png_n / 3 * 4 + 8);
    if (!png || !b64 || fread(png, 1, (size_t)png_n, f) != (size_t)png_n) {
        fclose(f); free(png); free(b64); return -1;
    }
    fclose(f);
    cdp_b64(png, (size_t)png_n, b64);
    free(png);

    /* 2. escape the two free-text fields the same way cdp.h escapes JS/URLs */
    char sys_esc[4096], goal_txt[1152], goal_esc[2048];
    snprintf(goal_txt, sizeof goal_txt, "Goal: %s\nStep %d. Next action?", goal, step);
    if (cdp_json_escape(sys_esc, sizeof sys_esc, AGENT_SYSTEM) < 0 ||
        cdp_json_escape(goal_esc, sizeof goal_esc, goal_txt) < 0) {
        free(b64); return -1;
    }

    /* 3. assemble the request body: system + one user turn (text + the image) */
    size_t bodysz = strlen(b64) + strlen(sys_esc) + strlen(goal_esc) + 512;
    char *body = malloc(bodysz);
    if (!body) { free(b64); return -1; }
    snprintf(body, bodysz,
        "{\"model\":\"%s\",\"max_tokens\":256,"
        "\"system\":\"%s\","
        "\"messages\":[{\"role\":\"user\",\"content\":["
        "{\"type\":\"text\",\"text\":\"%s\"},"
        "{\"type\":\"image\",\"source\":{\"type\":\"base64\","
        "\"media_type\":\"image/png\",\"data\":\"%s\"}}]}]}",
        AGENT_MODEL, sys_esc, goal_esc, b64);
    free(b64);

    /* 4. write the body to a temp file (fixed path, like AGENT_SHOT) and hand
     *    it to curl. The API key rides in via $ANTHROPIC_API_KEY, expanded by
     *    the shell popen spawns -- it never sits in this program's argv. */
    FILE *bf = fopen(AGENT_REQ, "wb");
    if (!bf || fwrite(body, 1, strlen(body), bf) != strlen(body)) {
        if (bf) fclose(bf);
        free(body);
        return -1;
    }
    fclose(bf); free(body);

    FILE *p = popen(
        "curl -s https://api.anthropic.com/v1/messages "
        "-H \"x-api-key: $ANTHROPIC_API_KEY\" "
        "-H \"anthropic-version: 2023-06-01\" "
        "-H \"content-type: application/json\" "
        "--data-binary @" AGENT_REQ, "r");
    if (!p) return -1;
    static char resp[1u << 16];              /* one short reply -- 64K is plenty */
    size_t got = fread(resp, 1, sizeof resp - 1, p);
    resp[got] = 0;
    pclose(p);
    unlink(AGENT_REQ);

    /* 5. pull the action out of {"content":[{"type":"text","text":"NAV ..."}]}.
     *    "text":" appears only on the value, not on "type":"text" (no colon there),
     *    so the first hit is the model's words. Unescape just enough to read one
     *    line, and stop at the first newline. */
    char *t = strstr(resp, "\"text\":\"");
    if (!t) {
        fprintf(stderr, "agent: no text in reply: %.200s\n", resp);
        return -1;
    }
    t += 8;
    size_t o = 0;
    for (; *t && *t != '"' && o + 1 < linesz; t++) {
        if (*t == '\\') {                    /* \n ends the line; \" \\ \t decode */
            t++;
            if (*t == 'n' || *t == 0) break;
            line[o++] = (*t == 't') ? '\t' : *t;
        } else {
            line[o++] = *t;
        }
    }
    line[o] = 0;
    return 0;
}

/* --- hands: replay one action line against the page ------------------------
 * Returns 1 to keep going, 0 on DONE, -1 on a malformed line. */
static int agent_act(cdp *c, const char *line)
{
    char verb[8]; const char *arg = line;
    size_t i = 0;
    while (arg[i] && arg[i] != ' ' && i < sizeof verb - 1) { verb[i] = arg[i]; i++; }
    verb[i] = 0;
    while (arg[i] == ' ') i++;
    arg += i;                                 /* arg now points past the verb    */

    if (strcmp(verb, "DONE") == 0) { printf("  done: %s\n", arg); return 0; }
    if (strcmp(verb, "NAV") == 0) {
        if (cdp_navigate(c, arg) < 0) return -1;
        cdp_wait_bool(c, "!!document.body", 5000);
        return 1;
    }
    if (strcmp(verb, "TYPE") == 0) return cdp_insert_text(c, arg) < 0 ? -1 : 1;
    if (strcmp(verb, "KEY")  == 0) return cdp_key(c, arg) < 0 ? -1 : 1;
    if (strcmp(verb, "JS")   == 0) {
        int v = 0;
        cdp_eval_bool(c, arg, &v);            /* fire-and-forget; a click yields true */
        return 1;
    }
    fprintf(stderr, "agent: unknown action: %s\n", line);
    return -1;
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: %s HOST PORT START_URL GOAL...\n", argv[0]);
        return 2;
    }
    if (!getenv("ANTHROPIC_API_KEY")) {
        fprintf(stderr, "agent: ANTHROPIC_API_KEY not set -- SKIP\n");
        return 77;
    }
    const char *host = argv[1]; int port = atoi(argv[2]); const char *start = argv[3];

    /* argv[4..] joined into one goal string */
    char goal[1024]; size_t g = 0;
    for (int a = 4; a < argc && g + 1 < sizeof goal; a++)
        g += (size_t)snprintf(goal + g, sizeof goal - g, "%s%s", a > 4 ? " " : "", argv[a]);

    cdp *c = cdp_open(host, port);
    if (!c) { fprintf(stderr, "agent: cannot attach to Chrome at %s:%d\n", host, port); return 1; }

    if (cdp_navigate(c, start) < 0 || cdp_wait_bool(c, "!!document.body", 5000) < 0) {
        fprintf(stderr, "agent: initial navigate to %s failed\n", start);
        cdp_close(c); return 1;
    }

    struct timespec settle = {1, 0};          /* let each action's effect render  */
    int rc = 1;
    for (int step = 1; step <= AGENT_STEPS; step++) {
        char line[1024];
        if (cdp_screenshot(c, AGENT_SHOT) < 0) { rc = -1; break; }
        if (agent_ask(goal, step, AGENT_SHOT, line, sizeof line) < 0) { rc = -1; break; }
        printf("step %2d: %s\n", step, line);
        rc = agent_act(c, line);
        if (rc <= 0) break;
        nanosleep(&settle, NULL);
    }

    cdp_close(c);
    if (rc == 0) return 0;                     /* clean DONE                       */
    if (rc < 0) return 1;                      /* an action or the API failed      */
    fprintf(stderr, "agent: gave up after %d steps\n", AGENT_STEPS);
    return 1;
}
