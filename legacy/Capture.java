/* Capture.java -- the Java orchestrator of the 2005 Xvfb capture proof of
 * concept, de-identified and reduced to one class with no package. See README.md.
 *
 * By design, image capture runs as a background BATCH, off the user's
 * request path. This class takes a list of URLs and, for each, shells out to
 * capture.sh (browser on the virtual display -> xwd -> thumbnail) with
 * Runtime.exec, pumping the child's stdout/stderr so it cannot block on a full
 * pipe. It is the ancestor of the CDP clients one directory up.
 *
 *   javac Capture.java
 *   java Capture capture.sh https://example.org https://example.net
 *
 * Author: Vasili Gavrilov, 2005. */
import java.io.InputStream;

public class Capture {

    /* Run a command (argv form), streaming its stdout/stderr, return exit code. */
    static int exec(String[] argv) {
        StringBuilder line = new StringBuilder("exec:");
        for (String a : argv) { line.append(' '); line.append(a); }
        System.out.println(line);
        try {
            return handleProc(Runtime.getRuntime().exec(argv));
        } catch (Exception e) {
            e.printStackTrace();
            return -1;
        }
    }

    /* Capture one URL to a thumbnail by invoking the shell pipeline. */
    static int capture(String script, String url) {
        return exec(new String[]{"/bin/sh", script, "-u", url});
    }

    /* Poll the child's stdout and stderr until it exits. A child that writes and
     * is never read will block on a full pipe, so we must drain both while it
     * runs (the alternative is a reader thread per stream). */
    static int handleProc(Process proc) throws Exception {
        int exitValue = 0;
        try {
            InputStream out = proc.getInputStream();
            InputStream err = proc.getErrorStream();
            boolean ended = false;
            while (!ended) {
                try { exitValue = proc.exitValue(); ended = true; }
                catch (IllegalThreadStateException e) { /* not done yet */ }

                int n = out.available();
                if (n > 0) { byte[] b = new byte[n]; int r = out.read(b); if (r > 0) System.out.print(new String(b, 0, r)); }
                n = err.available();
                if (n > 0) { byte[] b = new byte[n]; int r = err.read(b); if (r > 0) System.err.print(new String(b, 0, r)); }

                try { Thread.sleep(10); } catch (InterruptedException e) { /* keep waiting */ }
            }
        } finally {
            proc.destroy();
        }
        return exitValue;
    }

    public static void main(String[] args) {
        if (args.length < 2) {
            System.err.println("usage: java Capture capture.sh URL [URL ...]");
            System.exit(2);
        }
        String script = args[0];
        int ok = 0, fail = 0;
        for (int i = 1; i < args.length; i++) {
            System.out.println("[" + i + "/" + (args.length - 1) + "] " + args[i]);
            if (capture(script, args[i]) == 0) ok++; else fail++;
        }
        System.out.println("batch: " + ok + " captured, " + fail + " failed");
        System.exit(fail > 0 ? 1 : 0);
    }
}
