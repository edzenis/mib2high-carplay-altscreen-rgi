package com.luka.carplay.routeguidance;

/** Text-only projection of navigation estimates. No UI and no firmware APIs.
 * Caller must supply an already vehicle-local ETA epoch in milliseconds;
 * -1 suppresses ETA rather than displaying an unverified UTC clock.
 * Missing values are omitted, never silently replaced with zero.
 */
public final class AltCoreTripSummary {
    private AltCoreTripSummary() {}
    public static String format(AltCoreRouteState s, long vehicleLocalEtaMs, boolean metric) {
        if (s == null) return "";
        StringBuffer out = new StringBuffer(48);
        if (vehicleLocalEtaMs >= 0) {
            long min = vehicleLocalEtaMs / 60000L;
            min %= 1440L;
            if (min < 0) min += 1440L;
            int h = (int)(min / 60L), m = (int)(min % 60L);
            out.append("ETA ");
            if (h < 10) out.append('0');
            out.append(h).append(':');
            if (m < 10) out.append('0');
            out.append(m);
        }
        if (s.timeRemainingSeconds >= 0) {
            if (out.length() != 0) out.append(" |");
            long mins = (s.timeRemainingSeconds + 59L) / 60L;
            if (mins >= 60L) {
                out.append(mins / 60L).append("h");
                long rem = mins % 60L;
                if (rem != 0) out.append(rem).append("m");
            } else {
                out.append(mins).append("m");
            }
        }
        if (s.distDestM >= 0) {
            if (out.length() != 0) out.append(" |");
            int meters = s.distDestM;
            if (metric) {
                if (meters < 1000) out.append(meters).append("m");
                else if (meters < 10000) {
                    int deciKm = (meters + 50) / 100;
                    out.append(deciKm / 10).append('.').append(deciKm % 10).append("km");
                } else out.append((meters + 500) / 1000).append("km");
            } else {
                int tenthsMiles = (int)((((long)meters * 10L) + 804L) / 1609L);
                out.append(tenthsMiles / 10).append('.').append(tenthsMiles % 10).append("mi");
            }
        }
        return out.toString();
    }
}
