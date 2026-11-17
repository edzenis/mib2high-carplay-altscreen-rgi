/*
 * AltScreen K2161 diagnostic output. AltScreen: bounded asynchronous file sink.
 * Navigation/HUD threads never do filesystem operations or wait for file IO.
 * Java 1.4 compatible; no input/core/output reverse references.
 */
package com.luka.carplay.framework;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

public final class Log {
    public static final int LEVEL_DEBUG=0, LEVEL_INFO=1, LEVEL_WARN=2;
    public static final int LEVEL_ERROR=3, LEVEL_NONE=4;
    private static volatile String logPath="/tmp/mib2high-carplay-rgi.log";
    private static volatile int minLevel=LEVEL_INFO;
    private static volatile boolean enabled=true;
    private static volatile boolean includeTimestamp=true;
    private static final long MAX_BYTES=512L*1024L;
    private static final int MAX_BACKUPS=3;
    private static final int CAPACITY=4096;
    private static final Object QUEUE_LOCK=new Object();
    private static final String[] queue=new String[CAPACITY];
    private static int head=0;
    private static volatile int size=0;
    private static long dropped=0;
    private static volatile boolean sinkWarned=false;

    static {
        if (new File("/mnt/app/carplay_verbose").exists()) minLevel=LEVEL_DEBUG;
        Thread writer=new Thread(new Runnable() {
            public void run() { writerLoop(); }
        },"AltScreen-Log-Sink");
        writer.setDaemon(true);
        writer.start();
    }
    private Log() {}
    public static void setPath(String p) { logPath=p; }
    public static void setLevel(int v) { minLevel=v; }
    public static void setEnabled(boolean v) { enabled=v; }
    public static void setIncludeTimestamp(boolean v) { includeTimestamp=v; }
    public static void d(String tag,String msg) { write(LEVEL_DEBUG,tag,msg); }
    public static void i(String tag,String msg) { write(LEVEL_INFO,tag,msg); }
    public static void w(String tag,String msg) { write(LEVEL_WARN,tag,msg); }
    public static void e(String tag,String msg) { write(LEVEL_ERROR,tag,msg); }
    public static void e(String tag,String msg,Throwable t) {
        write(LEVEL_ERROR,tag,msg+": "+t.getClass().getName()+": "+t.getMessage());
    }
    public static void write(int level,String tag,String msg) {
        if (!enabled || level<minLevel || logPath==null) return;
        StringBuffer sb=new StringBuffer(128);
        if(includeTimestamp)sb.append(System.currentTimeMillis()).append(' ');
        sb.append('[').append(levelStr(level)).append("] ");
        if(tag!=null)sb.append('[').append(tag).append("] ");
        sb.append(msg);
        // This lock contains only an in-memory O(1) ring insertion.
        synchronized(QUEUE_LOCK) {
            if(size==CAPACITY) {
                // Preserve high-severity diagnostics under overload. The
                // oldest queued record is sacrificed, not the new warning.
                if(level<LEVEL_WARN) { dropped++; return; }
                queue[head]=null;head=(head+1)%CAPACITY;size--;dropped++;
            }
            queue[(head+size)%CAPACITY]=sb.toString();
            size++;
            QUEUE_LOCK.notifyAll();
        }
    }
    private static void writerLoop() {
        BufferedWriter out=null;
        String openedPath=null;
        long bytes=0;
        long lastFlush=0;
        for(;;) {
            String line=null;
            long missed=0;
            synchronized(QUEUE_LOCK) {
                if(size==0) {
                    try { QUEUE_LOCK.wait(150L); } catch(InterruptedException ignored) {}
                }
                if(size!=0) {
                    line=queue[head];queue[head]=null;
                    head=(head+1)%CAPACITY;size--;
                }
                if(dropped!=0) { missed=dropped; dropped=0; }
            }
            try {
                String target=logPath;
                if(target==null) {
                    if(out!=null){out.flush();out.close();out=null;openedPath=null;}
                    continue;
                }
                if(out==null||!target.equals(openedPath)) {
                    if(out!=null){out.flush();out.close();}
                    File f=new File(target);bytes=f.exists()?f.length():0;
                    out=new BufferedWriter(new FileWriter(target,true),8192);
                    openedPath=target;
                }
                if(line!=null || missed!=0) {
                    if(missed!=0) {
                        String note=System.currentTimeMillis()+" [WRN] [Log] ALTHUD_LOG_DROPPED="+missed;
                        out.write(note);out.newLine();bytes+=note.length()+1;
                    }
                    if(line!=null) {
                        out.write(line);out.newLine();bytes+=line.length()+1;
                    }
                }
                long now=System.currentTimeMillis();
                if(size==0 || now-lastFlush>=200L || bytes>=MAX_BYTES) {
                    out.flush();lastFlush=now;
                }
                if(bytes>=MAX_BYTES) {
                    out.close();out=null;
                    rotate(openedPath);
                    openedPath=null;bytes=0;
                }
            } catch(Throwable t) {
                if(!sinkWarned) { sinkWarned=true; System.err.println("ALTHUD_LOG_SINK_FAILED "+t.getClass().getName()); }
                if(out!=null)try {out.close();}catch(Throwable ignored){}
                out=null;openedPath=null;
            }
        }
    }
    private static void rotate(String path) throws IOException {
        File oldest=new File(path+"."+MAX_BACKUPS);
        if(oldest.exists()&&!oldest.delete())throw new IOException("log prune failed");
        for(int i=MAX_BACKUPS-1;i>=1;i--){
            File f=new File(path+"."+i);
            if(f.exists()&&!f.renameTo(new File(path+"."+(i+1))))throw new IOException("log rotate failed");
        }
        File current=new File(path);
        if(current.exists()&&!current.renameTo(new File(path+".1")))throw new IOException("log rotate rename failed");
    }
    public static void hexdump(String tag,String prefix,byte[] data,int offset,int len,int maxBytes) {
        if(!enabled||LEVEL_DEBUG<minLevel||data==null||len<=0)return;
        int n=(maxBytes>0 && len>maxBytes)?maxBytes:len;
        StringBuffer sb=new StringBuffer();if(prefix!=null)sb.append(prefix).append(' ');
        sb.append("len=").append(len).append(" bytes=");
        for(int i=0;i<n;i++){
            int b=data[offset+i]&255;
            sb.append(hexChar(b>>4));sb.append(hexChar(b&15)).append(' ');
        }
        if(n<len)sb.append("...");
        write(LEVEL_DEBUG,tag,sb.toString());
    }
    private static String levelStr(int x){switch(x){case 0:return "DBG";case 1:return "INF";case 2:return "WRN";case 3:return "ERR";default:return "???";}}
    private static char hexChar(int n){return n<10?(char)('0'+n):(char)('A'+n-10);}
}
