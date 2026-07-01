/* codeut.c -- browser-free unit tests: the "regular" layer. It exercises the
 * client's pure helpers (base64, JSON string escaping) with no Chrome, so it
 * always runs. Also a small cookbook of how to add plain unit tests: pull in the
 * implementation, then CHECK the static functions directly. */
#define CDP_IMPLEMENTATION
#include "../c/cdp.h"

#include <stdio.h>
#include <string.h>

static int pass, fail;
#define CHECK(cond, msg) do { \
    if (cond) pass++; \
    else { fail++; printf("  FAIL %s\n", msg); } \
} while (0)

int main(void) {
    char b[32], e[64];

    /* base64 */
    cdp_b64((const unsigned char *)"hello", 5, b);
    CHECK(strcmp(b, "aGVsbG8=") == 0, "base64 encode (with padding)");
    cdp_b64((const unsigned char *)"abc", 3, b);
    CHECK(strcmp(b, "YWJj") == 0, "base64 encode (whole triad, no padding)");
    cdp_b64((const unsigned char *)"", 0, b);
    CHECK(strcmp(b, "") == 0, "base64 empty");

    unsigned char raw[8];
    int rl = cdp_unb64("aGVsbG8=", 8, raw);
    CHECK(rl == 5 && memcmp(raw, "hello", 5) == 0, "base64 decode roundtrip");

    /* JSON string escaping */
    CHECK(cdp_json_escape(e, sizeof e, "plain") == 0 && strcmp(e, "plain") == 0,
          "json escape passthrough");
    CHECK(cdp_json_escape(e, sizeof e, "a\"b\\c\n") == 0 && strcmp(e, "a\\\"b\\\\c\\n") == 0,
          "json escape quote/backslash/newline");
    CHECK(cdp_json_escape(e, sizeof e, "\t") == 0 && strcmp(e, "\\t") == 0,
          "json escape tab");
    CHECK(cdp_json_escape(e, sizeof e, "\x01") == 0 && strcmp(e, "\\u0001") == 0,
          "json escape control char -> \\u");
    CHECK(cdp_json_escape(e, 3, "toolong") == -1,
          "json escape overflow guard");

    printf("codeut: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
