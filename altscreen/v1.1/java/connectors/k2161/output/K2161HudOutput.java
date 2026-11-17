package com.luka.carplay.routeguidance;
import com.luka.carplay.framework.Log;
/** BAP is one output sink; it neither owns nor gates input route decoding. */
public final class K2161HudOutput implements AltCoreGuidanceDispatcher.Sink {
    private static final String TAG="K2161HudOutput";
    private BAPBridge bap;
    private Object naviService;
    private boolean started;
    private boolean disposed;
    private long lastSlowReportMs;
    private long slowCount;
    private long slowMaxMs;
    public synchronized boolean init(Object service) {
        if (service==null) return false;
        naviService=service;
        bap=new BAPBridge();
        boolean ok=bap.init(service);
        if (!ok) Log.w(TAG,"BAP bridge unavailable; RGI input remains independent");
        return ok;
    }
    public synchronized void onGuidance(AltCoreGuidanceFrame frame) {
        long startedAt=System.currentTimeMillis();
        boolean wantActive=frame.active;
        boolean activationDelta=frame.activationDelta;
        if (disposed) return;
        if (bap == null) {
            if (naviService == null) return;
            BAPBridge replacement = new BAPBridge();
            if (!replacement.init(naviService)) return;
            bap = replacement;
        }
        if (activationDelta) {
            if (wantActive && !started) {
                if (bap.onStart()) started=true;
                else Log.w(TAG,"RG activation deferred: ownership gate not ready");
            } else if (!wantActive && started) {
                bap.onStop(); bap.onShutdown(); started=false;
            }
        }
        if (started) bap.update(frame.stateCopy());
        long elapsed=System.currentTimeMillis()-startedAt;
        if(elapsed>=100L) {
            slowCount++;
            if(elapsed>slowMaxMs)slowMaxMs=elapsed;
            long now=System.currentTimeMillis();
            if(now-lastSlowReportMs>=1000L){
                lastSlowReportMs=now;
                Log.w(TAG,"ALTHUD_HUD_SINK_MS="+elapsed+" slow_count="+slowCount+
                    " slow_max_ms="+slowMaxMs+" gen="+frame.generation+
                    " seq="+frame.sequence+" activated="+activationDelta);
            }
        }
    }
    public synchronized void onDisconnect() {
        if (disposed) return;
        if (bap != null) { bap.onStop();bap.onShutdown(); }started=false;
    }
    public synchronized void onSessionStop() {
        if (disposed) return;
        if (bap != null) { bap.dispose();bap=null; }
        started=false;
    }
    public synchronized void dispose() {
        if (disposed) return;
        disposed=true;
        if (bap != null) { bap.dispose();bap=null; }started=false;
    }
}
