/*
 * CarPlay Route Guidance Module
 *
 * Subscribes to EVT_RGD_UPDATE on CarplayBus, parses route state,
 * sends to BAP cluster.
 *
 * Payload is text in key:type:value format carried over into the bus
 * for minimal consumer churn:
 *   @routeguidance
 *   route_state:n:<0-6>
 *   dist_maneuver_m:n:<meters>
 *   m0_type:n:<maneuver type>
 *   ...
 */
package com.luka.carplay.routeguidance;

import com.luka.carplay.framework.CarplayBus;
import com.luka.carplay.framework.Log;

public class RouteGuidance implements CarplayBus.Listener {

    private static final String TAG = "RouteGuidance";
    private static final int MAX_MANEUVERS = 32;
    /*
     * RouteGuidanceState values verified from reference route-guidance type information:
     * 0=NO_ROUTE_SET, 1=ROUTE_SET, 2=ARRIVED, 3=LOADING, 4=LOCATING, 5=REROUTING, 6=PROCEED_TO_ROUTE
     */
    private static final int ROUTE_STATE_NO_ROUTE_SET = 0;
    private static final int ROUTE_STATE_ROUTE_SET = 1;
    private static final int ROUTE_STATE_REROUTING = 5;
    /*
     * Reference route-guidance processor behavior (CRouteGuidanceUpdateProcessorImpl::isRouteGuidanceManeuverIndexGreater):
     * if routeGuidanceState == 5 then maneuver updates are always accepted.
     *
     * Naming in the available navigation references varies (Locating/Rerouting); value 5 matches the observed state check.
     */
    private static final int ROUTE_STATE_ACCEPT_ALL_MANEUVER_IDX = ROUTE_STATE_REROUTING;

    /* State */
    private volatile boolean running;
    private final AltCoreNavigationEngine engine=new AltCoreNavigationEngine(
        new AltCoreNavigationReducer.Logger() {
            public void debug(String message) {Log.d(TAG,message);}
        });
    private volatile GuidanceWorkQueue outputQueue;
    /* Serializes vehicle output with disposal, NOT with the incoming bus reader. */
    private final Object vehicleOutputLock = new Object();
    private volatile long guidanceGeneration;
    private static final int GUIDANCE_QUEUE_CAPACITY = 1024;
    private long lastQueueLagReportMs;
    private long lastServiceLagReportMs;


    /* Core owns and resets this normalized state. The input only reads it for
     * existing diagnostics; no adapter output receives the mutable reference. */
    private AltCoreRouteState state=engine.diagnosticState();

    /* ============================================================
     * State Container
     * ============================================================ */

    /* Compatibility type for the original Java BAP implementation. All state
     * fields, reset and dirty-mask bookkeeping live in the shared core. */
    public static class State extends AltCoreRouteState {
        public State() { super(); }
    }

    /* ============================================================
     * Lifecycle
     * ============================================================ */

    /** Output attachment is independent of RGI input and core state. */
    public void addOutput(AltCoreGuidanceDispatcher.Sink sink) { engine.addOutput(sink); }
    public void addBufferedOutput(AltCoreGuidanceDispatcher.Sink sink,int capacity) {
        engine.addBufferedOutput(sink,capacity);
    }
    public void removeOutput(AltCoreGuidanceDispatcher.Sink sink) { engine.removeOutput(sink); }
    public boolean init() { Log.i(TAG,"Initialized without output dependency");return true; }

    /**
     * Start route guidance.
     */
    public void start() {
        if (running) return;

        synchronized (vehicleOutputLock) {
            engine.reset();
        }
        guidanceGeneration++;
        final long generation = guidanceGeneration;
        outputQueue = new GuidanceWorkQueue(GUIDANCE_QUEUE_CAPACITY,
            new GuidanceWorkQueue.Handler() {
                public void handle(Object frame) {
                    GuidanceFrame f = (GuidanceFrame)frame;
                    synchronized (vehicleOutputLock) {
                        if (running && generation == guidanceGeneration) {
                            long queueMs = System.currentTimeMillis() - f.enqueuedMs;
                            long now = System.currentTimeMillis();
                            if (queueMs > 200 && now-lastQueueLagReportMs>=1000L) {
                                lastQueueLagReportMs=now;
                                Log.w(TAG,"ALTHUD_RGI_QUEUE_LAG_MS="+queueMs+
                                    " pending="+outputQueue.pending());
                            }
                            processFrame(f.type, f.flags, f.payload, f.payload.length);
                            long serviceMs=System.currentTimeMillis()-now;
                            if(serviceMs>200 && System.currentTimeMillis()-lastServiceLagReportMs>=1000L){
                                lastServiceLagReportMs=System.currentTimeMillis();
                                Log.w(TAG,"ALTHUD_RGI_OUTPUT_SERVICE_MS="+serviceMs+
                                    " queue_before_ms="+queueMs+" pending="+outputQueue.pending());
                            }
                        }
                    }
                }
            }, new GuidanceWorkQueue.Failure() {
                public void handlerFailed(Throwable error) {
                    Log.e(TAG, "RGI_OUTPUT_WORKER_FAILED generation=" + generation, error);
                    recoverInput("guidance_worker_failure");
                }
            });
        running = true;

        CarplayBus bus = CarplayBus.getInstance();
        bus.on(CarplayBus.EVT_RGD_UPDATE, this);
        bus.start();   /* idempotent */

        Log.i(TAG, "Started (subscribed to EVT_RGD_UPDATE)");
    }

    /**
     * Stop route guidance.
     */
    public void stop() {
        if (!running) return;
        running = false;
        guidanceGeneration++;
        CarplayBus.getInstance().off(CarplayBus.EVT_RGD_UPDATE);
        GuidanceWorkQueue q = outputQueue;
        outputQueue = null;
        if (q != null) {
            q.advanceEpoch();
            try { q.shutdown(); }
            catch (InterruptedException e) { Thread.currentThread().interrupt(); }
        }
        /* A committed BAP call cannot be cancelled; wait for it before disposal. */
        synchronized (vehicleOutputLock) {
            engine.sessionStop();
        }

        Log.i(TAG, "Stopped");
    }

    public boolean isRunning() {
        return running;
    }

    /* Pure extraction of the existing wantActive decision for host verification. */
    static boolean wantsRouteOwnership(State candidate) {
        return AltCoreRoutePolicy.wantsRouteOwnership(candidate);
    }

    /* ============================================================
     * CarplayBus Listener
     * ============================================================ */

    private static final class GuidanceFrame {
        final int type, flags;
        final byte[] payload;
        final long enqueuedMs;
        GuidanceFrame(int type, int flags, byte[] payload) {
            this.type = type; this.flags = flags; this.payload = payload;
            this.enqueuedMs = System.currentTimeMillis();
        }
    }

    /* Reader thread only copies and enqueues. All parsing, route ownership and
     * K2161 BAP output is performed by Guidance-Output, in strict wire order.
     * No partial navigation delta may be coalesced or silently discarded. */
    public void onFrame(int type, int flags, byte[] payload, int len) {
        if (!running || type != CarplayBus.EVT_RGD_UPDATE || payload == null || len < 0 || len > payload.length) return;
        GuidanceWorkQueue q = outputQueue;
        if (q == null) return;
        byte[] copy = new byte[len];
        System.arraycopy(payload, 0, copy, 0, len);
        int result = q.offer(new GuidanceFrame(type, flags, copy), q.epoch(), 0);
        if (result == GuidanceWorkQueue.REJECTED) {
            Log.e(TAG, "RGI_QUEUE_OVERFLOW_EXPLICIT_RECONNECT pending=" + q.pending());
            recoverInput("guidance_queue_overflow");
        }
    }

    /* Force a session boundary after overflow or worker exception. Clear stale
     * pending deltas and enqueue an explicit disconnect before allowing new data.
     * The native/iPhone side must re-subscribe: no false claim of recovered route. */
    private void recoverInput(String reason) {
        GuidanceWorkQueue q = outputQueue;
        if (q != null) {
            long epoch = q.advanceEpoch();
            byte[] disconnect;
            try { disconnect = ("@routeguidance\ndisconnect_reason:s:"
                + reason + "\n").getBytes("UTF-8"); }
            catch (java.io.UnsupportedEncodingException e) { disconnect = new byte[0]; }
            if (disconnect.length > 0 &&
                q.offer(new GuidanceFrame(CarplayBus.EVT_RGD_UPDATE, 0, disconnect), epoch, 0)
                    == GuidanceWorkQueue.REJECTED)
                Log.e(TAG, "RGI_RECOVERY_RESET_ENQUEUE_FAILED reason=" + reason);
        }
        CarplayBus.getInstance().forceReconnect(reason);
    }

    private void processFrame(int type, int flags, byte[] payload, int len) {
        if (!running) return;
        if (type != CarplayBus.EVT_RGD_UPDATE) return;
        CarplayBus.Data d = CarplayBus.parseText(payload, len);
        if (d == null) return;

        /* Parse into state (delta) */
        parse(d);
        int diagSession = d.num("diag_session", -1);
        int diagRx = d.num("diag_rx_seq", -1);
        int orderLen = state.maneuverOrder == null ? -1 : state.maneuverOrder.length;
        int order0 = orderLen > 0 ? state.maneuverOrder[0] : -1;
        Log.d(TAG, "bus session=" + diagSession + " rx=" + diagRx
            + " dirty=0x" + Integer.toHexString(state.dirtyMask)
            + " state=" + state.routeState + " count=" + state.maneuverCount
            + " orderLen=" + orderLen + " order0=" + order0
            + " dist=" + state.distManeuverM);
        if (state.dirtyMask == 0) {
            Log.d(TAG, "Bus update: none (keys=" + d.size() + ")");
            return;
        }

        int result=engine.commit();
        if(result==AltCoreNavigationEngine.ACTIVATED ||
           result==AltCoreNavigationEngine.DEACTIVATED)
            Log.i(TAG,"RG state transition active="+
                (result==AltCoreNavigationEngine.ACTIVATED)+
                " route_state="+state.routeState);
    }

    /* ============================================================
     * Parsing
     * ============================================================ */

    private void parse(final CarplayBus.Data d) {
        engine.parse(new AltCoreNavigationReducer.Fields() {
            public boolean has(String key) { return d.has(key); }
            public int num(String key, int fallback) { return d.num(key, fallback); }
            public String str(String key) { return d.str(key); }
            public String str(String key, String fallback) { return d.str(key, fallback); }
            public int size() { return d.size(); }
            public int[] intList(String key) { return d.intList(key); }
            public long num64(String key, long fallback) { return d.num64(key, fallback); }
        });
    }
}
