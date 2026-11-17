package com.luka.carplay.routeguidance;
/** Firmware-neutral route authority. The platform input only parses its bus
 * envelope and supplies Fields; this engine alone owns normalized route state,
 * activation, dirty epochs and fan-out to independently attachable outputs. */
public final class AltCoreNavigationEngine {
    public static final int NO_CHANGE=0;
    public static final int PUBLISHED=1;
    public static final int DISCONNECTED=2;
    public static final int ACTIVATED=3;
    public static final int DEACTIVATED=4;
    private final AltCoreRouteState state=new AltCoreRouteState();
    private final AltCoreNavigationReducer reducer;
    private final AltCoreGuidanceCoordinator coordinator=new AltCoreGuidanceCoordinator();
    private final AltCoreGuidanceDispatcher dispatcher=new AltCoreGuidanceDispatcher();

    public AltCoreNavigationEngine(AltCoreNavigationReducer.Logger logger) {
        reducer=new AltCoreNavigationReducer(state,logger);
    }
    /** For input diagnostics only; never publish this mutable reference to an output. */
    public AltCoreRouteState diagnosticState() {return state;}
    public void addOutput(AltCoreGuidanceDispatcher.Sink sink) {dispatcher.add(sink);}
    public void addBufferedOutput(AltCoreGuidanceDispatcher.Sink sink,int capacity) {
        dispatcher.addBuffered(sink,capacity);
    }
    public void removeOutput(AltCoreGuidanceDispatcher.Sink sink) {dispatcher.remove(sink);}
    public void reset() {coordinator.reset();reducer.reset();}
    public void sessionStop() {reset();dispatcher.sessionStop();}
    public void parse(AltCoreNavigationReducer.Fields fields) {reducer.parse(fields);}
    public int commit() {
        if(state.dirtyMask==0)return NO_CHANGE;
        if(state.disconnectReason!=null) {
            dispatcher.disconnect();
            reset();
            return DISCONNECTED;
        }
        int actMask=AltCoreRouteState.DIRTY_ROUTE_STATE
            | AltCoreRouteState.DIRTY_MANEUVER_STATE
            | AltCoreRouteState.DIRTY_MANEUVER_COUNT
            | AltCoreRouteState.DIRTY_MANEUVER_LIST
            | AltCoreRouteState.DIRTY_MANEUVER_ICON
            | AltCoreRouteState.DIRTY_VISIBLE_IN_APP
            | AltCoreRouteState.DIRTY_SOURCE_SUPPORTS_RG;
        boolean activationDelta=(state.dirtyMask&actMask)!=0;
        boolean wasActive=coordinator.isActive();
        boolean wantActive=coordinator.evaluate(state,activationDelta);
        dispatcher.publish(state,wantActive,activationDelta);
        state.clearDirty();
        if(activationDelta && wasActive!=wantActive)
            return wantActive?ACTIVATED:DEACTIVATED;
        return PUBLISHED;
    }
}
