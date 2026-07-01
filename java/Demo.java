/* Demo.java -- drive page.html through the Java client: type a query, press
 * Enter, assert the page echoes it. The twin of c/demo.c. Started by demo.sh.
 *   args: CHROME_HOST CHROME_PORT PAGE_URL
 * Exit 0 = all passed, 1 = a failure, 2 = usage. */
public final class Demo {
    private static int pass = 0, fail = 0;
    private static void ok(String label, boolean cond) {
        if (cond) { pass++; System.out.println("  ok   " + label); }
        else      { fail++; System.out.println("  FAIL " + label); }
    }

    private static final String ECHOED =
        "document.getElementById('out').innerText.indexOf('echo: mincdp works')>=0";

    public static void main(String[] argv) throws Exception {
        if (argv.length < 3) { System.err.println("usage: Demo CHROME_HOST CHROME_PORT PAGE_URL"); System.exit(2); }
        String host = argv[0]; int port = Integer.parseInt(argv[1]); String url = argv[2];

        try (Cdp c = Cdp.attach(host, port)) {
            c.navigate(url); ok("navigate to page", true);
            ok("page loaded (#q present)", c.waitBool("!!document.getElementById('q')", 5000));

            ok("nothing echoed before typing (control)", !c.evalBool(ECHOED));

            c.evalBool("(function(){document.getElementById('q').focus();return true})()");
            c.insertText("mincdp works");
            ok("focus + type into #q", c.waitBool("document.getElementById('q').value==='mincdp works'", 2000));

            c.key("Enter"); ok("press Enter", true);
            ok("page echoed the input after Enter", c.waitBool(ECHOED, 5000));
        }
        System.out.println("demo: " + pass + " passed, " + fail + " failed");
        System.exit(fail > 0 ? 1 : 0);
    }
}
