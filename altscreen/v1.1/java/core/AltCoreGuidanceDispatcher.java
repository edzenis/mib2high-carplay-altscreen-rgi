package com.luka.carplay.routeguidance;
import java.util.Vector;
/** A platform-independent, independently attachable guidance-output multiplexer.
 * Every sink gets its own deep copy; failures cannot reconnect the input. */
public final class AltCoreGuidanceDispatcher {
    public static interface Sink {
        void onGuidance(AltCoreGuidanceFrame frame);
        void onDisconnect();
        void onSessionStop();
    }
    /* Inline sinks preserve the shipped HUD call order. Buffered outputs have
     * their own bounded serial workers and can never block inline delivery. */
    private final Vector sinks = new Vector();
    private final Vector buffered = new Vector();
    private long failedBuffered;

    private static final class Delivery {
        static final int FRAME=1, DISCONNECT=2, STOP=3;
        final int kind;
        final AltCoreGuidanceFrame frame;
        Delivery(int kind, AltCoreGuidanceFrame frame) { this.kind=kind;this.frame=frame; }
    }
    private final class BufferedSink {
        final Sink sink;
        final GuidanceWorkQueue queue;
        volatile boolean closed;
        BufferedSink(final Sink s, int capacity) {
            sink=s;
            queue = new GuidanceWorkQueue(capacity, new GuidanceWorkQueue.Handler() {
                public void handle(Object obj) {
                    Delivery d=(Delivery)obj;
                    if(closed)return;
                    if(d.kind==Delivery.FRAME) sink.onGuidance(d.frame);
                    else if(d.kind==Delivery.DISCONNECT) sink.onDisconnect();
                    else sink.onSessionStop();
                }
            }, new GuidanceWorkQueue.Failure() {
                public void handlerFailed(Throwable error) { failBuffered(BufferedSink.this); }
            });
        }
        void send(Delivery d) {
            if(closed)return;
            if(queue.offer(d,queue.epoch(),0)==GuidanceWorkQueue.REJECTED)
                failBuffered(this); /* Never drop a maneuver delta silently. */
        }
        void boundary(Delivery d) {
            if(closed)return;
            long next=queue.advanceEpoch();
            if(queue.offer(d,next,0)==GuidanceWorkQueue.REJECTED)failBuffered(this);
        }
        void close() { closed=true;queue.shutdownNoWait(); }
    }
    private synchronized void failBuffered(BufferedSink b) {
        if(buffered.removeElement(b)) {failedBuffered++;b.close();}
    }
    public synchronized long failedBuffered() {return failedBuffered;}
    public synchronized int bufferedCount() {return buffered.size();}
    /** Register an independently scheduled output. A stuck or overflowing
     * output is disabled, never allowed to block an established inline sink.
     * No lossy coalescing is permitted for partial RGI updates. */
    public synchronized void addBuffered(Sink s,int capacity) {
        if(s==null)throw new IllegalArgumentException("sink");
        if(capacity<2 || capacity>8192)throw new IllegalArgumentException("capacity");
        if(sinks.contains(s))return;
        for(int i=0;i<buffered.size();i++)if(((BufferedSink)buffered.elementAt(i)).sink==s)return;
        BufferedSink b=new BufferedSink(s,capacity);
        buffered.addElement(b);
        AltCoreGuidanceFrame initial=bootstrap();
        if(initial!=null)b.send(new Delivery(Delivery.FRAME,initial));
    }
    /** Caller may wait for an output to drain in offline tests only. */
    public boolean awaitBufferedIdle(Sink s,long timeoutMs) throws InterruptedException {
        GuidanceWorkQueue q=null;
        synchronized(this) {
            for(int i=0;i<buffered.size();i++) {
                BufferedSink b=(BufferedSink)buffered.elementAt(i);
                if(b.sink==s) {q=b.queue;break;}
            }
        }
        return q==null || q.awaitIdle(timeoutMs);
    }

    private long sequence;
    private long generation;
    private AltCoreGuidanceFrame latest;
    private long failedSinks;
    public synchronized long generation() { return generation; }
    public synchronized long failedSinks() { return failedSinks; }
    /** A late HUD service must receive complete authoritative state immediately;
     * otherwise incremental updates without a new activation delta could never
     * activate HUD guidance. Replay is isolated from the input's dirty epoch. */
    private AltCoreGuidanceFrame bootstrap() {
        if(latest==null || !latest.active)return null;
        AltCoreRouteState initial=latest.stateCopy();
        initial.dirtyMask = AltCoreRouteState.DIRTY_ROUTE_STATE
            | AltCoreRouteState.DIRTY_MANEUVER_STATE
            | AltCoreRouteState.DIRTY_MANEUVER_COUNT
            | AltCoreRouteState.DIRTY_MANEUVER_LIST
            | AltCoreRouteState.DIRTY_DIST_DEST
            | AltCoreRouteState.DIRTY_DIST_MAN
            | AltCoreRouteState.DIRTY_ETA
            | AltCoreRouteState.DIRTY_TIME_REMAINING
            | AltCoreRouteState.DIRTY_CURRENT_ROAD
            | AltCoreRouteState.DIRTY_DESTINATION
            | AltCoreRouteState.DIRTY_MANEUVER_ICON
            | AltCoreRouteState.DIRTY_MANEUVER_TEXT
            | AltCoreRouteState.DIRTY_VISIBLE_IN_APP
            | AltCoreRouteState.DIRTY_SOURCE_SUPPORTS_RG
            | AltCoreRouteState.DIRTY_LANE_GUIDANCE;
        for (int i=0;i<initial.mDirty.length;i++)
            initial.mDirty[i]=AltCoreRouteState.MAN_DIR_ICON|AltCoreRouteState.MAN_DIR_TEXT;
        return new AltCoreGuidanceFrame(generation,sequence,initial,true,true);
    }
    public synchronized void add(Sink s) {
        if (s==null || sinks.contains(s)) return;
        sinks.addElement(s);
        AltCoreGuidanceFrame initial=bootstrap();
        if(initial!=null) {
            try {s.onGuidance(initial);}
            catch(Throwable t) {sinks.removeElement(s);failedSinks++;}
        }
    }
    public synchronized void remove(Sink s) {
        sinks.removeElement(s);
        for(int i=buffered.size()-1;i>=0;i--) {
            BufferedSink b=(BufferedSink)buffered.elementAt(i);
            if(b.sink==s) {buffered.removeElementAt(i);b.close();}
        }
    }
    public synchronized void clear() {
        sinks.removeAllElements();
        for(int i=0;i<buffered.size();i++)((BufferedSink)buffered.elementAt(i)).close();
        buffered.removeAllElements();
    }
    public synchronized void publish(AltCoreRouteState current,
                                     boolean wantActive,boolean activationDelta) {
        AltCoreGuidanceFrame frame=new AltCoreGuidanceFrame(generation,++sequence,
                                                          current,wantActive,activationDelta);
        latest=frame;
        /* Preserve single HUD sink's original serial update order. A future
         * GPU output must internally enqueue and return immediately so it
         * cannot hold the input/vehicle output worker. */
        Object[] consumers=sinks.toArray();
        for (int n=0;n<consumers.length;n++) {
            Sink sink=(Sink)consumers[n];
            if (!sinks.contains(sink)) continue;
            try { sink.onGuidance(frame); }
            catch (Throwable t) { sinks.removeElement(sink); failedSinks++; }
        }
        Object[] extra=buffered.toArray();
        for(int n=0;n<extra.length;n++)((BufferedSink)extra[n]).send(new Delivery(Delivery.FRAME,frame));
    }
    public synchronized void disconnect() {
        generation++; sequence=0;latest=null;
        Object[] consumers=sinks.toArray();
        for (int n=0;n<consumers.length;n++) {
            Sink sink=(Sink)consumers[n];
            try { sink.onDisconnect(); }
            catch (Throwable t) { sinks.removeElement(sink); failedSinks++; }
        }
        Object[] extra=buffered.toArray();
        for(int n=0;n<extra.length;n++)((BufferedSink)extra[n]).boundary(new Delivery(Delivery.DISCONNECT,null));
    }
    public synchronized void sessionStop() {
        generation++; sequence=0;latest=null;
        Object[] consumers=sinks.toArray();
        for (int n=0;n<consumers.length;n++) {
            Sink sink=(Sink)consumers[n];
            try { sink.onSessionStop(); }
            catch (Throwable t) { sinks.removeElement(sink); failedSinks++; }
        }
        Object[] extra=buffered.toArray();
        for(int n=0;n<extra.length;n++)((BufferedSink)extra[n]).boundary(new Delivery(Delivery.STOP,null));
    }
    /** Explicit copy of every mutable navigation field; only arrays are mutable. */
    public static AltCoreRouteState copy(AltCoreRouteState s) {
        if (s == null) throw new IllegalArgumentException("null route state");
        AltCoreRouteState d = new AltCoreRouteState();
        d.dirtyMask = s.dirtyMask;
        d.mDirty = (s.mDirty == null ? null : (int[])s.mDirty.clone());
        d.routeState = s.routeState;
        d.maneuverState = s.maneuverState;
        d.maneuverCount = s.maneuverCount;
        d.maneuverOrder = (s.maneuverOrder == null ? null : (int[])s.maneuverOrder.clone());
        d.visibleInApp = s.visibleInApp;
        d.sourceSupportsRg = s.sourceSupportsRg;
        d.laneGuidanceShowing = s.laneGuidanceShowing;
        d.laneGuidanceTotal = s.laneGuidanceTotal;
        d.laneGuidanceIndex = s.laneGuidanceIndex;
        d.laneGuidanceSlot = s.laneGuidanceSlot;
        d.distDestM = s.distDestM;
        d.distManeuverM = s.distManeuverM;
        d.etaSeconds = s.etaSeconds;
        d.timeRemainingSeconds = s.timeRemainingSeconds;
        d.currentRoad = s.currentRoad;
        d.destination = s.destination;
        d.disconnectReason = s.disconnectReason;
        d.mType = (s.mType == null ? null : (int[])s.mType.clone());
        d.mTurnAngle = (s.mTurnAngle == null ? null : (int[])s.mTurnAngle.clone());
        d.mZLevel = (s.mZLevel == null ? null : (int[])s.mZLevel.clone());
        d.mJunctionType = (s.mJunctionType == null ? null : (int[])s.mJunctionType.clone());
        d.mDrivingSide = (s.mDrivingSide == null ? null : (int[])s.mDrivingSide.clone());
        d.mDistance = (s.mDistance == null ? null : (int[])s.mDistance.clone());
        d.mName = (s.mName == null ? null : (String[])s.mName.clone());
        d.mAfterRoad = (s.mAfterRoad == null ? null : (String[])s.mAfterRoad.clone());
        d.mExitInfo = (s.mExitInfo == null ? null : (String[])s.mExitInfo.clone());
        d.mJunctionAngles = copy2(s.mJunctionAngles);
        d.mExitAngle = (s.mExitAngle == null ? null : (int[])s.mExitAngle.clone());
        d.mLinkedLaneGuidanceIndex = (s.mLinkedLaneGuidanceIndex == null ? null : (int[])s.mLinkedLaneGuidanceIndex.clone());
        d.mLinkedLaneGuidanceSlot = (s.mLinkedLaneGuidanceSlot == null ? null : (int[])s.mLinkedLaneGuidanceSlot.clone());
        d.mVer = (s.mVer == null ? null : (int[])s.mVer.clone());
        d.mLanePositions = copy2(s.mLanePositions);
        d.mLaneCount = (s.mLaneCount == null ? null : (int[])s.mLaneCount.clone());
        d.mLaneDirections = copy2(s.mLaneDirections);
        d.mLaneStatus = copy2(s.mLaneStatus);
        d.mLaneAngles = copy3(s.mLaneAngles);
        d.lgIndex = (s.lgIndex == null ? null : (int[])s.lgIndex.clone());
        d.lgLanePositions = copy2(s.lgLanePositions);
        d.lgLaneCount = (s.lgLaneCount == null ? null : (int[])s.lgLaneCount.clone());
        d.lgLaneDirections = copy2(s.lgLaneDirections);
        d.lgLaneStatus = copy2(s.lgLaneStatus);
        d.lgLaneAngles = copy3(s.lgLaneAngles);
        return d;
    }
    private static int[][] copy2(int[][] source) {
        if (source == null) return null;
        int[][] result = new int[source.length][];
        for (int i=0;i<source.length;i++) result[i] = source[i] == null ? null : (int[])source[i].clone();
        return result;
    }
    private static int[][][] copy3(int[][][] source) {
        if (source == null) return null;
        int[][][] result = new int[source.length][][];
        for (int i=0;i<source.length;i++) result[i] = copy2(source[i]);
        return result;
    }
}
