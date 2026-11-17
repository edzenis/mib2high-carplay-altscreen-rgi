/*
 * Firmware-independent single-consumer work queue for route-guidance snapshots.
 * No navigation interpretation or car API is referenced here. Every queued work
 * item MUST be an immutable, complete snapshot owned by its producer.
 *
 * This is NOT the complete AltScreen RGI fix: it requires source-level integration
 * of the upstream parser and downstream output adapter, with verification
 * of which semantic updates can safely be coalesced.
 */
package com.luka.carplay.routeguidance;

public final class GuidanceWorkQueue {
    public static final int ACCEPTED = 1;
    public static final int COALESCED = 2;
    public static final int REJECTED = 0;

    public interface Handler { void handle(Object immutableSnapshot); }
    public interface Failure { void handlerFailed(Throwable error); }

    private static final class Work {
        final Object value;
        final long epoch;
        final int coalesceKey;
        Work(Object v, long e, int key) { value = v; epoch = e; coalesceKey = key; }
    }

    private final Handler handler;
    private final Failure failure;
    private final Work[] ring;
    private final Thread consumer;
    private int start;
    private int size;
    private boolean running = true;
    private boolean busy;
    private long epoch = 1;
    private long completed;
    private long replaced;
    private long rejected;

    public GuidanceWorkQueue(int capacity, Handler h, Failure f) {
        if (capacity < 2 || capacity > 8192 || h == null)
            throw new IllegalArgumentException("capacity/handler");
        ring = new Work[capacity];
        handler = h;
        failure = f;
        consumer = new Thread(new Runnable() {
            public void run() { process(); }
        }, "Guidance-Output");
        consumer.setDaemon(true);
        consumer.start();
    }

    /* Caller passes coalesceKey=0 for transitions, maneuvers, route start/stop,
     * disconnects, and ANY ambiguous/partial delta. Nonzero coalesceKey is
     * permitted only for a proved, complete, replaceable state snapshot.
     * A replacement can occur at the tail only: never across a barrier. */
    public synchronized int offer(Object immutableSnapshot, long expectedEpoch, int coalesceKey) {
        if (!running || immutableSnapshot == null || expectedEpoch != epoch) {
            rejected++;
            return REJECTED;
        }
        if (size > 0 && coalesceKey != 0) {
            int tail = (start + size - 1) % ring.length;
            Work old = ring[tail];
            if (old.coalesceKey == coalesceKey && old.epoch == expectedEpoch) {
                ring[tail] = new Work(immutableSnapshot, expectedEpoch, coalesceKey);
                replaced++;
                notifyAll();
                return COALESCED;
            }
        }
        if (size == ring.length) {
            rejected++;
            return REJECTED; /* Must be surfaced by input adapter, never silent. */
        }
        ring[(start + size) % ring.length] = new Work(immutableSnapshot, expectedEpoch, coalesceKey);
        size++;
        notifyAll();
        return ACCEPTED;
    }

    public synchronized long epoch() { return epoch; }

    /* Disconnect, new RGI session, or explicit reset: discard old work.
     * Cannot abort an output invocation already running; output adapter must
     * independently guard session/ownership before committing output operations. */
    public synchronized long advanceEpoch() {
        epoch++;
        clearPending();
        notifyAll();
        return epoch;
    }

    private void clearPending() {
        for (int i = 0; i < ring.length; i++) ring[i] = null;
        start = 0;
        size = 0;
    }

    public synchronized int pending() { return size; }
    public synchronized long completed() { return completed; }
    public synchronized long coalesced() { return replaced; }
    public synchronized long rejected() { return rejected; }

    public synchronized boolean awaitIdle(long timeoutMs) throws InterruptedException {
        long deadline = System.currentTimeMillis() + timeoutMs;
        while ((busy || size > 0) && timeoutMs > 0) {
            wait(timeoutMs);
            timeoutMs = deadline - System.currentTimeMillis();
        }
        return !busy && size == 0;
    }

    /** Stop without joining a blocked consumer: a failing peripheral output
     * must never stall the real-time guidance worker. */
    public synchronized void shutdownNoWait() {
        running=false;
        clearPending();
        notifyAll();
    }

    public void shutdown() throws InterruptedException {
        synchronized (this) {
            running = false;
            clearPending();
            notifyAll();
        }
        consumer.join(2000);
    }

    private void process() {
        for (;;) {
            Work next;
            synchronized (this) {
                while (running && size == 0) {
                    try { wait(); } catch (InterruptedException ignored) { }
                }
                if (!running) return;
                next = ring[start];
                ring[start] = null;
                start = (start + 1) % ring.length;
                size--;
                busy = true;
            }
            try {
                /* Input adapter must use immutable snapshot values and output
                 * adapter must guard epoch/session when mutating vehicle state. */
                synchronized (this) {
                    if (next.epoch != epoch) continue;
                }
                handler.handle(next.value);
            } catch (Throwable t) {
                if (failure != null) {
                    try { failure.handlerFailed(t); } catch (Throwable ignored) { }
                }
            } finally {
                synchronized (this) {
                    busy = false;
                    completed++;
                    notifyAll();
                }
            }
        }
    }
}
