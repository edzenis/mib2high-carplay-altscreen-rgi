package org.mib2high.carplay.rgi;
import java.lang.reflect.Field;
import com.luka.carplay.framework.Log;
import com.luka.carplay.routeguidance.TouchpadSwipeCore;
import com.luka.carplay.routeguidance.AltCoreTouchpadPort;
import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.smartphone.carplay.CarPlayDSIManager;
import de.audi.app.terminalmode.dsi.carplay.CarplayDSILifecycleController;
import org.dsi.ifc.carplay.DSICarplay;
/** K2161-only physical CarPlay DPAD output. */
public final class K2161CarPlayButtonOutput implements AltCoreTouchpadPort {
    private static final String TAG = "K2161Swipe";
    private volatile boolean enabled;
    private IContext tmContext;
    public void configure(Object ctx, boolean on) { tmContext=ctx instanceof IContext?(IContext)ctx:null; enabled=on; }
    public void direction(int direction) {
        if (!enabled) return;
        int id;
        switch (direction) {
            case TouchpadSwipeCore.LEFT: id=5; break; // stock DSI JS_WEST
            case TouchpadSwipeCore.RIGHT: id=6; break; // stock DSI JS_EAST
            case TouchpadSwipeCore.UP: id=7; break; // stock DSI JS_NORTH
            case TouchpadSwipeCore.DOWN: id=8; break; // stock DSI JS_SOUTH
            default: return;
        }
        try {
            IContext ctx = tmContext;
            if (ctx == null) return;
            Object mgr = ctx.getCarPlayDSIManager();
            if (!(mgr instanceof CarPlayDSIManager)) return;
            Field lifecycleField = CarPlayDSIManager.class.getDeclaredField("carPlayDSILifecycleController");
            lifecycleField.setAccessible(true);
            Object lc = lifecycleField.get(mgr);
            if (!(lc instanceof CarplayDSILifecycleController)) return;
            Field dsiField = CarplayDSILifecycleController.class.getDeclaredField("dsiCarplay");
            dsiField.setAccessible(true);
            Object proxy = dsiField.get(lc);
            if (!(proxy instanceof DSICarplay)) return;
            DSICarplay carplay = (DSICarplay)proxy;
            carplay.postButtonEvent(id,0);
            carplay.postButtonEvent(id,1);
        } catch (Throwable t) { Log.e(TAG, "SWIPE_DSI_SEND_FAILED", t); }
    }

}
