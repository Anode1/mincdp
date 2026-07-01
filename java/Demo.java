/* Demo.java -- drive page.html through the Java client. The twin of c/demo.c: a
 * cookbook of the two eyeless test kinds -- page-internals checks (presence,
 * attribute, text, computed style, geometry) and interaction ("hands"). The
 * "eyes" (a screenshot) live in tests/shot.sh. Started by demo.sh.
 *   args: CHROME_HOST CHROME_PORT PAGE_URL
 * Exit 0 = all passed, 1 = a failure, 2 = usage. */
public final class Demo {
    private static int pass = 0, fail = 0;
    private static void ok(String label, boolean cond) {
        if (cond) { pass++; System.out.println("  ok   " + label); }
        else      { fail++; System.out.println("  FAIL " + label); }
    }

    /* the workhorse for page-internals checks: is this JS expression true? */
    private static boolean jstrue(Cdp c, String js) {
        try { return c.evalBool(js); } catch (Exception e) { return false; }
    }

    /* is this file a PNG (magic bytes)? */
    private static boolean isPng(String path) {
        try {
            byte[] h = new byte[4];
            try (java.io.InputStream in = java.nio.file.Files.newInputStream(java.nio.file.Path.of(path))) {
                return in.read(h) == 4 && (h[0] & 0xff) == 0x89 && h[1] == 'P' && h[2] == 'N' && h[3] == 'G';
            }
        } catch (Exception e) { return false; }
    }

    private static final String ECHOED =
        "document.getElementById('out').innerText.indexOf('echo: mincdp works')>=0";

    public static void main(String[] argv) throws Exception {
        if (argv.length < 3) { System.err.println("usage: Demo CHROME_HOST CHROME_PORT PAGE_URL"); System.exit(2); }
        String host = argv[0]; int port = Integer.parseInt(argv[1]); String url = argv[2];

        try (Cdp c = Cdp.attach(host, port)) {
            c.navigate(url); ok("navigate to page", true);
            ok("page loaded (#q present)", c.waitBool("!!document.getElementById('q')", 5000));

            // --- page internals: inspect the rendered DOM (no interaction) ---
            ok("title is right",           jstrue(c, "document.title==='mincdp demo'"));
            ok("attribute (placeholder)",  jstrue(c, "document.getElementById('q').getAttribute('placeholder')==='type, then Enter'"));
            ok("text content (#go)",       jstrue(c, "document.getElementById('go').textContent==='go'"));
            ok("visible (computed style)", jstrue(c, "getComputedStyle(document.getElementById('go')).display!=='none'"));
            ok("laid out (geometry)",      jstrue(c, "document.getElementById('go').getBoundingClientRect().width>0"));

            // --- interaction: drive the page and assert the result (hands) ---
            ok("nothing echoed before typing (control)", !c.evalBool(ECHOED));

            c.evalBool("(function(){document.getElementById('q').focus();return true})()");
            c.insertText("mincdp works");
            ok("focus + type into #q", c.waitBool("document.getElementById('q').value==='mincdp works'", 2000));

            c.key("Enter"); ok("press Enter", true);
            ok("page echoed the input after Enter", c.waitBool(ECHOED, 5000));

            // --- eyes over the protocol: screenshot the driven state ---
            c.screenshot("/tmp/mincdp-demo-java.png");
            ok("screenshot via CDP -> valid PNG", isPng("/tmp/mincdp-demo-java.png"));
        }
        System.out.println("demo: " + pass + " passed, " + fail + " failed");
        System.exit(fail > 0 ? 1 : 0);
    }
}
