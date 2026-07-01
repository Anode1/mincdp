/* mincdp -- a minimal Chrome DevTools Protocol client in one Java file, no deps.
 *
 * JDK-only: java.net.http.WebSocket speaks CDP straight to a headless Chrome,
 * with no chromedriver and nothing to vendor. It knows only the handful of
 * commands a click-and-assert test needs. The twin of c/cdp.h; see README.md.
 *
 * Usage: start Chrome yourself with --remote-debugging-port=PORT, then
 *     try (Cdp c = Cdp.attach("127.0.0.1", PORT)) {
 *         c.navigate("http://127.0.0.1:9099/");
 *         c.waitBool("!!document.getElementById('q')", 5000);
 *         c.insertText("hello"); c.key("Enter");
 *     }
 * Responses are matched by id and read with targeted substring checks (no JSON
 * library): it works because every command here returns a boolean or a short
 * ack. Set CDP_DEBUG=1 to trace frames. Not thread-safe. */
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.net.http.WebSocket;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionStage;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class Cdp implements AutoCloseable {
    private static final long   SEND_TIMEOUT_S = 15;
    private static final Pattern ID  = Pattern.compile("\"id\"\\s*:\\s*(\\d+)");
    private static final Pattern WSU = Pattern.compile("\"webSocketDebuggerUrl\"\\s*:\\s*\"(ws://[^\"]+)\"");
    private static final boolean DEBUG = System.getenv("CDP_DEBUG") != null;

    private final WebSocket ws;
    private final AtomicInteger nextId = new AtomicInteger(1);
    private final ConcurrentHashMap<Integer, CompletableFuture<String>> pending = new ConcurrentHashMap<>();
    private final StringBuilder buf = new StringBuilder();

    private Cdp(WebSocket ws) { this.ws = ws; }

    /** Attach to the first "page" target of a Chrome listening on host:port. */
    public static Cdp attach(String host, int port) throws Exception {
        HttpResponse<String> r = HttpClient.newHttpClient().send(
                HttpRequest.newBuilder(URI.create("http://" + host + ":" + port + "/json")).build(),
                HttpResponse.BodyHandlers.ofString());
        String wsUrl = null;
        for (String obj : r.body().split("\\}\\s*,\\s*\\{"))
            if (obj.replace(" ", "").contains("\"type\":\"page\"")) {
                Matcher m = WSU.matcher(obj);
                if (m.find()) { wsUrl = m.group(1); break; }
            }
        if (wsUrl == null) throw new IllegalStateException("cdp: no page target in /json");

        Cdp[] box = new Cdp[1];
        WebSocket.Listener listener = new WebSocket.Listener() {
            @Override public void onOpen(WebSocket s) { s.request(1); }
            @Override public CompletionStage<?> onText(WebSocket s, CharSequence data, boolean last) {
                box[0].buf.append(data);
                if (last) { String m = box[0].buf.toString(); box[0].buf.setLength(0); box[0].dispatch(m); }
                s.request(1);
                return null;
            }
        };
        WebSocket ws = HttpClient.newHttpClient().newWebSocketBuilder()
                .buildAsync(URI.create(wsUrl), listener).join();
        Cdp c = new Cdp(ws);
        box[0] = c;
        return c;
    }

    /** Page.navigate. Does not wait for load -- follow with waitBool on a page predicate. */
    public void navigate(String url) throws Exception {
        send("Page.navigate", "{\"url\":\"" + esc(url) + "\"}");
    }

    /** Runtime.evaluate a JS expression that must yield a boolean. Throws if it throws. */
    public boolean evalBool(String js) throws Exception {
        String resp = send("Runtime.evaluate", "{\"expression\":\"" + esc(js) + "\",\"returnByValue\":true}");
        if (resp.contains("exceptionDetails")) throw new IllegalStateException("page eval threw: " + js);
        return resp.contains("\"value\":true");
    }

    /** Poll evalBool(js) until true or timeout_ms; a transient eval error counts as "not yet". */
    public boolean waitBool(String js, int timeoutMs) throws Exception {
        for (int waited = 0; waited <= timeoutMs; waited += 50) {
            try { if (evalBool(js)) return true; } catch (Exception ignore) { /* mid-navigation */ }
            Thread.sleep(50);
        }
        return false;
    }

    /** Input.insertText into the focused element (fires input events). */
    public void insertText(String text) throws Exception {
        send("Input.insertText", "{\"text\":\"" + esc(text) + "\"}");
    }

    /** Input.dispatchKeyEvent: keyDown+keyUp for a named key ("Enter" -> vk 13). */
    public void key(String key) throws Exception {
        int vk = "Enter".equals(key) ? 13 : 0;
        String at = "\"key\":\"" + esc(key) + "\",\"code\":\"" + esc(key)
                + "\",\"windowsVirtualKeyCode\":" + vk + ",\"nativeVirtualKeyCode\":" + vk;
        send("Input.dispatchKeyEvent", "{\"type\":\"keyDown\"," + at + "}");
        send("Input.dispatchKeyEvent", "{\"type\":\"keyUp\","   + at + "}");
    }

    /** Page.captureScreenshot (PNG) of the current page state; writes it to path.
     *  Unlike Chrome's --screenshot, this captures whatever you have driven the
     *  page to. */
    public void screenshot(String path) throws Exception {
        String resp = send("Page.captureScreenshot", "{\"format\":\"png\"}");
        int i = resp.indexOf("\"data\":\"");
        if (i < 0) throw new IllegalStateException("screenshot: no data in response");
        i += 8;
        int j = resp.indexOf('"', i);
        byte[] png = java.util.Base64.getDecoder().decode(resp.substring(i, j));
        java.nio.file.Files.write(java.nio.file.Path.of(path), png);
    }

    @Override public void close() throws Exception {
        try { ws.sendClose(WebSocket.NORMAL_CLOSURE, ""); } catch (Exception ignore) {}
    }

    // ---- transport ---------------------------------------------------------

    private String send(String method, String params) throws Exception {
        int id = nextId.getAndIncrement();
        CompletableFuture<String> f = new CompletableFuture<>();
        pending.put(id, f);
        String req = "{\"id\":" + id + ",\"method\":\"" + method + "\",\"params\":" + params + "}";
        if (DEBUG) System.err.println("[cdp>] " + req);
        ws.sendText(req, true).join();
        return f.get(SEND_TIMEOUT_S, TimeUnit.SECONDS);
    }

    private void dispatch(String msg) {
        if (DEBUG) System.err.println("[cdp<] " + (msg.length() > 300 ? msg.substring(0, 300) : msg));
        Matcher m = ID.matcher(msg);
        if (m.find()) {                                    // a response carries an id
            CompletableFuture<String> f = pending.remove(Integer.parseInt(m.group(1)));
            if (f != null) f.complete(msg);
        }                                                  // else an event -- ignore
    }

    private static String esc(String s) {
        return s.replace("\\", "\\\\").replace("\"", "\\\"")
                .replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t");
    }
}
