package com.luka.carplay.routeguidance;
/** Universal semantic swipe-output contract. Context is an opaque platform token. */
public interface AltCoreTouchpadPort extends TouchpadSwipeCore.Sink {
    void configure(Object platformContext,boolean active);
}
