package com.luka.carplay.routeguidance;
/** Immutable event envelope: the contained mutable legacy BAP state is never exposed.
 * stateCopy() returns a deep copy isolated from both ingress and other sinks. */
public final class AltCoreGuidanceFrame {
    public final long generation;
    public final long sequence;
    public final boolean active;
    public final boolean activationDelta;
    private final AltCoreRouteState state;
    private final AltCoreSceneSnapshot scene;
    AltCoreGuidanceFrame(long generation,long sequence,AltCoreRouteState state,
                         boolean active,boolean activationDelta) {
        this.generation=generation; this.sequence=sequence;
        this.state=AltCoreGuidanceDispatcher.copy(state);
        this.scene=AltCoreSceneSnapshot.from(this.state,active);
        this.active=active; this.activationDelta=activationDelta;
    }
    public AltCoreSceneSnapshot scene() {return scene;}
    public AltCoreRouteState stateCopy() { return AltCoreGuidanceDispatcher.copy(state); }
}
