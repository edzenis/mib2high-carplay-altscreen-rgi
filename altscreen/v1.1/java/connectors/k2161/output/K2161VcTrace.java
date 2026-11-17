package com.luka.carplay.routeguidance;

import java.io.File;
import java.io.FileOutputStream;

/** Active K2161 VC diagnostics. Legacy class name retained for Java compatibility. */
final class K2161VcTrace {
    private static final File LIVE = new File("/tmp/AltScreen-rgi-vc-java.log");
    private static final File OLD = new File("/tmp/AltScreen-rgi-vc-java.log.1");
    private static final long MAX_BYTES = 262144L;

    private K2161VcTrace() {}

    static synchronized void record(String event) {
        try {
            if (LIVE.length() > MAX_BYTES) {
                if (OLD.exists()) OLD.delete();
                LIVE.renameTo(OLD);
            }
            FileOutputStream out = new FileOutputStream(LIVE, true);
            try {
                String msg = System.currentTimeMillis() + " VC_TRACE " + event + "\n";
                out.write(msg.getBytes("UTF-8"));
            } finally {
                out.close();
            }
        } catch (Throwable ignored) {
            // Diagnostics must never interrupt RGI/HUD/VC behavior.
        }
    }
}
