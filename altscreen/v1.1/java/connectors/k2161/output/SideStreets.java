package com.luka.carplay.routeguidance;
/** Audi BAP side-street wire-encoding adapter. Geometric interpretation
 * is implemented only in firmware-neutral AltCoreSideStreetGeometry. */
final class SideStreets {
    private SideStreets() {}
    static byte[] calcSideStreetsBytes(int maneuverType, int junctionType,
                                      int drivingSide, int[] junctionAngles, int exitAngle) {
        int[] codes = AltCoreSideStreetGeometry.calcSideStreetDirections(
            maneuverType,junctionType,drivingSide,junctionAngles,exitAngle);
        byte[] bytes = new byte[codes.length];
        for (int i=0;i<codes.length;i++) bytes[i]=(byte)(codes[i]&0xff);
        return bytes;
    }
    static int[] calcSideStreetDirections(int maneuverType, int junctionType,
                                         int drivingSide, int[] junctionAngles, int exitAngle) {
        return AltCoreSideStreetGeometry.calcSideStreetDirections(
            maneuverType,junctionType,drivingSide,junctionAngles,exitAngle);
    }
}
