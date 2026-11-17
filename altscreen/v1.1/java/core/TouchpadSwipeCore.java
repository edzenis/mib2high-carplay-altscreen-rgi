/*
 * Firmware-independent touchpad-to-direction core.
 * Uses only time/coordinate samples; the calling firmware input adapter provides
 * raw one-finger samples and turns directions into platform button events.
 * Original clean-room implementation, inspired by Luka's D-pad gesture design.
 */
package com.luka.carplay.routeguidance;

public final class TouchpadSwipeCore {
    public static final int LEFT = 1;
    public static final int RIGHT = 2;
    public static final int UP = 3;
    public static final int DOWN = 4;

    public interface Sink { void direction(int key); }

    private static final int DEADZONE = 30;
    private static final int FAST_THRESHOLD = 150;
    private static final int SLOW_THRESHOLD = 200;
    private static final int SLOW_SPEED = 100;
    private static final int FAST_SPEED = 300;
    private static final int MAX_TICKS_PER_SAMPLE = 8;

    private final Sink sink;
    private boolean active;
    private boolean armed;
    private int prevX;
    private int prevY;
    private long prevMs;
    private int dxAccum;
    private int dyAccum;

    public TouchpadSwipeCore(Sink newSink) {
        if (newSink == null) throw new IllegalArgumentException("sink");
        sink = newSink;
    }

    public synchronized void release() {
        active = false;
        armed = false;
        dxAccum = 0;
        dyAccum = 0;
        prevMs = 0;
    }

    public synchronized void sample(int x, int y, long ms) {
        if (!active || ms < prevMs) {
            active = true;
            armed = false;
            dxAccum = 0;
            dyAccum = 0;
            prevX = x;
            prevY = y;
            prevMs = ms;
            return;
        }
        int dx = x - prevX;
        int dy = y - prevY;
        prevX = x;
        prevY = y;
        long elapsed = ms - prevMs;
        prevMs = ms;
        if (elapsed < 1) elapsed = 1;
        if (elapsed > 200) elapsed = 200;
        dxAccum = saturatingAdd(dxAccum, dx);
        dyAccum = saturatingAdd(dyAccum, dy);
        if (!armed) {
            if (abs(dxAccum) < DEADZONE && abs(dyAccum) < DEADZONE) return;
            armed = true;
        }

        long speedLong = ((long)abs(dx) + abs(dy)) * 100L / elapsed;
        int speed = speedLong > Integer.MAX_VALUE ? Integer.MAX_VALUE : (int)speedLong;
        int threshold;
        if (speed <= SLOW_SPEED) threshold = SLOW_THRESHOLD;
        else if (speed >= FAST_SPEED) threshold = FAST_THRESHOLD;
        else threshold = SLOW_THRESHOLD - (speed - SLOW_SPEED)
              * (SLOW_THRESHOLD - FAST_THRESHOLD) / (FAST_SPEED - SLOW_SPEED);

        /* Bound emission per sample: prevent a malformed input sample from
         * generating an unbounded synchronous burst into the DSI adapter. */
        int emitted = 0;
        while (emitted < MAX_TICKS_PER_SAMPLE &&
               (abs(dxAccum) >= threshold || abs(dyAccum) >= threshold)) {
            /* Largest accumulated axis first, respecting signed direction. */
            if (abs(dxAccum) >= abs(dyAccum) && abs(dxAccum) >= threshold) {
                int key = dxAccum < 0 ? LEFT : RIGHT;
                dxAccum += dxAccum < 0 ? threshold : -threshold;
                sink.direction(key);
            } else if (abs(dyAccum) >= threshold) {
                int key = dyAccum < 0 ? UP : DOWN;
                dyAccum += dyAccum < 0 ? threshold : -threshold;
                sink.direction(key);
            }
            emitted++;
        }
        /* A very large or corrupt movement is not replayed across later
         * samples: preserve at most one pending tick of each direction. */
        dxAccum = clamp(dxAccum, -threshold + 1, threshold - 1);
        dyAccum = clamp(dyAccum, -threshold + 1, threshold - 1);
    }

    private static int clamp(int value, int min, int max) {
        return value < min ? min : (value > max ? max : value);
    }
    private static int abs(int value) {
        return value == Integer.MIN_VALUE ? Integer.MAX_VALUE :
               (value < 0 ? -value : value);
    }
    private static int saturatingAdd(int a, int b) {
        long c = (long)a + (long)b;
        if (c > Integer.MAX_VALUE) return Integer.MAX_VALUE;
        if (c < Integer.MIN_VALUE) return Integer.MIN_VALUE;
        return (int)c;
    }
}
