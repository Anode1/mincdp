/* mincdp -- a minimal Chrome DevTools Protocol client in one C99 file, no deps.
 *
 * Single-header library. In exactly ONE translation unit:
 *     #define CDP_IMPLEMENTATION
 *     #include "cdp.h"
 * elsewhere just #include "cdp.h" for the declarations. See README.md for what
 * it is, why it exists, and how it compares to Selenium/Playwright.
 *
 * Usage: start Chrome yourself with --remote-debugging-port=PORT, then
 *     cdp *c = cdp_open("127.0.0.1", PORT);
 *     cdp_navigate(c, "http://127.0.0.1:9099/");
 *     cdp_wait_bool(c, "!!document.getElementById('q')", 5000);
 *     cdp_insert_text(c, "hello");  cdp_key(c, "Enter");
 *     cdp_close(c);
 * Every call returns 0 on success, -1 on failure (cdp_open returns NULL); a
 * diagnostic goes to stderr. Set CDP_DEBUG=1 to trace frames. Not thread-safe:
 * one request/response in flight at a time. */
#ifndef CDP_H
#define CDP_H

typedef struct cdp cdp;

/* Attach to the first "page" target of a Chrome listening on host:port. */
cdp *cdp_open(const char *host, int port);
void cdp_close(cdp *c);

/* Page.navigate. Does not wait for load -- follow with cdp_wait_bool on a
 * predicate only your page satisfies (e.g. an element it defines). */
int cdp_navigate(cdp *c, const char *url);

/* Runtime.evaluate a JS expression that must yield a boolean; *out gets 0/1.
 * -1 if it throws or is not a boolean. */
int cdp_eval_bool(cdp *c, const char *js, int *out);

/* Poll cdp_eval_bool(js) until true or timeout_ms elapses; a transient eval
 * error (e.g. mid-navigation) counts as "not yet". 0 once true, -1 on timeout. */
int cdp_wait_bool(cdp *c, const char *js, int timeout_ms);

/* Input.insertText into the focused element (fires input events, as an IME
 * commit; not per-key events). */
int cdp_insert_text(cdp *c, const char *text);

/* Input.dispatchKeyEvent: keyDown+keyUp for a named key ("Enter" is mapped to
 * its virtual key so a keydown listener sees e.key === "Enter"). */
int cdp_key(cdp *c, const char *key);

/* Page.captureScreenshot (PNG) of the CURRENT page state -- unlike Chrome's
 * --screenshot (which shoots a fresh load), this captures whatever you have
 * driven the page to. Decodes the base64 and writes it to path. Bounded by the
 * 1 MiB message cap. */
int cdp_screenshot(cdp *c, const char *path);

#endif /* CDP_H */

/* ======================================================================== */
#ifdef CDP_IMPLEMENTATION

/* Two layers: a tiny WebSocket client (RFC 6455 -- TCP connect, HTTP Upgrade,
 * masked frames out, fragmented frames in, ping answered) under a CDP
 * request/response loop. JSON is written by hand and read by targeted substring
 * checks: it works only because every command here returns a boolean or a short
 * ack, so we look for "value":true / exceptionDetails rather than parse a
 * document. The handshake accept-hash is not verified -- the peer is a local
 * Chrome we launched, not an untrusted server. */
#define _POSIX_C_SOURCE 200112L   /* getaddrinfo, nanosleep, struct timespec */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>

#define CDP_MSG_CAP (1u << 20)   /* max CDP message we assemble (1 MiB) */
#define CDP_REQ_CAP 65536        /* max command frame we send          */

struct cdp {
    int      fd;               /* the WebSocket TCP socket         */
    int      next_id;          /* monotonic CDP command id         */
    uint64_t rng;              /* xorshift state for frame masks    */
    char    *msg;              /* reusable message assembly buffer  */
};

static uint64_t cdp_xs(uint64_t *s) {   /* xorshift64: mask keys, not security */
    uint64_t x = *s ? *s : 0x9e3779b97f4a7c15ull;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    return (*s = x);
}

static int cdp_read_n(int fd, void *buf, size_t n) {
    unsigned char *p = buf; size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, p + got, n - got);
        if (r == 0) return -1;                     /* peer closed */
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        got += (size_t)r;
    }
    return 0;
}

static int cdp_write_all(int fd, const void *buf, size_t n) {
    const unsigned char *p = buf; size_t sent = 0;
    while (sent < n) {
        ssize_t w = write(fd, p + sent, n - sent);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        sent += (size_t)w;
    }
    return 0;
}

static const char cdp_b64_alpha[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void cdp_b64(const unsigned char *in, size_t n, char *out) {
    size_t i, o = 0;
    for (i = 0; i < n; i += 3) {
        unsigned v = (unsigned)in[i] << 16;
        if (i + 1 < n) v |= (unsigned)in[i + 1] << 8;
        if (i + 2 < n) v |= in[i + 2];
        out[o++] = cdp_b64_alpha[(v >> 18) & 63];
        out[o++] = cdp_b64_alpha[(v >> 12) & 63];
        out[o++] = (i + 1 < n) ? cdp_b64_alpha[(v >> 6) & 63] : '=';
        out[o++] = (i + 2 < n) ? cdp_b64_alpha[v & 63] : '=';
    }
    out[o] = 0;
}

/* Decode base64 in[0..n) into out bytes; returns the byte count. Skips any char
 * not in the alphabet; stops at '='. */
static int cdp_unb64(const char *in, size_t n, unsigned char *out) {
    size_t i, o = 0;
    int val = 0, bits = 0;
    for (i = 0; i < n; i++) {
        char ch = in[i];
        const char *p;
        if (ch == '=') break;
        if (ch == 0) continue;
        p = strchr(cdp_b64_alpha, ch);
        if (!p) continue;
        val = (val << 6) | (int)(p - cdp_b64_alpha);
        bits += 6;
        if (bits >= 8) { bits -= 8; out[o++] = (unsigned char)((val >> bits) & 0xff); }
    }
    return (int)o;
}

/* Escape src into a JSON string body (no surrounding quotes). -1 on overflow. */
static int cdp_json_escape(char *dst, size_t dstsz, const char *src) {
    size_t o = 0;
    for (; *src; src++) {
        unsigned char ch = (unsigned char)*src;
        char esc[8]; const char *e = esc; size_t len;
        switch (ch) {
            case '"':  e = "\\\""; len = 2; break;
            case '\\': e = "\\\\"; len = 2; break;
            case '\n': e = "\\n";  len = 2; break;
            case '\r': e = "\\r";  len = 2; break;
            case '\t': e = "\\t";  len = 2; break;
            default:
                if (ch < 0x20) { snprintf(esc, sizeof esc, "\\u%04x", ch); len = 6; }
                else           { esc[0] = (char)ch; len = 1; }
        }
        if (o + len >= dstsz) return -1;
        memcpy(dst + o, e, len); o += len;
    }
    dst[o] = 0;
    return 0;
}

static int cdp_tcp_connect(const char *host, int port) {
    char portstr[16]; snprintf(portstr, sizeof portstr, "%d", port);
    struct addrinfo hints, *res, *ai; int fd = -1;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, portstr, &hints, &res) != 0) return -1;
    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

/* GET path into buf; returns body length or -1. Stops as soon as Content-Length
 * bytes of body have arrived (Chrome's DevTools server keeps the socket open, so
 * we cannot wait for EOF); a receive timeout is the safety net. */
static int cdp_http_get(const char *host, int port, const char *path,
                        char *buf, size_t bufsz) {
    int fd = cdp_tcp_connect(host, port);
    if (fd < 0) return -1;
    struct timeval tv = {5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    char req[512];
    int n = snprintf(req, sizeof req,
        "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n",
        path, host, port);
    if (n < 0 || (size_t)n >= sizeof req || cdp_write_all(fd, req, (size_t)n) < 0) {
        close(fd); return -1;
    }
    size_t got = 0, hdr = 0; long clen = -1;
    for (;;) {
        if (got + 1 >= bufsz) break;
        ssize_t r = read(fd, buf + got, bufsz - 1 - got);
        if (r == 0) break;                             /* EOF     */
        if (r < 0) { if (errno == EINTR) continue; break; }   /* timeout */
        got += (size_t)r; buf[got] = 0;
        if (!hdr) {
            char *e = strstr(buf, "\r\n\r\n");
            if (e) {
                hdr = (size_t)(e - buf) + 4;
                char *c = strstr(buf, "Content-Length:");
                if (!c) c = strstr(buf, "content-length:");
                if (c) clen = atol(c + 15);
            }
        }
        if (hdr && clen >= 0 && got >= hdr + (size_t)clen) break;
    }
    close(fd);
    buf[got] = 0;
    char *body = strstr(buf, "\r\n\r\n");
    if (!body) return -1;
    body += 4;
    memmove(buf, body, got - (size_t)(body - buf) + 1);
    return (int)strlen(buf);
}

static int cdp_ws_send_frame(cdp *c, int opcode, const unsigned char *payload, size_t n) {
    unsigned char hdr[14]; size_t h = 0;
    hdr[0] = (unsigned char)(0x80 | opcode);       /* FIN + opcode */
    if (n < 126) { hdr[1] = (unsigned char)(0x80 | n); h = 2; }
    else if (n <= 0xffff) {
        hdr[1] = 0x80 | 126;
        hdr[2] = (unsigned char)(n >> 8); hdr[3] = (unsigned char)n; h = 4;
    } else {
        hdr[1] = 0x80 | 127;
        for (int i = 0; i < 8; i++) hdr[2 + i] = (unsigned char)(n >> (56 - 8 * i));
        h = 10;
    }
    unsigned char mask[4];
    uint64_t m = cdp_xs(&c->rng);
    for (int i = 0; i < 4; i++) mask[i] = (unsigned char)(m >> (8 * i));
    memcpy(hdr + h, mask, 4); h += 4;
    if (cdp_write_all(c->fd, hdr, h) < 0) return -1;

    unsigned char chunk[4096]; size_t off = 0;
    while (off < n) {
        size_t k = n - off; if (k > sizeof chunk) k = sizeof chunk;
        for (size_t i = 0; i < k; i++)
            chunk[i] = (unsigned char)(payload[off + i] ^ mask[(off + i) & 3]);
        if (cdp_write_all(c->fd, chunk, k) < 0) return -1;
        off += k;
    }
    return 0;
}

static int cdp_ws_send_text(cdp *c, const char *s, size_t n) {
    return cdp_ws_send_frame(c, 0x1, (const unsigned char *)s, n);
}

/* Read one full application message (assembling fragments, answering pings)
 * into c->msg; sets *len. -1 on close or error. */
static int cdp_ws_read_msg(cdp *c, size_t *len) {
    size_t total = 0;
    for (;;) {
        unsigned char h2[2];
        if (cdp_read_n(c->fd, h2, 2) < 0) return -1;
        int fin = h2[0] & 0x80, op = h2[0] & 0x0f, masked = h2[1] & 0x80;
        uint64_t plen = h2[1] & 0x7f;
        if (plen == 126) {
            unsigned char e[2]; if (cdp_read_n(c->fd, e, 2) < 0) return -1;
            plen = ((uint64_t)e[0] << 8) | e[1];
        } else if (plen == 127) {
            unsigned char e[8]; if (cdp_read_n(c->fd, e, 8) < 0) return -1;
            plen = 0; for (int i = 0; i < 8; i++) plen = (plen << 8) | e[i];
        }
        unsigned char mk[4] = {0,0,0,0};
        if (masked && cdp_read_n(c->fd, mk, 4) < 0) return -1;

        if (op == 0x8) return -1;                  /* close     */
        if (op == 0x9 || op == 0xA) {              /* ping/pong */
            unsigned char ctl[125]; size_t k = plen < sizeof ctl ? (size_t)plen : sizeof ctl;
            if (plen && cdp_read_n(c->fd, ctl, k) < 0) return -1;
            if (op == 0x9 && cdp_ws_send_frame(c, 0xA, ctl, k) < 0) return -1;
            continue;
        }
        if (total + plen >= CDP_MSG_CAP) return -1;    /* message too large */
        if (plen && cdp_read_n(c->fd, c->msg + total, plen) < 0) return -1;
        if (masked) for (uint64_t i = 0; i < plen; i++)
            c->msg[total + i] = (char)((unsigned char)c->msg[total + i] ^ mk[i & 3]);
        total += plen;
        if (fin) break;
    }
    c->msg[total] = 0; *len = total;
    if (getenv("CDP_DEBUG")) fprintf(stderr, "[cdp<] %.300s\n", c->msg);
    return 0;
}

/* Send one command; leave the matching response in c->msg. params is a ready
 * JSON object (or NULL for {}). */
static int cdp_call(cdp *c, const char *method, const char *params) {
    int id = c->next_id++;
    char req[CDP_REQ_CAP];
    int n = snprintf(req, sizeof req,
        "{\"id\":%d,\"method\":\"%s\",\"params\":%s}",
        id, method, params ? params : "{}");
    if (n < 0 || (size_t)n >= sizeof req) return -1;
    if (getenv("CDP_DEBUG")) fprintf(stderr, "[cdp>] %s\n", req);
    if (cdp_ws_send_text(c, req, (size_t)n) < 0) return -1;

    char needle[24]; snprintf(needle, sizeof needle, "\"id\":%d,", id);
    for (;;) {
        size_t len;
        if (cdp_ws_read_msg(c, &len) < 0) return -1;
        if (strstr(c->msg, needle)) return 0;      /* our response; events skipped */
    }
}

cdp *cdp_open(const char *host, int port) {
    char list[65536];
    if (cdp_http_get(host, port, "/json", list, sizeof list) < 0) {
        fprintf(stderr, "cdp: cannot GET /json from %s:%d\n", host, port);
        return NULL;
    }
    /* first target of type "page"; its ws url follows the type field. */
    char *t = strstr(list, "\"type\": \"page\"");
    if (!t) t = strstr(list, "\"type\":\"page\"");
    char *w = t ? strstr(t, "webSocketDebuggerUrl") : NULL;
    char *path = w ? strstr(w, "/devtools/") : NULL;
    if (!path) { fprintf(stderr, "cdp: no page target in /json\n"); return NULL; }
    char *end = strchr(path, '"');
    if (!end) { fprintf(stderr, "cdp: malformed ws url in /json\n"); return NULL; }
    *end = 0;

    int fd = cdp_tcp_connect(host, port);
    if (fd < 0) { fprintf(stderr, "cdp: connect %s:%d failed\n", host, port); return NULL; }
    struct timeval tv = {10, 0};   /* never block forever on a missing reply */
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    unsigned char key[16];
    uint64_t seed = 0x243f6a8885a308d3ull;
    { FILE *u = fopen("/dev/urandom", "rb");
      if (u) { if (fread(key, 1, sizeof key, u) != sizeof key) seed ^= 1; fclose(u); }
      else for (size_t i = 0; i < sizeof key; i++) key[i] = (unsigned char)(seed >> (i & 7)); }
    char keyb64[32]; cdp_b64(key, sizeof key, keyb64);

    char req[512];
    int n = snprintf(req, sizeof req,
        "GET %s HTTP/1.1\r\nHost: %s:%d\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n",
        path, host, port, keyb64);
    if (n < 0 || (size_t)n >= sizeof req || cdp_write_all(fd, req, (size_t)n) < 0) {
        fprintf(stderr, "cdp: ws handshake write failed\n"); close(fd); return NULL;
    }
    char resp[1024]; size_t got = 0;
    while (got + 1 < sizeof resp) {
        ssize_t r = read(fd, resp + got, sizeof resp - 1 - got);
        if (r <= 0) break;
        got += (size_t)r;
        resp[got] = 0;
        if (strstr(resp, "\r\n\r\n")) break;
    }
    if (!strstr(resp, " 101 ")) {
        fprintf(stderr, "cdp: ws upgrade refused\n"); close(fd); return NULL;
    }

    cdp *c = calloc(1, sizeof *c);
    if (!c) { close(fd); return NULL; }
    c->msg = malloc(CDP_MSG_CAP);
    if (!c->msg) { free(c); close(fd); return NULL; }
    c->fd = fd; c->next_id = 1; c->rng = seed ^ ((uint64_t)key[0] << 32 | key[8]);
    return c;
}

void cdp_close(cdp *c) {
    if (!c) return;
    if (c->fd >= 0) close(c->fd);
    free(c->msg); free(c);
}

int cdp_navigate(cdp *c, const char *url) {
    char esc[2048], params[2100];
    if (cdp_json_escape(esc, sizeof esc, url) < 0) return -1;
    snprintf(params, sizeof params, "{\"url\":\"%s\"}", esc);
    return cdp_call(c, "Page.navigate", params);
}

int cdp_eval_bool(cdp *c, const char *js, int *out) {
    char esc[16384], params[16500];
    if (cdp_json_escape(esc, sizeof esc, js) < 0) return -1;
    snprintf(params, sizeof params,
        "{\"expression\":\"%s\",\"returnByValue\":true}", esc);
    if (cdp_call(c, "Runtime.evaluate", params) < 0) return -1;
    if (strstr(c->msg, "exceptionDetails")) return -1;
    if (strstr(c->msg, "\"value\":true"))  { *out = 1; return 0; }
    if (strstr(c->msg, "\"value\":false")) { *out = 0; return 0; }
    return -1;                                     /* not a boolean */
}

int cdp_wait_bool(cdp *c, const char *js, int timeout_ms) {
    struct timespec nap = {0, 50 * 1000 * 1000};   /* 50 ms */
    for (int waited = 0; waited <= timeout_ms; waited += 50) {
        int v = 0;
        if (cdp_eval_bool(c, js, &v) == 0 && v) return 0;
        nanosleep(&nap, NULL);
    }
    return -1;
}

int cdp_insert_text(cdp *c, const char *text) {
    char esc[8192], params[8300];
    if (cdp_json_escape(esc, sizeof esc, text) < 0) return -1;
    snprintf(params, sizeof params, "{\"text\":\"%s\"}", esc);
    return cdp_call(c, "Input.insertText", params);
}

int cdp_key(cdp *c, const char *key) {
    int vk = 0;
    if (strcmp(key, "Enter") == 0) vk = 13;
    char esc[128], params[512];
    if (cdp_json_escape(esc, sizeof esc, key) < 0) return -1;
    snprintf(params, sizeof params,
        "{\"type\":\"keyDown\",\"key\":\"%s\",\"code\":\"%s\","
        "\"windowsVirtualKeyCode\":%d,\"nativeVirtualKeyCode\":%d}",
        esc, esc, vk, vk);
    if (cdp_call(c, "Input.dispatchKeyEvent", params) < 0) return -1;
    snprintf(params, sizeof params,
        "{\"type\":\"keyUp\",\"key\":\"%s\",\"code\":\"%s\","
        "\"windowsVirtualKeyCode\":%d,\"nativeVirtualKeyCode\":%d}",
        esc, esc, vk, vk);
    return cdp_call(c, "Input.dispatchKeyEvent", params);
}

int cdp_screenshot(cdp *c, const char *path) {
    char *d, *end;
    unsigned char *raw;
    size_t n;
    int rawlen, rc = -1;
    FILE *f;

    if (cdp_call(c, "Page.captureScreenshot", "{\"format\":\"png\"}") < 0) return -1;
    d = strstr(c->msg, "\"data\":\"");         /* {"result":{"data":"<base64>"}} */
    if (!d) return -1;
    d += 8;
    end = strchr(d, '"');                       /* base64 has no '"', so this ends it */
    if (!end) return -1;
    n = (size_t)(end - d);

    raw = malloc(n / 4 * 3 + 3);
    if (!raw) return -1;
    rawlen = cdp_unb64(d, n, raw);
    if (rawlen >= 0 && (f = fopen(path, "wb")) != NULL) {
        if (fwrite(raw, 1, (size_t)rawlen, f) == (size_t)rawlen) rc = 0;
        fclose(f);
    }
    free(raw);
    return rc;
}

#endif /* CDP_IMPLEMENTATION */
