package org.mib2high.carplay.rgi;

import java.lang.reflect.Field;
import com.luka.carplay.framework.Log;
import com.luka.carplay.routeguidance.TouchpadSwipeCore;
import com.luka.carplay.routeguidance.AltCoreTouchpadPort;
import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.keyevents.ITerminalModeDSIKeyEventsController;
import de.audi.app.terminalmode.keyevents.TMKeyEventsHandler;
import de.audi.app.terminalmode.keyevents.Key;
import de.audi.app.terminalmode.keyevents.KeyState;

/** K2161-specific input/output adapter. No Audi details belong in TouchpadSwipeCore.
 * Observe the actual stock TM key-event delegate, forward its existing calls,
 * and emit ordinary CarPlay DPAD button events only for single-finger pad motion.
 * All reflection field names below were verified against K2161/MU1421 JXE.
 */
public final class K2161TouchpadAdapter {
    private static final String TAG = "K2161Swipe";
    private static final int TOUCHINPUT_PAD = 1; // verified TMKeyEventsHandler 9->1
    private final TouchpadSwipeCore swipe;
    private final AltCoreTouchpadPort output;
    private IContext tmContext;
    private TMKeyEventsHandler handler;
    private ITerminalModeDSIKeyEventsController original;
    private ITerminalModeDSIKeyEventsController wrapper;
    private volatile boolean enabled;
    private int attachmentEpoch;

    public K2161TouchpadAdapter(AltCoreTouchpadPort output) {
        if (output==null) throw new IllegalArgumentException("output=null");
        this.output=output;
        swipe = new TouchpadSwipeCore(new TouchpadSwipeCore.Sink() {
            public void direction(int direction) { K2161TouchpadAdapter.this.output.direction(direction); }
        });
    }

    public synchronized void attach(Object tmService) {
        detach();
        if (tmService == null || !"de.audi.app.terminalmode.TMInterAppHandler".equals(tmService.getClass().getName())) return;
        try {
            Field f = tmService.getClass().getDeclaredField("context");
            if (f.getType() != IContext.class) throw new IllegalStateException("context type mismatch");
            f.setAccessible(true);
            tmContext = (IContext)f.get(tmService);
            output.configure(tmContext, enabled);
            handler = tmContext != null ? tmContext.getKeyEventsHandler() : null;
            Log.i(TAG, "K2161_TOUCHPAD_ATTACHED context=" + (tmContext != null));
            final int epoch = attachmentEpoch;
            Thread watcher = new Thread(new Runnable() {
                public void run() {
                    for (;;) {
                        try { Thread.sleep(350); } catch (InterruptedException ex) { return; }
                        synchronized (K2161TouchpadAdapter.this) {
                            if (epoch != attachmentEpoch || tmContext == null) return;
                            if (enabled) installIfReady();
                        }
                    }
                }
            }, "K2161-Swipe-Attach");
            watcher.setDaemon(true);
            watcher.start();
        } catch (Throwable ex) {
            Log.e(TAG, "K2161_TOUCHPAD_ATTACH_FAILED", ex);
            tmContext = null; handler = null;
        }
    }

    public synchronized void setEnabled(boolean active) {
        if (!active) { enabled = false; output.configure(tmContext,false); swipe.release(); uninstall(); return; }
        enabled = true;
        output.configure(tmContext,true);
        installIfReady();
    }

    public synchronized void detach() {
        attachmentEpoch++;
        enabled = false;
        swipe.release();
        uninstall();
        handler = null; tmContext = null; output.configure(null,false);
    }

    private void installIfReady() {
        if (handler == null || !enabled) return;
        try {
            Field f = TMKeyEventsHandler.class.getDeclaredField("dsiKeyEventController");
            f.setAccessible(true);
            Object delegate = f.get(handler);
            if (delegate == wrapper) return;
            if (!(delegate instanceof ITerminalModeDSIKeyEventsController) ||
                    delegate.getClass().getName().indexOf("NullTerminalModeDSIKeyEventsController") >= 0) {
                Log.w(TAG, "SWIPE_DEFERRED no active TM key delegate");
                return;
            }
            original = (ITerminalModeDSIKeyEventsController)delegate;
            wrapper = new Interceptor(original);
            handler.setDSIKeyEventController(wrapper);
            Log.i(TAG, "SWIPE_INPUT_WRAPPER_INSTALLED source=" + original.getClass().getName());
        } catch (Throwable t) { Log.e(TAG, "SWIPE_INSTALL_FAILED", t); }
    }

    private void uninstall() {
        if (handler == null || wrapper == null) { wrapper=null; original=null; return; }
        try {
            Field f = TMKeyEventsHandler.class.getDeclaredField("dsiKeyEventController");
            f.setAccessible(true);
            if (f.get(handler) == wrapper && original != null)
                handler.setDSIKeyEventController(original);
        } catch (Throwable t) { Log.e(TAG, "SWIPE_UNINSTALL_FAILED", t); }
        wrapper=null; original=null;
    }

    private final class Interceptor implements ITerminalModeDSIKeyEventsController {
        private final ITerminalModeDSIKeyEventsController next;
        Interceptor(ITerminalModeDSIKeyEventsController n) { next=n; }
        public void updateKey(Key key, KeyState state) { next.updateKey(key,state); }
        public void updateRotary(int count) { next.updateRotary(count); }
        public void updateCharacterEvent(String[] chars, int[] ids) { next.updateCharacterEvent(chars,ids); }
        public void updateTouchEvent(int inputId,int fingers,int gesture,int x,int y,int x2,int y2) {
            /* Preserve the stock DSI input path, including normal touchpad reports. */
            next.updateTouchEvent(inputId,fingers,gesture,x,y,x2,y2);
            if (!enabled || inputId != TOUCHINPUT_PAD || fingers != 1) swipe.release();
            else swipe.sample(x,y,System.currentTimeMillis());
        }
    }
}
