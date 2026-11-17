/*
 * CarPlay Route Guidance - BAP Bridge
 *
 * Translates real CarPlay route-guidance state to the exact K2161 BAP API.
 * Scope: factory HUD plus the cluster's shared/simple VC route-guidance presentation.
 * No custom Virtual Cockpit graphics renderer is used by this bridge.
 *
 */
package com.luka.carplay.routeguidance;

import com.luka.carplay.framework.Log;
import de.audi.atip.interapp.combi.bap.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.data.CombiBAPDestinationInfo;
import de.audi.atip.interapp.combi.bap.data.CombiBAPNaviDestination;
import de.audi.atip.interapp.combi.bap.data.CombiBAPNaviLaneGuidanceData;
import de.audi.atip.interapp.combi.bap.data.CombiBAPNaviManeuverDescriptor;
import de.audi.atip.log.LogChannel;
import de.audi.atip.metrics.DateMetric;
import de.audi.atip.metrics.Distance;
import de.audi.tghu.navi.app.Navigation;
import de.audi.tghu.navi.app.cluster.BAPDistanceFormatter;
import de.audi.tghu.navi.app.cluster.ClusterService;
import de.audi.tghu.navi.app.command.DSIResponseContainer;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

public class BAPBridge {

    private static final String TAG = "BAPBridge";
    /* RGType sent to cluster: 0=RGI (BAP ManeuverDescriptor icons for HUD).
     * FPK has rgType=4 hardcoded in CombiBAPListener -- the BAP rgType=0 is for the
     * AppConnectorNavi FSG sync flow, not for view mode selection. */
    private static final int ACTIVE_RGTYPE = 0;  /* RGI -- native BAP HUD icons. */
    private static final boolean BAP_TRACE_ENABLED = true;

    /* The factory VC and HUD both consume the same CarPlay BAP guidance. */
    private static final int VC_VIEW_RGI = 1;

    /* Allow small rounding differences before treating the top-level distance as belonging
     * to a different (typically START_ROUTE) presentation state. */

    /* BAP supports three maneuver slots, but publish only the current one. */
    private static final int MAX_BAP_MANEUVERS = 1;

    /* Presentation thresholds used only for maneuver state / street text.
     * The real maneuver icon and numeric distance are published at all ranges. */
    private static final int CITY_PREPARE_THRESHOLD_M = 1500;
    private static final int HIGHWAY_PREPARE_THRESHOLD_M = 3000;
    private static final int HIGHWAY_STEP_THRESHOLD_M = 2000;

    private CombiBAPServiceNavi appConnectorNavi;
    private final BAPDistanceFormatter distanceFormatter =
        new BAPDistanceFormatter(new SilentLogChannel());

    private boolean initialized = false;

    /* Approach state affects only maneuver-state emphasis and turn-to text. */
    private boolean inApproachZone = false;
    private int lastFirstManeuverIdx = -1;
    private int lastFirstManeuverVer = -1;
    private String latchedTurnToText = "";
    private final Object distanceToManeuverLock = new Object();
    private boolean hasLastDistM = false;
    private int lastDistM = 0;
    private K2161RouteGuidanceOwnership ownership;

    /* K2161 output-only deduplication. Per-frame CarPlay deltas remain ordered
     * and unmodified; only identical visible BAP presentation is suppressed. */
    private String lastInfoBarText = null;
    private boolean hasLastDestinationBap = false;
    private int lastDestinationValue, lastDestinationUnit;
    private boolean lastDestinationStopOver;
    private boolean hasLastTimeBap = false;
    private int lastTimeInfoType, lastTimeFormat;
    private long lastTimeValue;

    private void resetPresentationCache() {
        lastInfoBarText = null;
        hasLastDestinationBap = false;
        hasLastTimeBap = false;
    }

    private ClusterService csRef;

    /*
     * Temporary VC presentation ownership.
     *
     * K2161's ClusterInputListener rejects RGI as a directly user-selectable
     * FPK mode, while ClusterViewMode itself supports the automatic RGI mode.
     * Save the existing favored state so CarPlay can temporarily override it
     * and restore it exactly when guidance is released.
     */
    private boolean vcFactoryRgStatePropagated = false;
    private boolean vcPresentationOverrideActive = false;
    private int vcSavedFavoredViewMode = 0;
    private boolean vcSavedFavoredViewModeReceived = false;

    private Field vcFavoredViewModeField;
    private Field vcFavoredViewModeReceivedField;

    private static final class SilentLogChannel extends LogChannel {
        public void log(int level, String pattern,
                        Object a, Object b, Object c, Object d,
                        long l1, long l2, long l3, int flags, Throwable t) {
            /* no-op */
        }
        public void log(int level, int messageId,
                        Object a, Object b, Object c, Object d,
                        long l1, long l2, long l3, int flags, Throwable t) {
            /* no-op */
        }
    }

    private static final class FormattedDistance {
        final int value;
        final int unit;

        FormattedDistance(int value, int unit) {
            this.value = value;
            this.unit = unit;
        }
    }

    /* One authoritative maneuver selection for descriptor, distance and presentation state. */
    

    /**
     * Send distance to maneuver through AppConnectorNavi using native formatter rules.
     */
    private void sendDistanceToManeuverRaw(int meters, boolean bargraphOn, int bargraph) throws Exception {
        synchronized (distanceToManeuverLock) {
            if (meters > 0) {
                hasLastDistM = true;
                lastDistM = meters;
            } else {
                hasLastDistM = false;
                lastDistM = 0;
            }
        }

        /* Keep the BAP distance bargraph disabled for direct RGI output. */
        FormattedDistance fd = formatDistanceToTurn(meters);
        traceBap("updateDistanceToNextManeuver",
            fd.value + "," + fd.unit + ",false,0");
        /* AltScreen: no synchronous factory reflection/file I/O in the HUD hot path. */
        appConnectorNavi.updateDistanceToNextManeuver(fd.value, fd.unit, false, 0);

    }

    private void sendDistanceToDestinationRaw(int meters, boolean isStopOver) {
        FormattedDistance fd = formatDistanceToDestination(meters);
        if (hasLastDestinationBap && lastDestinationValue == fd.value &&
                lastDestinationUnit == fd.unit && lastDestinationStopOver == isStopOver) return;
        traceBap("updateDistanceToDestination", fd.value + "," + fd.unit + "," + isStopOver);
        appConnectorNavi.updateDistanceToDestination(fd.value, fd.unit, isStopOver);
        hasLastDestinationBap = true;
        lastDestinationValue = fd.value;
        lastDestinationUnit = fd.unit;
        lastDestinationStopOver = isStopOver;
    }

    private FormattedDistance formatDistanceToTurn(int meters) {
        if (meters <= 0) return new FormattedDistance(-1, 0);
        try {
            boolean metric = isMetricDistanceUnits();
            /* K2161 metric unit 0 is 0.1 m.  The stock turn formatter clamps
             * short values; preserve the actual CarPlay distance below 1 km. */
            if (metric && meters < 1000) {
                return new FormattedDistance(meters * 10, 0);
            }
            /* BAPDistanceFormatter$BAPDistance is public, but the outer class .class file
             * lacks the InnerClasses attribute (decompiler artifact), so javac can't
             * resolve BAPDistanceFormatter.BAPDistance as a type.  Use Object + getValue/getUnit. */
            Object d = distanceFormatter.formatDistanceToTurn(meters, metric);
            int v = ((Integer) d.getClass().getMethod("getValue", new Class[0]).invoke(d, new Object[0])).intValue();
            int u = ((Integer) d.getClass().getMethod("getUnit", new Class[0]).invoke(d, new Object[0])).intValue();
            return new FormattedDistance(v, u);
        } catch (Throwable t) {
            Log.w(TAG, "formatDistanceToTurn failed, using invalid distance: " + t.getMessage());
            return new FormattedDistance(-1, 0);
        }
    }

    private FormattedDistance formatDistanceToDestination(int meters) {
        if (meters <= 0) return new FormattedDistance(-1, 0);
        try {
            boolean metric = isMetricDistanceUnits();
            Object d = distanceFormatter.formatDistanceToDestination(meters, metric);
            int v = ((Integer) d.getClass().getMethod("getValue", new Class[0]).invoke(d, new Object[0])).intValue();
            int u = ((Integer) d.getClass().getMethod("getUnit", new Class[0]).invoke(d, new Object[0])).intValue();
            return new FormattedDistance(v, u);
        } catch (Throwable t) {
            Log.w(TAG, "formatDistanceToDestination failed, using invalid distance: " + t.getMessage());
            return new FormattedDistance(-1, 0);
        }
    }

    private static boolean isMetricDistanceUnits() {
        try {
            int unit = Distance.getSystemUnit();
            return unit == Distance.KM || unit == Distance.METERS || unit == Distance.NONE;
        } catch (Throwable t) {
            return true;
        }
    }

    private void traceBap(String call, String args) {
        if (!BAP_TRACE_ENABLED) return;
        Log.d(TAG, "[BAP] " + call + "(" + args + ")");
    }

    /** Capture the factory VC preference before CarPlay changes it. */
    private void captureVcPresentationState() throws Exception {
        if (vcPresentationOverrideActive) return;

        if (csRef == null || csRef.getClusterViewMode() == null) {
            throw new Exception("ClusterViewMode unavailable");
        }

        Object cvm = csRef.getClusterViewMode();
        Class cls = cvm.getClass();

        vcFavoredViewModeField =
            cls.getDeclaredField("favoredViewMode");
        vcFavoredViewModeReceivedField =
            cls.getDeclaredField("favoredViewModeReceived");

        vcFavoredViewModeField.setAccessible(true);
        vcFavoredViewModeReceivedField.setAccessible(true);

        vcSavedFavoredViewMode =
            vcFavoredViewModeField.getInt(cvm);
        vcSavedFavoredViewModeReceived =
            vcFavoredViewModeReceivedField.getBoolean(cvm);

        vcPresentationOverrideActive = true;

        Log.i(TAG,
            "VC state captured: favored=" + vcSavedFavoredViewMode
            + " received=" + vcSavedFavoredViewModeReceived);
    }

    /**
     * Select a factory K2161 ClusterViewMode directly.
     *
     * Do not route this through ClusterInputListener: on FPK that user-input
     * path validates RGI away. ClusterViewMode.setFavoredViewMode() is the
     * underlying OEM state-machine input and immediately refreshes the view.
     */
    private void setVcFavoredViewMode(int mode, String reason)
        throws Exception {
        K2161VcTrace.record("VC_SET_FAVORED_BEGIN mode=" + mode + " reason=" + reason);

        if (csRef == null || csRef.getClusterViewMode() == null) {
            throw new Exception("ClusterViewMode unavailable");
        }

        csRef.getClusterViewMode().setFavoredViewMode(mode);
        K2161FactoryTrace.snapshot("after_favored_view_mode", csRef);
        K2161VcTrace.record("VC_SET_FAVORED_RETURNED_OK mode=" + mode);

        Log.i(TAG,
            "VC favored mode=" + mode + " reason=" + reason);
    }

    /** Save the user's VC presentation before enabling factory route guidance. */
    private void prepareVcPresentation() throws Exception {
        captureVcPresentationState();
        K2161VcTrace.record("CAPTURE_STATE_OK favored=" + vcSavedFavoredViewMode);
    }

    /** Always propagate active CarPlay navigation to the factory VC. */
    private void configureVcPresentation() throws Exception {
        K2161VcTrace.record("UPDATE_RG_ACTIVE_BEGIN");
        csRef.updateRgActive(true);
        vcFactoryRgStatePropagated = true;
        K2161FactoryTrace.snapshot("after_rg_active_true", csRef);
        K2161VcTrace.record("UPDATE_RG_ACTIVE_RETURNED_OK");
        setVcFavoredViewMode(VC_VIEW_RGI, "CarPlay factory RGI");
        K2161VcTrace.record("SET_FAVORED_RGI_RETURNED_OK");
        Log.i(TAG, "VC policy active: FACTORY_RGI_AND_HUD");
    }

    /** Preserve full factory VC navigation during the active CarPlay route. */
    private void maintainVcPresentation() {
        if (!vcPresentationOverrideActive || csRef == null || vcFavoredViewModeField == null) return;
        try {
            Object cvm = csRef.getClusterViewMode();
            if (cvm == null) return;
            int current = vcFavoredViewModeField.getInt(cvm);
            if (current != VC_VIEW_RGI) {
                setVcFavoredViewMode(VC_VIEW_RGI, "reassert after external VC mode change");
                K2161VcTrace.record("VC_MODE_REASSERT_RETURNED_OK");
            }
        } catch (Throwable t) {
            Log.w(TAG, "VC presentation maintenance failed: "
                + t.getClass().getName() + ": " + t.getMessage());
        }
    }

    /** Restore the exact favored VC state captured before CarPlay takeover. */
    private void restoreVcPresentation() {
        K2161VcTrace.record("RESTORE override=" + vcPresentationOverrideActive);
        if (!vcPresentationOverrideActive) return;

        try {
            if (csRef != null
                    && csRef.getClusterViewMode() != null) {

                Object cvm =
                    csRef.getClusterViewMode();

                csRef.getClusterViewMode().setFavoredViewMode(
                    vcSavedFavoredViewMode
                );
                K2161VcTrace.record("RESTORE_FAVORED_RETURNED_OK mode="
                    + vcSavedFavoredViewMode);

                /*
                 * setFavoredViewMode() marks the value as explicitly received.
                 * Restore that bookkeeping flag as well so shutdown leaves the
                 * stock state exactly as it was before CarPlay takeover.
                 */
                if (vcFavoredViewModeReceivedField != null) {
                    vcFavoredViewModeReceivedField.setBoolean(
                        cvm,
                        vcSavedFavoredViewModeReceived
                    );
                }

                Log.i(TAG,
                    "VC state restored: favored="
                    + vcSavedFavoredViewMode
                    + " received="
                    + vcSavedFavoredViewModeReceived);
            }
        } catch (Throwable t) {
            Log.w(TAG,
                "VC state restore failed: "
                + t.getClass().getName() + ": "
                + t.getMessage());
        } finally {
            vcPresentationOverrideActive = false;
            vcFavoredViewModeField = null;
            vcFavoredViewModeReceivedField = null;
        }
    }

    private void traceDescriptor(int outPos, int idx, int type, int main, int dir, int zLevel, byte[] sideStreets) {
        if (!BAP_TRACE_ENABLED) return;
        StringBuffer sb = new StringBuffer();
        sb.append("pos=").append(outPos)
          .append(" idx=").append(idx)
          .append(" type=").append(type)
          .append(" main=").append(main)
          .append(" dir=").append(dir)
          .append(" z=").append(zLevel)
          .append(" side=[");
        if (sideStreets != null) {
            for (int i = 0; i < sideStreets.length; i++) {
                if (i > 0) sb.append(',');
                sb.append(sideStreets[i] & 0xFF);
            }
        }
        sb.append(']');
        Log.d(TAG, "[BAP] descriptor " + sb.toString());
    }

    /* ============================================================
     * Initialization
     * ============================================================ */

    public boolean init(Object naviService) {
        if (initialized) return true;

        try {
            if (!(naviService instanceof CombiBAPServiceNavi)) {
                String cls = (naviService != null) ? naviService.getClass().getName() : "null";
                Log.e(TAG, "Init failed: service is not CombiBAPServiceNavi (" + cls + ")");
                return false;
            }
            this.appConnectorNavi = (CombiBAPServiceNavi) naviService;

            initialized = true;
            initClusterAccess(); /* installs the native gate open when Navigation is ready */
            Log.i(TAG, "Initialized successfully (raw AppConnectorNavi): " + naviService.getClass().getName());
            return true;

        } catch (Exception e) {
            Log.e(TAG, "Init failed", e);
            return false;
        }
    }

    /**
     * Get ClusterService via Navigation singleton and install native BAP gate.
     * Non-fatal - if this fails, native RG stream won't be blocked.
     */
    private void initClusterAccess() {
        try {
            Navigation navi = Navigation.getInstance();
            if (navi == null) {
                Log.w(TAG, "ClusterAccess: Navigation.getInstance() returned null");
                return;
            }
            ClusterService cs = navi.getClusterService();
            if (cs == null) {
                Log.w(TAG, "ClusterAccess: ClusterService is null");
                return;
            }
            this.csRef = cs;
            /* Install the exact K2161 dispatcher-safe ownership gate, initially open. */
            try {
                ownership = new K2161RouteGuidanceOwnership(appConnectorNavi);
                if (ownership.install()) {
                    Log.i(TAG, "Exact K2161 route-guidance ownership gate installed open");
                } else {
                    ownership = null;
                    Log.w(TAG, "K2161 ownership gate not ready");
                }
            } catch (Exception ex) {
                ownership = null;
                Log.w(TAG, "K2161 ownership gate install failed: " + ex.getMessage());
            }

            Log.i(TAG, "ClusterAccess init OK");
        } catch (Exception e) {
            Log.w(TAG, "ClusterAccess setup failed (non-fatal): " + e.getMessage());
        }
    }

    /* Preserve the real DSI rgActive value while CarPlay temporarily owns it. */
    private boolean rgActiveForced = false;
    private boolean rgActiveSaved = false;
    /**
     * Set the K2161 route-information acceptance state used by the HUD path.
     */
    private void forceClusterRouteInfoState(boolean active) {
        if (csRef == null) return;

        if (!active
                && !rgActiveForced
                && !vcFactoryRgStatePropagated) {
            return;
        }

        try {
            Navigation nav = Navigation.getInstance();
            DSIResponseContainer container = (nav != null) ? nav.getDsiResponseContainer() : null;
            if (container != null) {
                if (active) {
                    if (!rgActiveForced) {
                        rgActiveSaved = container.isRgActive();
                        rgActiveForced = true;
                    }
                    container.setRgActive(true);
                } else if (rgActiveForced) {
                    rgActiveForced = false;
                    container.setRgActive(rgActiveSaved);
                    Log.i(TAG, "rgActive overlay released (restored " + rgActiveSaved + ")");
                }
            }
        } catch (Exception e) {
            Log.d(TAG, "force container rgActive failed: " + e.getMessage());
        }

        if (active) {
            K2161FactoryTrace.snapshot("before_rgi_valid_on", csRef);
            try { csRef.updateRGIString(new short[]{1}); }
            catch (Exception e) { Log.d(TAG, "force rgiValid=true failed: " + e.getMessage()); }
            K2161FactoryTrace.snapshot("after_rgi_valid_on", csRef);
        } else {
            K2161FactoryTrace.snapshot("before_rgi_valid_off", csRef);
            try { csRef.updateRGIString(null); }
            catch (Exception e) { Log.d(TAG, "force rgiValid=false failed: " + e.getMessage()); }
            K2161FactoryTrace.snapshot("after_rgi_valid_off", csRef);

            /*
             * Only the VC-enabled path propagates ClusterService RG-active
             * state. Restore that factory state symmetrically on release.
             */
            if (vcFactoryRgStatePropagated) {
                try {
                    csRef.updateRgActive(rgActiveSaved);
                    Log.i(TAG,
                        "Factory VC RG state restored to "
                        + rgActiveSaved);
                } catch (Exception e) {
                    Log.w(TAG,
                        "Factory VC RG-state restore failed: "
                        + e.getMessage());
                } finally {
                    vcFactoryRgStatePropagated = false;
                }
            }
        }
    }


    /* ============================================================
     * Lifecycle
     * ============================================================ */

    public boolean onStart() {
        if (!initialized) return false;

        try {
            inApproachZone = false;
            lastFirstManeuverIdx = -1;
            lastFirstManeuverVer = -1;
            latchedTurnToText = "";
            resetPresentationCache();
            synchronized (distanceToManeuverLock) {
                hasLastDistM = false;
                lastDistM = 0;
            }

            /*
             * Lazy-init cluster hooks (native stream gate).
             * Navigation singleton may not be available at init() time.
             */
            if (ownership == null) {
                initClusterAccess();
            }

            /* Native Audi navigation is never cancelled or stopped. */

            /* Block native route-guidance BAP stream during CarPlay RG. */
            if (ownership == null || !ownership.setCarPlayRouteActive(true)) {
                Log.w(TAG, "Route ownership not ready; no CarPlay BAP emitted");
                return false;
            }

            /*
             * Capture/prepare VC state before enabling the shared HUD
             * route-information acceptance overlay.
             */
            prepareVcPresentation();

            /*
             * Factory VC-RGI mode uses the proven K2161
             * rgActive/RGI-valid acceptance state.
             */
            forceClusterRouteInfoState(true);

            /*
             * Guidance start -- K2161 HUD route-guidance BAP transaction.
             *
             * 1. RGStatus(1) - FctID 17 -> triggers startSync(0) for {17,39,23,18,49}
             * 2. Complete sync(0) window: rgType(39), descriptor(23), distance(18), exitView(49)
             */
            traceBap(
                "updateRGStatusAndActiveRGType",
                "1," + ACTIVE_RGTYPE
            );

            appConnectorNavi.updateRGStatusAndActiveRGType(
                1,
                ACTIVE_RGTYPE
            );

            /* Keep HUD active and configure whether VC may consume the shared RGI presentation. */
            configureVcPresentation();

            /* Sync(0) FctIDs: descriptor, distance, exitView */
            sendNoSymbol();                                                         /* FctID 23 */
            sendDistanceToManeuverRaw(0, false, 0);                                  /* FctID 18 */


            /* Non-sync FctIDs */
            traceBap("updateCurrentPositionInfo", "\"\"");
            try {
                appConnectorNavi.updateCurrentPositionInfo("");                       /* FctID 19 */
                lastInfoBarText = "";
            } catch (Throwable t) {
                Log.w(TAG, "BAP FctID 19 failed during start: "
                    + t.getClass().getName() + ": " + t.getMessage());
            }

            Log.i(TAG, "Started (rgType=" + ACTIVE_RGTYPE + ", display=VC+HUD)");
            return true;

        } catch (Throwable e) {
            Log.e(TAG, "onStart error: " + e.getClass().getName() + ": " + e.getMessage());

            try { forceClusterRouteInfoState(false); }
            catch (Throwable ignored) { }

            restoreVcPresentation();

            try {
                if (ownership != null && ownership.isCarPlayRouteActive()) {
                    ownership.setCarPlayRouteActive(false);
                }
            } catch (Throwable ignored) { }

            return false;
        }
    }

    public void onStop() {
        if (!initialized) return;

        try {
            /* Lightweight stop — reset internal state only.
             * No BAP teardown. iOS sends transient route_state=0
             * during maneuver transitions; full teardown causes HUD flicker. BAP teardown happens in onShutdown() on real disconnect. */
            inApproachZone = false;
            lastFirstManeuverIdx = -1;
            lastFirstManeuverVer = -1;
            latchedTurnToText = "";
            resetPresentationCache();
            Log.d(TAG, "Stopped (lightweight, BAP kept alive)");
        } catch (Exception e) {
            Log.e(TAG, "onStop error", e);
        }
    }

    /**
     * Full shutdown — BAP teardown and native ownership hand-back.
     * Called on actual CarPlay disconnect or stop().
     */
    public void onShutdown() {
        if (!initialized) return;

        try {

            /*
             * Guidance stop — full BAP teardown:
             * 1. RGStatus(0) - triggers sync(0) for {17,39,23,18,49}
             * 2. Complete sync(0) window: descriptor(23), distance(18), exitView(49)
             * 3. Non-sync FctIDs last
             */
            resetPresentationCache(); /* Route teardown must explicitly clear factory infobar. */
            traceBap("updateRGStatusAndActiveRGType", "0,0");
            appConnectorNavi.updateRGStatusAndActiveRGType(0, 0);
            sendNoSymbol();
            sendDistanceToManeuverRaw(0, false, 0);

            traceBap("updateManeuverState", "0");
            appConnectorNavi.updateManeuverState(0);
            traceBap("updateCurrentPositionInfo", "\"\"");
            try { appConnectorNavi.updateCurrentPositionInfo(""); } catch (Throwable t) {}
            sendDistanceToDestinationRaw(0, false);
            traceBap("updateTimeToDestination", "0,0,-1");
            appConnectorNavi.updateTimeToDestination(0, 0, -1);
            traceBap("updateLaneGuidance", "[],false");
            appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);

            /* Restore the real DSI value (if still held) and always clear our
             * RGIString even if DSI was already released at the 30s boundary. */
            forceClusterRouteInfoState(false);

            /* Restore the user's pre-CarPlay VC presentation preference. */
            restoreVcPresentation();

            /* Clear CarPlay BAP first, then reopen native RG and force the
             * exact K2161 ClusterService setter/updateAll hand-back. */
            if (ownership != null && ownership.isCarPlayRouteActive()) {
                ownership.setCarPlayRouteActive(false);
            }

            Log.i(TAG, "Shutdown (full teardown)");
        } catch (Exception e) {
            Log.e(TAG, "onShutdown error", e);
        }
    }

    /** Full bundle/service teardown: hand native ClusterService its raw service back. */
    public synchronized void dispose() {
        onShutdown();
        if (ownership != null) {
            ownership.restoreBestEffort();
            ownership = null;
        }
        initialized = false;
        appConnectorNavi = null;
        csRef = null;
        Log.i(TAG, "Disposed; raw K2161 CombiBAPServiceNavi restored");
    }

    /* ============================================================
     * Update
     * ============================================================ */

    public void update(AltCoreRouteState s) {
        if (!initialized || s == null) return;

        int dirty = s.dirtyMask;
        if (dirty == 0) return;
        Log.d(TAG, "Update delta mask=0x" + Integer.toHexString(dirty));

        try {
            /*
             * Keep dev.4 VC policy authoritative during the active
             * CarPlay route.
             */
            maintainVcPresentation();

            /*
             * Explicit clear: count dropped to 0.  But only treat it as a real clear
             * if the route itself is inactive (routeState < 1).
             */
            boolean explicitClear = ((dirty & AltCoreRouteState.DIRTY_MANEUVER_COUNT) != 0)
                && (s.maneuverCount == 0)
                && (s.routeState <= 0);
            int[] idxs = K2161ManeuverAdapter.getManeuverIndexList(s);
            boolean hasManeuverList = (idxs != null && idxs.length > 0);
            boolean hasAnyManeuver = (s.maneuverCount > 0);
            boolean shouldClearManeuver = (s.maneuverCount == 0) && (s.routeState <= 0);

            AltCoreManeuverPolicy.PresentationManeuver presentation = K2161ManeuverAdapter.resolvePresentationManeuver(s);
            int firstIdx = presentation.index;
            int type0 = presentation.type;
            int distM = presentation.distanceM;
            boolean showManeuver = ManeuverMapper.isValidType(type0);

            if (presentation.usedSlotDistance) {
                Log.d(TAG, "Presentation distance aligned to selected maneuver idx=" + firstIdx
                    + " live=" + presentation.liveDistanceM + "m slot="
                    + presentation.slotDistanceM + "m");
            }

            /* Keep a lightweight approach state for Audi maneuver-state emphasis
             * and turn-to street text only.  It never gates the real icon or distance. */
            int rawStepM = (firstIdx >= 0 && s.mDistance != null
                    && firstIdx < s.mDistance.length) ? s.mDistance[firstIdx] : -1;
            boolean isHighway;
            if (rawStepM > 0) {
                isHighway = rawStepM > HIGHWAY_STEP_THRESHOLD_M;
            } else {
                isHighway = (type0 >= 0) && ManeuverMapper.isHighwayManeuver(type0);
            }
            int prepareThreshold = isHighway ? HIGHWAY_PREPARE_THRESHOLD_M : CITY_PREPARE_THRESHOLD_M;
            int actionThresholdM = K2161ManeuverAdapter.getActionThresholdM(s, firstIdx, prepareThreshold);

            int currentFirstVer = (firstIdx >= 0 && s.mVer != null
                    && firstIdx < s.mVer.length) ? s.mVer[firstIdx] : -1;
            boolean primaryChanged = (firstIdx != lastFirstManeuverIdx)
                || (currentFirstVer != lastFirstManeuverVer);
            if (primaryChanged) {
                inApproachZone = false;
                lastFirstManeuverIdx = firstIdx;
                lastFirstManeuverVer = currentFirstVer;
            }

            boolean hasUsableDistance = distM > 0;
            boolean isArrival = (type0 == ManeuverMapper.MT_ARRIVE_END_OF_NAVIGATION
                || type0 == ManeuverMapper.MT_ARRIVE_AT_DESTINATION
                || type0 == ManeuverMapper.MT_ARRIVE_END_OF_DIRECTIONS
                || type0 == ManeuverMapper.MT_ARRIVE_DESTINATION_LEFT
                || type0 == ManeuverMapper.MT_ARRIVE_DESTINATION_RIGHT);
            boolean nowApproach = isArrival
                || (hasUsableDistance ? (distM <= prepareThreshold) : inApproachZone);
            boolean approachChanged = hasUsableDistance
                && (nowApproach != inApproachZone)
                && showManeuver && hasManeuverList;
            if (approachChanged) {
                dirty |= AltCoreRouteState.DIRTY_DIST_MAN
                       | AltCoreRouteState.DIRTY_LANE_GUIDANCE
                       | AltCoreRouteState.DIRTY_MANEUVER_TEXT
                       | AltCoreRouteState.DIRTY_MANEUVER_ICON;
                inApproachZone = nowApproach;
                latchedTurnToText = "";
                Log.i(TAG, "Approach zone " + (nowApproach ? "ENTER" : "EXIT")
                    + " (dist=" + distM + "m, highway=" + isHighway
                    + ", prepThr=" + prepareThreshold + ", actionThr=" + actionThresholdM + ")");
            }

            if (explicitClear || shouldClearManeuver) {
                latchedTurnToText = "";
            }

            /*
             * 1. Maneuver icons (FctID 23)
             */
            if ((dirty & (AltCoreRouteState.DIRTY_MANEUVER_ICON |
                          AltCoreRouteState.DIRTY_MANEUVER_LIST |
                          AltCoreRouteState.DIRTY_MANEUVER_COUNT)) != 0) {
                if (explicitClear) {
                    sendNoSymbol();
                } else if (hasManeuverList) {
                    if (showManeuver) {
                        sendManeuvers(s, firstIdx);
                    } else if (shouldClearManeuver) {
                        sendNoSymbol();
                    } else if (hasAnyManeuver) {
                        Log.d(TAG, "Slot data pending for list, keeping last icons (count=" + s.maneuverCount + ")");
                    }
                } else if (shouldClearManeuver) {
                    sendNoSymbol();
                } else if (hasAnyManeuver) {
                    Log.d(TAG, "Maneuver list missing (count=" + s.maneuverCount + "), keep last icons");
                }
            }

            /*
             * 2. ExitView is published atomically with every descriptor through
             *    updateManeuverDescriptorAndExitView(..., 0, 0), matching stock K2161.
             */
            /*
             * 3. Distance to maneuver (FctID 18)
             *
             * Sub-kilometer metric values are encoded directly; other values use the OEM formatter.
             */
            if ((dirty & (AltCoreRouteState.DIRTY_DIST_MAN |
                          AltCoreRouteState.DIRTY_MANEUVER_ICON |
                          AltCoreRouteState.DIRTY_MANEUVER_LIST |
                          AltCoreRouteState.DIRTY_MANEUVER_COUNT)) != 0) {
                boolean transientNoDistance = (distM <= 0)
                    && !shouldClearManeuver
                    && !explicitClear
                    && hasManeuverList
                    && hasAnyManeuver;
                if (transientNoDistance) {
                    boolean haveCached;
                    int cachedDistM;
                    synchronized (distanceToManeuverLock) {
                        haveCached = hasLastDistM;
                        cachedDistM = lastDistM;
                    }
                    if (haveCached) {
                        sendDistanceToManeuverRaw(cachedDistM, false, 0);
                    } else {
                        sendDistanceToManeuverRaw(0, false, 0);
                    }
                } else if (distM <= 0 || shouldClearManeuver) {
                    sendDistanceToManeuverRaw(0, false, 0);
                } else {
                    sendDistanceToManeuverRaw(distM, false, 0);
                }
            }

            /* 4. Native Audi infobar: ETA / remaining travel time / distance only.
             * Never publish next-street or maneuver text into FctID 19.
             * The HUD FctIDs 17/18/23 are completely unchanged.
             */
            if ((dirty & (AltCoreRouteState.DIRTY_ETA |
                          AltCoreRouteState.DIRTY_TIME_REMAINING |
                          AltCoreRouteState.DIRTY_DIST_DEST |
                          AltCoreRouteState.DIRTY_ROUTE_STATE)) != 0) {
                long localEtaMs = -1L;
                if (s.etaSeconds >= 0) {
                    try {
                        Navigation navi = Navigation.getInstance();
                        if (navi != null && navi.getEnv() != null
                                && navi.getEnv().getFramework() != null) {
                            localEtaMs = convertUtcToLocalMs(((long)s.etaSeconds) * 1000L);
                        }
                    } catch (Throwable t) {
                        Log.w(TAG, "Vehicle-local ETA unavailable: " + t.getMessage());
                    }
                }
                String trip = AltCoreTripSummary.format(s, localEtaMs, isMetricDistanceUnits());
                trip = limitUtf8(trip, 48); /* K2161 native Fct19 narrow VC text area */
                Log.d(TAG, "ALTTRIP_INFOBAR text_len=" + trip.length() + " eta_utc=" +
                      s.etaSeconds + " local_ms=" + localEtaMs +
                      " remaining_s=" + s.timeRemainingSeconds +
                      " dest_m=" + s.distDestM);
                if (lastInfoBarText == null || !lastInfoBarText.equals(trip)) {
                    traceBap("updateCurrentPositionInfo", "\"" + trip + "\"");
                    appConnectorNavi.updateCurrentPositionInfo(trip);
                    lastInfoBarText = trip;
                }
            }

            /*
             * 5. Maneuver state (FctID 24) - for HUD
             */
            if ((dirty & (AltCoreRouteState.DIRTY_MANEUVER_STATE |
                          AltCoreRouteState.DIRTY_MANEUVER_ICON |
                          AltCoreRouteState.DIRTY_MANEUVER_LIST |
                          AltCoreRouteState.DIRTY_MANEUVER_COUNT |
                          AltCoreRouteState.DIRTY_DIST_MAN)) != 0) {
                if (explicitClear) {
                    traceBap("updateManeuverState", "0");
                    appConnectorNavi.updateManeuverState(0);
                } else if (shouldClearManeuver || (hasManeuverList && showManeuver)) {
                    int bapState;
                    if (showManeuver) {
                        if (!nowApproach) {
                            bapState = 1;   /* Follow */
                        } else if (hasUsableDistance && actionThresholdM > 0 && distM <= actionThresholdM) {
                            bapState = 4;   /* Action */
                        } else {
                            bapState = 2;   /* Prepare */
                        }
                    } else {
                        bapState = 0;
                    }
                    traceBap("updateManeuverState", String.valueOf(bapState));
                    appConnectorNavi.updateManeuverState(bapState);
                } else if (hasManeuverList && hasAnyManeuver) {
                    traceBap("updateManeuverState", "1");
                    appConnectorNavi.updateManeuverState(1);
                } else if (hasAnyManeuver && !hasManeuverList) {
                    Log.d(TAG, "Maneuver state unchanged: list missing (count=" + s.maneuverCount + ")");
                }
            }

            /*
             * 6. Lane guidance (FctID 24)
             */
            int laneRecomputeMask = AltCoreRouteState.DIRTY_LANE_GUIDANCE
                | AltCoreRouteState.DIRTY_MANEUVER_LIST
                | AltCoreRouteState.DIRTY_MANEUVER_COUNT;
            if ((dirty & laneRecomputeMask) != 0) {
                /* Trust iOS's display intent — `laneGuidanceShowing` is the
                 * authoritative signal.  iOS sets it to 1 only when its own
                 * navigation manager (MNGuidanceManager._considerLaneGuidance)
                 * has decided to show lanes for the user, based on the lane
                 * event's startValidRouteCoordinate / endValidRouteCoordinate
                 * range.  This naturally covers:
                 *   - highway pre-positioning (shows 1-2 km early)
                 *   - active lane choice (right at the maneuver)
                 *   - hide on exit (showing→0 once past endValidRouteCoordinate)
                 * Do not add a local distance gate here; iOS owns lane visibility. */
                boolean wantLaneGuidance = !explicitClear
                    && !shouldClearManeuver
                    && s.laneGuidanceShowing == 1;
                if (wantLaneGuidance) {
                    sendLaneGuidance(s);
                } else {
                    traceBap("updateLaneGuidance", "[],false");
                    appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
                }
            }

            /* 7. Distance to destination (FctID 21) */
            if ((dirty & AltCoreRouteState.DIRTY_DIST_DEST) != 0) {
                sendDistanceToDestinationRaw(s.distDestM, false);
            }

            /* 8. Time to destination (FctID 22) - via AppConnectorNavi */
            if ((dirty & (AltCoreRouteState.DIRTY_TIME_REMAINING |
                          AltCoreRouteState.DIRTY_ETA)) != 0) {
                long eta = s.etaSeconds;
                int timeInfoType = 1;
                int timeFormat = getHuNavigationTimeFormat();
                long timeVal;

                if (eta >= 0) {
                    timeVal = eta;
                } else if (s.timeRemainingSeconds >= 0) {
                    long nowMs = getUtcMillis();
                    timeVal = (nowMs / 1000L) + s.timeRemainingSeconds;
                } else {
                    timeVal = -1;
                }

                /* JVM default TZ is UTC on MHI2. AppConnectorNavi uses
                 * GregorianCalendar(Date(epoch*1000)) which would show UTC.
                 * Convert UTC epoch to local epoch so calendar shows local time.
                 * Uses K2161 HU UTC offset with Latvia DST-aware fallback. */
                if (timeVal >= 0) {
                    long utcMs = timeVal * 1000L;
                    long localMs = convertUtcToLocalMs(utcMs);
                    timeVal = localMs / 1000L;
                }

                /* Factory ETA display has minute resolution. Do not issue another
                 * synchronous BAP write for each changing raw ETA second. */
                boolean sameTime = hasLastTimeBap && lastTimeInfoType == timeInfoType &&
                    lastTimeFormat == timeFormat &&
                    ((timeVal < 0 && lastTimeValue == timeVal) ||
                     (timeVal >= 0 && lastTimeValue >= 0 &&
                      timeVal / 60L == lastTimeValue / 60L));
                if (!sameTime) {
                    traceBap("updateTimeToDestination", timeInfoType + "," + timeFormat + "," + timeVal);
                    appConnectorNavi.updateTimeToDestination(timeInfoType, timeFormat, timeVal);
                    hasLastTimeBap = true;
                    lastTimeInfoType = timeInfoType;
                    lastTimeFormat = timeFormat;
                    lastTimeValue = timeVal;
                }
            }

            /* 9. Destination info (FctID 46) - via AppConnectorNavi */
            if ((dirty & AltCoreRouteState.DIRTY_DESTINATION) != 0) {
                String dest = (s.destination != null) ? s.destination : "";
                dest = limitUtf8(dest, 50);
                CombiBAPNaviDestination naviDest =
                    new CombiBAPNaviDestination("", "", dest, "", "", "", "");
                CombiBAPDestinationInfo destInfo = new CombiBAPDestinationInfo(naviDest);
                traceBap("updateDestinationInfo", "\"" + dest + "\"");
                appConnectorNavi.updateDestinationInfo(destInfo);
            }


            Log.d(TAG, "Update: dist=" + distM + "m maneuvers="
                + Math.min(s.maneuverCount, MAX_BAP_MANEUVERS)
                + " selectedIdx=" + firstIdx
                + " distSource=" + (presentation.usedSlotDistance ? "slot" : "live")
                + " startSkipped=" + presentation.skippedStartRoute);

        } catch (Exception e) {
            Log.e(TAG, "update error", e);
        }
    }

    /* ============================================================
     * Maneuver Sending (current maneuver for HUD)
     * ============================================================ */

    /**
     * Send FOLLOW_STREET descriptor for the K2161 startup sync transaction only.
     */
    private void sendFollowStreet() throws Exception {
        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[1];
        arr[0] = createDescriptor(ManeuverMapper.FOLLOW_STREET, ManeuverMapper.DIR_STRAIGHT, 0, new byte[0]);
        traceBap("updateManeuverDescriptorAndExitView", "count=1 FOLLOW_STREET exit=0,0");
        /* AltScreen: BAP descriptor trace remains in traceBap(); avoid reflective file snapshots here. */
        appConnectorNavi.updateManeuverDescriptorAndExitView(arr, 0, 0);

    }

    /**
     * Send NO_SYMBOL descriptor.
     */
    private void sendNoSymbol() throws Exception {
        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[1];
        arr[0] = createDescriptor(0, 0, 0, new byte[0]);
        traceBap("updateManeuverDescriptorAndExitView", "count=1 NO_SYMBOL exit=0,0");
        /* AltScreen: BAP descriptor trace remains in traceBap(); avoid reflective file snapshots here. */
        appConnectorNavi.updateManeuverDescriptorAndExitView(arr, 0, 0);

    }

    /**
     * Send only the current (first valid) maneuver from iOS maneuverOrder.
     */
    private void sendManeuvers(AltCoreRouteState s, int idx) throws Exception {
        if (idx < 0) {
            if (s.maneuverCount == 0) sendNoSymbol();
            else Log.d(TAG, "No presentable authoritative maneuver yet; keeping last icon");
            return;
        }

        int[] mapped = ManeuverMapper.map(
            s.mType[idx], s.mTurnAngle[idx],
            s.mJunctionType[idx], s.mDrivingSide[idx]);
        int zLevel = (s.mZLevel != null && idx < s.mZLevel.length) ? s.mZLevel[idx] : 0;
        byte[] sideStreets;
        if (mapped[0] == ManeuverMapper.NO_INFO || mapped[0] == ManeuverMapper.NO_SYMBOL) {
            sideStreets = new byte[0];
        } else {
            sideStreets = SideStreets.calcSideStreetsBytes(
                s.mType[idx], s.mJunctionType[idx], s.mDrivingSide[idx],
                s.mJunctionAngles[idx], s.mExitAngle[idx]);
        }

        Log.d(TAG, "[BAP] mapinput idx=" + idx
            + " type=" + s.mType[idx]
            + " turnAngle=" + s.mTurnAngle[idx]
            + " junctionType=" + s.mJunctionType[idx]
            + " drivingSide=" + s.mDrivingSide[idx]
            + " distM=" + s.distManeuverM);
        traceDescriptor(0, idx, s.mType[idx], mapped[0], mapped[1], zLevel, sideStreets);

        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[1];
        arr[0] = createDescriptor(mapped[0], mapped[1], zLevel, sideStreets);
        traceBap("updateManeuverDescriptorAndExitView", "count=1 exit=0,0");
        /* AltScreen: BAP descriptor trace remains in traceBap(); avoid reflective file snapshots here. */
        appConnectorNavi.updateManeuverDescriptorAndExitView(arr, 0, 0);

        Log.d(TAG, "Sent 1 maneuver");
    }

    /* ============================================================
     * Lane Guidance (FctID 24)
     * ============================================================ */

    private void sendLaneGuidance(AltCoreRouteState s) throws Exception {
        int idx = resolveLaneGuidanceManeuverIndex(s);
        int count = laneCountForManeuver(s, idx);
        boolean hasRealData = hasLaneGuidanceForManeuver(s, idx);

        if (hasRealData) {
            sendRealLaneGuidance(s, idx, count);
        } else {
            traceBap("updateLaneGuidance", "[],false");
            appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
        }
    }

    private void sendRealLaneGuidance(AltCoreRouteState s, int idx, int count) throws Exception {
        int n = Math.min(count, 8);
        CombiBAPNaviLaneGuidanceData[] tmp = new CombiBAPNaviLaneGuidanceData[n];
        int out = 0;

        for (int i = 0; i < n; i++) {
            short lanePos = mapLanePosition(s, idx, i);
            short laneDir = mapLaneDirection(s, idx, i);
            if (!shouldEmitLane(s, idx, i, laneDir)) {
                continue;
            }
            byte[] laneSideStreets = mapLaneSideStreets(s, idx, i, laneDir);
            byte gi = mapGuidanceInfo(s, idx, i);
            Log.d(TAG, "  lane[" + i + "] pos=" + lanePos + " dir=0x"
                + Integer.toHexString(laneDir & 0xFF) + " gi=" + gi
                + " sideStreets=" + hexBytes(laneSideStreets));
            tmp[out++] = new CombiBAPNaviLaneGuidanceData(
                lanePos, laneDir, laneSideStreets, (short) 0,
                (byte) 0, (byte) 0, (byte) 0, gi);
        }

        if (out <= 0) {
            traceBap("updateLaneGuidance", "[],false");
            appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
            return;
        }

        CombiBAPNaviLaneGuidanceData[] arr = tmp;
        if (out != n) {
            arr = new CombiBAPNaviLaneGuidanceData[out];
            for (int i = 0; i < out; i++) arr[i] = tmp[i];
        }

        traceBap("updateLaneGuidance", "real,count=" + out
            + ",slot=" + idx
            + ",current=" + s.laneGuidanceIndex
            + ",mapped=" + s.laneGuidanceSlot);
        appConnectorNavi.updateLaneGuidance(true, arr);
    }

    private static boolean hasLaneGuidanceForManeuver(AltCoreRouteState s, int manIdx) {
        if (manIdx < 0) return false;
        if (hasLaneCacheForSlot(s, manIdx)) return true;
        if (s.mLaneDirections == null || manIdx >= s.mLaneDirections.length) return false;
        if (s.mLaneDirections[manIdx] == null) return false;
        return laneCountForManeuver(s, manIdx) > 0;
    }

    private static boolean hasLaneCacheForSlot(AltCoreRouteState s, int slot) {
        if (slot < 0) return false;
        if (s.lgLaneDirections == null || slot >= s.lgLaneDirections.length) return false;
        if (s.lgLaneDirections[slot] == null) return false;
        return laneCacheCountForSlot(s, slot) > 0;
    }

    /* Verify lg-cache slot's stored event id still matches the active idx.
     * Needed because lg cache slots are LRU-allocated independently of
     * maneuver slot numbers, so a slot can be remapped to a different
     * event after eviction. */
    private static boolean lgSlotMatches(AltCoreRouteState s, int slot, int idx) {
        if (slot < 0 || s.lgIndex == null || slot >= s.lgIndex.length) return false;
        return s.lgIndex[slot] == idx;
    }

    /* m-cache-only check (skips lg-cache).  Used in legacy fallback paths
     * where we need m-cache data without aliasing into lg-cache that
     * happens to occupy the same numeric slot. */
    private static boolean hasMCacheLaneForSlot(AltCoreRouteState s, int slot) {
        if (slot < 0) return false;
        if (s.mLaneDirections == null || slot >= s.mLaneDirections.length) return false;
        if (s.mLaneDirections[slot] == null) return false;
        int count = -1;
        if (s.mLaneCount != null && slot < s.mLaneCount.length) count = s.mLaneCount[slot];
        if (count <= 0) count = s.mLaneDirections[slot].length;
        return count > 0;
    }

    private static int laneCacheCountForSlot(AltCoreRouteState s, int slot) {
        if (slot < 0) return 0;
        int count = 0;
        if (s.lgLaneCount != null && slot < s.lgLaneCount.length) {
            count = s.lgLaneCount[slot];
        }
        if (count <= 0 && s.lgLaneDirections != null && slot < s.lgLaneDirections.length
            && s.lgLaneDirections[slot] != null) {
            count = s.lgLaneDirections[slot].length;
        }
        if (count <= 0 && s.lgLaneStatus != null && slot < s.lgLaneStatus.length
            && s.lgLaneStatus[slot] != null) {
            count = s.lgLaneStatus[slot].length;
        }
        return Math.max(count, 0);
    }

    private static int laneSlotForGuidanceIndex(AltCoreRouteState s, int guidanceIndex) {
        if (guidanceIndex < 0 || s.lgIndex == null) return -1;
        for (int i = 0; i < s.lgIndex.length; i++) {
            if (s.lgIndex[i] == guidanceIndex && hasLaneCacheForSlot(s, i)) {
                return i;
            }
        }
        return -1;
    }

    /* Slot ids are shared by the legacy m-cache fallback and the lg-cache
     * arrays.  Keep both caches in the same bounded numeric slot range. */
    private static int[] lanePositionsFor(AltCoreRouteState s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLanePositions != null && slot < s.lgLanePositions.length)
            return s.lgLanePositions[slot];
        if (s.mLanePositions != null && slot >= 0 && slot < s.mLanePositions.length)
            return s.mLanePositions[slot];
        return null;
    }

    private static int[] laneDirectionsFor(AltCoreRouteState s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLaneDirections != null && slot < s.lgLaneDirections.length)
            return s.lgLaneDirections[slot];
        if (s.mLaneDirections != null && slot >= 0 && slot < s.mLaneDirections.length)
            return s.mLaneDirections[slot];
        return null;
    }

    private static int[] laneStatusFor(AltCoreRouteState s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLaneStatus != null && slot < s.lgLaneStatus.length)
            return s.lgLaneStatus[slot];
        if (s.mLaneStatus != null && slot >= 0 && slot < s.mLaneStatus.length)
            return s.mLaneStatus[slot];
        return null;
    }

    private static int[][] laneAnglesFor(AltCoreRouteState s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLaneAngles != null && slot < s.lgLaneAngles.length)
            return s.lgLaneAngles[slot];
        if (s.mLaneAngles != null && slot >= 0 && slot < s.mLaneAngles.length)
            return s.mLaneAngles[slot];
        return null;
    }

    private static int laneCountForManeuver(AltCoreRouteState s, int manIdx) {
        if (manIdx < 0) return 0;
        if (hasLaneCacheForSlot(s, manIdx)) {
            return laneCacheCountForSlot(s, manIdx);
        }
        int count = 0;
        if (s.mLaneCount != null && manIdx < s.mLaneCount.length) {
            count = s.mLaneCount[manIdx];
        }
        if (count <= 0 && s.mLaneDirections != null && manIdx < s.mLaneDirections.length
            && s.mLaneDirections[manIdx] != null) {
            count = s.mLaneDirections[manIdx].length;
        }
        if (count <= 0 && s.mLaneStatus != null && manIdx < s.mLaneStatus.length
            && s.mLaneStatus[manIdx] != null) {
            count = s.mLaneStatus[manIdx].length;
        }
        if (count < 0) count = 0;
        return count;
    }

    private static int resolveLaneGuidanceManeuverIndex(AltCoreRouteState s) {
        /*
         * iOS exposes the currently displayed lane guidance as a top-level
         * currentLaneGuidanceIndex (0x5201 InfoType 16 — written by Maps.app
         * in setCurrentLaneGuidanceIndex: from CarMetadataNavigationListener
         * each location update).  0x5204's TLV1 is iOS's
         * composedGuidanceEventIndex — an EVENT IDENTIFIER, sent for every
         * cached event including future precache.  So the active is purely
         * 0x5201, and we must look up the lg-cache slot whose stored event
         * id matches the active.
         *
         * lg cache slot indices are NOT the same numeric space as maneuver
         * slot indices: in C, rgd_lane_slot_for_iap_index() maintains an
         * independent LRU.  After eviction, lg slot N may hold any cached
         * event id, not necessarily event N — which means resolving by
         * "primary maneuver slot" via lg-cache silently sends data for
         * the wrong event.  Validate s.lgIndex[slot] == active before
         * trusting any lg-cache slot.
         */

        int activeIdx = s.laneGuidanceIndex;
        if (activeIdx < 0) return -1;

        if (s.laneGuidanceSlot >= 0 && hasLaneCacheForSlot(s, s.laneGuidanceSlot)
            && lgSlotMatches(s, s.laneGuidanceSlot, activeIdx)) {
            return s.laneGuidanceSlot;
        }

        int byIndex = laneSlotForGuidanceIndex(s, activeIdx);
        if (byIndex >= 0) return byIndex;

        /*
         * Linked-lane fallback: C publishes mN_linked_lane_guidance_slot
         * pointing at a real lg-cache slot (not a maneuver slot), so it's
         * safe to consult directly.  Only use it when its stored event id
         * still matches the active (the linked field can outlive a cache
         * remap of its target).
         */
        int primary = K2161ManeuverAdapter.getFirstManeuverIndex(s);
        int linked = linkedLaneSlotForManeuver(s, primary);
        if (linked >= 0 && hasLaneCacheForSlot(s, linked)
            && lgSlotMatches(s, linked, activeIdx)) {
            return linked;
        }

        /*
         * Legacy m-cache fallback for compatibility with pre-lg-split
         * hooks (which wrote lane data into mN_lane_*).  Skip if the
         * primary slot has any lg-cache data — that lg entry could be
         * unrelated event data (post-eviction) and m-cache lookup with
         * the same numeric slot would alias into it.
         */
        int max = (s.mLaneCount != null) ? s.mLaneCount.length : 0;
        if (activeIdx < max && hasMCacheLaneForSlot(s, activeIdx)
            && !hasLaneCacheForSlot(s, activeIdx)) {
            return activeIdx;
        }
        if (primary >= 0 && hasMCacheLaneForSlot(s, primary)
            && !hasLaneCacheForSlot(s, primary)) {
            return primary;
        }

        return -1;
    }



    private static int linkedLaneSlotForManeuver(AltCoreRouteState s, int manIdx) {
        if (manIdx < 0) return -1;
        int max = (s.mLaneCount != null) ? s.mLaneCount.length : 0;

        if (s.mLinkedLaneGuidanceSlot != null && manIdx < s.mLinkedLaneGuidanceSlot.length) {
            int slot = s.mLinkedLaneGuidanceSlot[manIdx];
            if (slot >= 0 && slot < max) return slot;
        }

        if (s.mLinkedLaneGuidanceIndex != null && manIdx < s.mLinkedLaneGuidanceIndex.length) {
            int linkedIdx = s.mLinkedLaneGuidanceIndex[manIdx];
            if (linkedIdx >= 0 && linkedIdx < max) return linkedIdx;
        }
        return -1;
    }

    private static short mapLanePosition(AltCoreRouteState s, int manIdx, int laneIdx) {
        int[] pos = lanePositionsFor(s, manIdx);
        if (pos == null || laneIdx < 0 || laneIdx >= pos.length) {
            return (short) laneIdx;
        }
        int v = pos[laneIdx];
        if (v < 0 || v > 0x7FFF) return (short) laneIdx;
        return (short) v;
    }

    private static boolean hasLaneAnglesForLane(AltCoreRouteState s, int manIdx, int laneIdx) {
        int[][] lanes = laneAnglesFor(s, manIdx);
        if (lanes == null || lanes.length == 0) return false;
        int sel = (laneIdx >= 0 && laneIdx < lanes.length) ? laneIdx : 0;
        int[] angles = lanes[sel];
        return (angles != null && angles.length > 0);
    }

    private static boolean shouldEmitLane(AltCoreRouteState s, int manIdx, int laneIdx, short laneDir) {
        int[] dirs = laneDirectionsFor(s, manIdx);
        if (dirs == null || laneIdx < 0 || laneIdx >= dirs.length) return true;
        int raw = dirs[laneIdx];

        if (raw == 1000 && !hasLaneAnglesForLane(s, manIdx, laneIdx)) return false;
        return laneDir != (short)0xFF;
    }

    private static final int[] LANE_DIR_NATIVE_ANGLES = {
        -180, -135, -90, -45, 0, 45, 90, 135, 180
    };
    private static final int[] LANE_DIR_NATIVE_CODES = {
        0x72, 0x60, 0x40, 0x20, 0x00, 0xE0, 0xC0, 0xA0, 0x92
    };

    private static int mapRawLaneValueToDirectionCode(int raw) {
        return mapRawLaneValueToDirectionCode(raw, false, 0);
    }

    private static int mapRawLaneValueToDirectionCode(int raw, boolean excludeOneNativeKey, int keyToExclude) {
        if (raw == 1000) return 0xFF;

        int bestCode = 0;
        int bestDiff = 100000;
        for (int i = 0; i < LANE_DIR_NATIVE_ANGLES.length; i++) {
            int key = LANE_DIR_NATIVE_ANGLES[i];
            if (excludeOneNativeKey && key == keyToExclude) continue;
            int d = raw - key;
            if (d < 0) d = -d;
            if (d < bestDiff) {
                bestDiff = d;
                bestCode = LANE_DIR_NATIVE_CODES[i];
            }
        }
        return bestCode;
    }

    private static short mapLaneDirectionFromSentinel(AltCoreRouteState s, int manIdx, int laneIdx) {
        if (manIdx < 0) return (short) 0xFF;

        int[][] laneAngles = laneAnglesFor(s, manIdx);
        if (laneAngles != null && laneAngles.length > 0) {
            int sel = (laneIdx >= 0 && laneIdx < laneAngles.length) ? laneIdx : 0;
            int[] angles = laneAngles[sel];
            if (angles != null && angles.length > 0) {
                return (short)(mapRawLaneValueToDirectionCode(angles[0]) & 0xFF);
            }
        }
        return (short) 0xFF;
    }

    private static short mapLaneDirection(AltCoreRouteState s, int manIdx, int laneIdx) {
        int[] dirs = laneDirectionsFor(s, manIdx);
        if (dirs == null || laneIdx < 0 || laneIdx >= dirs.length) return (short)0xFF;
        int raw = dirs[laneIdx];

        if (raw == 1000) {
            return mapLaneDirectionFromSentinel(s, manIdx, laneIdx);
        }

        return (short)(mapRawLaneValueToDirectionCode(raw) & 0xFF);
    }

    private static byte[] mapLaneSideStreets(AltCoreRouteState s, int manIdx, int laneIdx, short laneDirection) {
        if (manIdx < 0) return new byte[0];
        int[][] lanes = laneAnglesFor(s, manIdx);
        if (lanes == null || lanes.length == 0) return new byte[0];
        int sel = (laneIdx >= 0 && laneIdx < lanes.length) ? laneIdx : 0;
        int[] angles = lanes[sel];
        if (angles == null || angles.length == 0) return new byte[0];

        int start = 0;
        int[] dirs = laneDirectionsFor(s, manIdx);
        if (dirs != null && laneIdx >= 0 && laneIdx < dirs.length && dirs[laneIdx] == 1000) {
            start = 1;
        }
        if (start >= angles.length) return new byte[0];

        int primaryDir = laneDirection & 0xFF;
        int[] codes = new int[angles.length - start];
        int n = 0;
        for (int i = start; i < angles.length; i++) {
            int code = mapRawLaneValueToDirectionCode(angles[i]) & 0xFF;
            if (code == 0xFF) continue;
            /* Skip angles that map to the same BAP direction as the primary --
             * they're not additional directions, just the same lane. */
            if (code == primaryDir) continue;

            boolean dup = false;
            for (int j = 0; j < n; j++) {
                if (codes[j] == code) {
                    dup = true;
                    break;
                }
            }
            if (!dup) codes[n++] = code;
        }
        if (n == 0) return new byte[0];

        for (int i = 1; i < n; i++) {
            int key = codes[i];
            int j = i - 1;
            while (j >= 0 && codes[j] > key) {
                codes[j + 1] = codes[j];
                j--;
            }
            codes[j + 1] = key;
        }

        byte[] out = new byte[n];
        for (int i = 0; i < n; i++) out[i] = (byte)(codes[i] & 0xFF);
        return out;
    }

    private static byte mapGuidanceInfo(AltCoreRouteState s, int manIdx, int laneIdx) {
        int[] status = laneStatusFor(s, manIdx);
        if (status == null || laneIdx < 0 || laneIdx >= status.length) return 0;
        int v = status[laneIdx];
        if (v < 0 || v > 2) return 0;
        return (byte)v;
    }

    /**
     * Create a single CombiBAPNaviManeuverDescriptor.
     */
    private CombiBAPNaviManeuverDescriptor createDescriptor(int main, int dir, int zLevel, byte[] sideStreets) {
        int mappedMain = main;
        if (main == ManeuverMapper.PREPARE_TURN &&
            (dir == ManeuverMapper.DIR_STRAIGHT || dir == ManeuverMapper.DIR_LEFT)) {
            mappedMain = ManeuverMapper.CHANGE_LANE;
        }

        /* Direction is used as-is - ManeuverMapper.applyDsiNavBapDirectionOverride()
         * already handles per-type coarsening.  No second coarsening layer needed. */
        int mappedDir = dir;

        if (sideStreets == null) sideStreets = new byte[0];
        int mappedZ = (zLevel == 1 || zLevel == 2) ? zLevel : 0;
        return new CombiBAPNaviManeuverDescriptor(mappedMain, mappedDir, mappedZ, sideStreets);
    }

    /* ============================================================
     * Utilities
     * ============================================================ */

    private static long getUtcMillis() {
        return System.currentTimeMillis();
    }

    private static int getHuNavigationTimeFormat() {
        try {
            int v = DateMetric.timeFormat;
            int bap = (v == 11) ? 1 : 0;
            Log.d(TAG, "timeFormat DateMetric.timeFormat=" + v + " bap=" + bap);
            return bap;
        } catch (Throwable t) {
            Log.d(TAG, "timeFormat access failed: " + t.getMessage());
        }
        return 0;
    }

    /**
     * Convert UTC millis using the K2161 system's current timezone offset.
     * The HU offset is not guaranteed initialized: SysApp.ClockHandler starts
     * with zero until a UTCOffset callback. For this Latvian Q7, a configurable
     * Europe/Riga timezone fallback is applied in that case, with diagnostics.
     * This is firmware output policy; never introduce zone logic in the core.
     */
    private static long lastLoggedOffset = Long.MIN_VALUE;
    private static String lastLoggedOffsetOrigin = "";
    private static long convertUtcToLocalMs(long utcMs) {
        long offset = 0L;
        String origin = "HU";
        try {
            Navigation navi = Navigation.getInstance();
            if (navi != null && navi.getEnv() != null &&
                navi.getEnv().getFramework() != null) {
                offset = navi.getEnv().getFramework().getCurrentTimezoneOffsetMilliseconds();
            }
        } catch (Throwable t) {
            Log.w(TAG, "ALTTRIP_CLOCK hu_offset_query_failed=" + t.getClass().getName());
        }
        /* SysApp ClockHandler defaults to 0 before UTCOffset events arrive.
         * K2161's JVM defaults to UTC and offers no local fallback on its own.
         * This specific Latvian Q7 is configured for Europe/Riga. Other owners
         * may override the fallback via system property altscreen.eta.zone.
         * Use DST rules for the ETA instant, not today's current offset. */
        if (offset == 0L) {
            String zoneId = System.getProperty("altscreen.eta.zone", "Europe/Riga");
            java.util.TimeZone zone = java.util.TimeZone.getTimeZone(zoneId);
            offset = zone.getOffset(utcMs);
            origin = "ZONE_FALLBACK:" + zone.getID();
        }
        if (offset < -14L*3600000L || offset > 14L*3600000L) {
            Log.w(TAG, "ALTTRIP_CLOCK offset_out_of_range=" + offset);
            return utcMs;
        }
        if (offset != lastLoggedOffset || !origin.equals(lastLoggedOffsetOrigin)) {
            Log.i(TAG, "ALTTRIP_CLOCK offset_ms=" + offset + " source=" + origin +
                " utc=" + utcMs + " local=" + (utcMs + offset));
            lastLoggedOffset = offset;
            lastLoggedOffsetOrigin = origin;
        }
        return utcMs + offset;
    }

    private static String hexBytes(byte[] b) {
        if (b == null || b.length == 0) return "[]";
        StringBuffer sb = new StringBuffer("[");
        for (int i = 0; i < b.length; i++) {
            if (i > 0) sb.append(",");
            sb.append("0x");
            sb.append(Integer.toHexString(b[i] & 0xFF));
        }
        sb.append("]");
        return sb.toString();
    }

    private static String limitUtf8(String s, int maxBytes) {
        if (s == null) return "";
        if (maxBytes <= 0) return "";
        try {
            byte[] b = s.getBytes("UTF-8");
            if (b.length <= maxBytes) return s;
            int lo = 0;
            int hi = s.length();
            while (lo < hi) {
                int mid = (lo + hi + 1) / 2;
                byte[] bm = s.substring(0, mid).getBytes("UTF-8");
                if (bm.length <= maxBytes) {
                    lo = mid;
                } else {
                    hi = mid - 1;
                }
            }
            return s.substring(0, lo);
        } catch (Exception e) {
            if (s.length() <= maxBytes) return s;
            return s.substring(0, maxBytes);
        }
    }

    /**
     * Pick a directional Unicode arrow matching the maneuver's BAP direction.
     * Uses the same ManeuverMapper output that drives FctID 23 icons.
     */
    private static String directionArrow(AltCoreRouteState s, int idx) {
        if (idx < 0 || s.mType == null || idx >= s.mType.length) {
            return "";
        }
        int type = s.mType[idx];
        int[] mapped = ManeuverMapper.map(
            type, s.mTurnAngle[idx],
            s.mJunctionType[idx], s.mDrivingSide[idx]);
        int main = mapped[0];
        int dir = mapped[1];

        if (main == ManeuverMapper.NO_SYMBOL || main == ManeuverMapper.NO_INFO) {
            return "";
        }
        if (dir == ManeuverMapper.DIR_LEFT)         return "\u2190"; /* left */
        if (dir == ManeuverMapper.DIR_SLIGHT_LEFT)  return "\u2196"; /* upper-left */
        if (dir == ManeuverMapper.DIR_SHARP_LEFT)   return "\u2199"; /* lower-left */
        if (dir == ManeuverMapper.DIR_RIGHT)        return "\u2192"; /* right */
        if (dir == ManeuverMapper.DIR_SLIGHT_RIGHT) return "\u2197"; /* upper-right */
        if (dir == ManeuverMapper.DIR_SHARP_RIGHT)  return "\u2198"; /* lower-right */
        if (dir == ManeuverMapper.DIR_UTURN)        return "\u21B6"; /* uturn */

        /* Only explicit no-turn/follow-road/straight-ahead source types may
         * produce a straight arrow. Other straight direction codes belong to
         * non-straight main elements (arrival/roundabout/etc.) and must not
         * fabricate an up-arrow text prefix. */
        if (type == ManeuverMapper.MT_NO_TURN
                || type == ManeuverMapper.MT_FOLLOW_ROAD
                || type == ManeuverMapper.MT_STRAIGHT_AHEAD) {
            return "\u2191";
        }
        return "";
    }

    private static String keepLastColonPart(String v) {
        if (v == null) return "";
        int pos = v.lastIndexOf(':');
        if (pos >= 0 && pos + 1 < v.length()) {
            String tail = v.substring(pos + 1).trim();
            if (tail.length() > 0) return tail;
        }
        return v;
    }

    

    /**
     * Resolve one presentation maneuver and keep its descriptor/distance coherent.
     *
     * iOS can leave START_ROUTE at the head of the authoritative list while a real
     * actionable maneuver is already available.  We intentionally skip START_ROUTE
     * for the descriptor.  During that interval the top-level 0x5201 distance can
     * still describe the head/current iOS presentation state rather than the real
     * maneuver we selected.  If the selected 0x5202 slot has a usable distance and
     * the live value is absent or implausibly larger, use the slot distance until
     * iOS advances the authoritative head and the normal live countdown takes over.
     */
    

    

    

    
}
