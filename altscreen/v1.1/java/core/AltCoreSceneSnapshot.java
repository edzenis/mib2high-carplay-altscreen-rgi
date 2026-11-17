package com.luka.carplay.routeguidance;
/** Firmware-independent maneuver scene, normalized from the universal route
 * state exactly once before outputs are notified. No K2161, BAP or GL API.
 * Immutable; K2161 output only serializes it to the renderer. */
public final class AltCoreSceneSnapshot extends AltCoreManeuverTypes {
    public static final int NONE=0, APPROACH=1, TURN=2, UTURN=3, MERGE=4;
    public static final int EXIT=5, ROUNDABOUT=6, ARRIVED=7, LANE_CHANGE=8;
    public static final int ROUNDABOUT_EXIT=9;
    public final boolean valid;
    public final int icon,exitAngle,direction,drivingSide,distanceM;
    private final int[] junctionAngles;
    private AltCoreSceneSnapshot(boolean ok,int icon,int angle,int dir,int drive,
                                 int distance,int[] junctions) {
        valid=ok;this.icon=icon;exitAngle=angle;direction=dir;
        drivingSide=drive;distanceM=distance;
        junctionAngles=junctions==null?new int[0]:(int[])junctions.clone();
    }
    public int[] junctionAngles() {return (int[])junctionAngles.clone();}
    public static AltCoreSceneSnapshot empty() {
        return new AltCoreSceneSnapshot(false,NONE,-999,0,0,-1,null);
    }
    public static AltCoreSceneSnapshot from(AltCoreRouteState s,boolean routeActive) {
        if(!routeActive||s==null||s.mType==null)return empty();
        AltCoreManeuverPolicy.Snapshot p=new AltCoreManeuverPolicy.Snapshot(
            s.mType,s.mDistance,s.maneuverOrder,s.distManeuverM,
            MT_NO_TURN,MT_CHANGE_HIGHWAY_RIGHT,MT_START_ROUTE);
        AltCoreManeuverPolicy.PresentationManeuver head=
            AltCoreManeuverPolicy.resolvePresentationManeuver(p);
        int idx=head.index;
        if(idx<0||idx>=s.mType.length)return empty();
        int type=s.mType[idx],icon=NONE,dir=0;
        int drive=validAt(s.mDrivingSide,idx,0);
        if(drive!=0&&drive!=1)drive=0;
        int exit=validAt(s.mExitAngle,idx,1000);
        int turn=validAt(s.mTurnAngle,idx,-1);
        if(exit==1000)exit=(turn>=-180&&turn<=180&&turn!=-1)?turn:-999;
        switch(type){
        case MT_NO_TURN:case MT_STRAIGHT_AHEAD:case MT_FOLLOW_ROAD:
        case MT_START_ROUTE: icon=TURN;exit=0;break;
        case MT_LEFT_TURN:case MT_LEFT_TURN_AT_END:icon=TURN;dir=-1;break;
        case MT_RIGHT_TURN:case MT_RIGHT_TURN_AT_END:icon=TURN;dir=1;break;
        case MT_SLIGHT_LEFT_TURN:case MT_KEEP_LEFT:icon=TURN;dir=-1;
            if(exit==-999)exit=-35;break;
        case MT_SLIGHT_RIGHT_TURN:case MT_KEEP_RIGHT:icon=TURN;dir=1;
            if(exit==-999)exit=35;break;
        case MT_SHARP_LEFT_TURN:icon=TURN;dir=-1;if(exit==-999)exit=-135;break;
        case MT_SHARP_RIGHT_TURN:icon=TURN;dir=1;if(exit==-999)exit=135;break;
        case MT_U_TURN:case MT_START_ROUTE_WITH_U_TURN:
        case MT_U_TURN_WHEN_POSSIBLE:icon=UTURN;break;
        case MT_ENTER_ROUNDABOUT:case MT_U_TURN_AT_ROUNDABOUT:
            icon=ROUNDABOUT;break;
        case MT_EXIT_ROUNDABOUT:icon=ROUNDABOUT_EXIT;break;
        case MT_OFF_RAMP:case MT_ON_RAMP:icon=EXIT;break;
        case MT_HIGHWAY_OFF_RAMP_LEFT:icon=EXIT;dir=-1;break;
        case MT_HIGHWAY_OFF_RAMP_RIGHT:icon=EXIT;dir=1;break;
        case MT_CHANGE_HIGHWAY:icon=MERGE;break;
        case MT_CHANGE_HIGHWAY_LEFT:icon=LANE_CHANGE;dir=-1;break;
        case MT_CHANGE_HIGHWAY_RIGHT:icon=LANE_CHANGE;dir=1;break;
        case MT_ARRIVE_END_OF_NAVIGATION:case MT_ARRIVE_AT_DESTINATION:
        case MT_ARRIVE_END_OF_DIRECTIONS:icon=ARRIVED;break;
        case MT_ARRIVE_DESTINATION_LEFT:icon=ARRIVED;dir=-1;break;
        case MT_ARRIVE_DESTINATION_RIGHT:icon=ARRIVED;dir=1;break;
        default:
            if(type>=MT_ROUNDABOUT_EXIT_1&&type<=MT_ROUNDABOUT_EXIT_19)
                icon=ROUNDABOUT_EXIT;
            break;
        }
        if(icon==NONE)return empty();
        if(icon==TURN&&exit==-999)exit=dir*90;
        if(icon==EXIT&&exit==-999&&dir!=0)exit=dir*30;
        /* Never manufacture an arbitrary exit where neither direction nor
         * iAP2 exit geometry provides one; omit the graphic rather than lie. */
        /* The roundabout departure shape uses driving side and does not
         * require an exit angle. Only the full entry/ring scene does. */
        if(icon==ROUNDABOUT&&exit==-999)return empty();
        if(icon==EXIT&&exit==-999)return empty();
        if(exit!=-999&&(exit<-180||exit>180))return empty();
        int[] junctions=(s.mJunctionAngles!=null&&idx<s.mJunctionAngles.length)
            ?s.mJunctionAngles[idx]:null;
        if(junctions!=null&&junctions.length>18){
            int[] truncated=new int[18];System.arraycopy(junctions,0,truncated,0,18);
            junctions=truncated;
        }
        if(junctions!=null)for(int i=0;i<junctions.length;i++)
            if(junctions[i]<-180||junctions[i]>180)return empty();
        return new AltCoreSceneSnapshot(true,icon,exit,dir,drive,
            head.distanceM,junctions);
    }
    private static int validAt(int[] arr,int i,int fallback) {
        return arr!=null&&i>=0&&i<arr.length?arr[i]:fallback;
    }
}
