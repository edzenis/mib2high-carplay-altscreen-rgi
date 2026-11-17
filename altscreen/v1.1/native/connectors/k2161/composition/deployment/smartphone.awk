# AltScreen K2161 CarPlay env patch. Only touches carplay.envs. Preserves all
# unrelated preload tokens and other service sections. Fail closed on
# unsupported JSON array syntax; caller installs only on zero exit.
# Supported stock forms: a quoted-string env array on one or multiple lines.
function trim(s) { sub(/^[ \t\r\n]+/,"",s); sub(/[ \t\r\n]+$/,"",s); return s }
function fail(msg) { print "ALTSESSION_SMARTPHONE_UNSUPPORTED=" msg > "/dev/stderr"; bad=1 }
function addvalue(v,    tok,n,j,k,more) {
    if (v ~ /^LD_PRELOAD=/) {
        v=substr(v,length("LD_PRELOAD=")+1)
        n=split(v,tok,/[ \t:]+/)
        for(j=1;j<=n;j++) {
            if(tok[j]=="" || tok[j]==hook) continue
            if(preload!="") preload=preload " "
            preload=preload tok[j]
        }
    } else { values[++nv]=v }
}
function consume(segment,    q,v) {
    segment=trim(segment)
    if (segment=="") return
    # JSON escapes are rejected rather than silently corrupted.
    if (index(segment,"\\") || segment !~ /^"[^"]*"$/) {fail("environment element not plain JSON string");return}
    v=substr(segment,2,length(segment)-2)
    addvalue(v)
}
function arrayitems(str,    count,i,seg) {
    # This deliberately accepts only the actual stock string-array format.
    count=split(str,items,",")
    for(i=1;i<=count;i++) consume(items[i])
}
function emit(    i,ind,v,n) {
    ind=indent "    "
    v="LD_PRELOAD=" (preload=="" ? hook : preload " " hook)
    values[++nv]=v
    print indent "\"envs\": ["
    for(i=1;i<=nv;i++) print ind "\"" values[i] "\"" (i<nv ? "," : "")
    print indent "]" suffix
}
BEGIN {hook="/mnt/app/root/hooks/libmib2high-carplay-rgi.so"; car=0;found=0;env=0;patched=0;bad=0;nv=0;preload=""}
{
    line=$0
    if (!car && line ~ /^[ \t]*"carplay"[ \t]*:/) {car=1;found++}
    if (car && !env && line ~ /^[ \t]*"envs"[ \t]*:[ \t]*\[/) {
        patched++
        indent=line;sub(/[^ \t].*$/,"",indent)
        # Find exactly the array opening bracket and process the remainder.
        rest=line;sub(/^.*"envs"[ \t]*:[ \t]*\[/,"",rest)
        nv=0;preload="";env=1
    } else if (env) {
        rest=line
    } else {
        print line
        next
    }
    if (index(rest,"]")>0) {
        idx=index(rest,"]")
        arrayitems(substr(rest,1,idx-1))
        suffix=trim(substr(rest,idx+1))
        if (suffix!="" && suffix!=",") fail("unsupported trailing tokens after envs array")
        if (!bad) emit()
        env=0;car=0
    } else {
        arrayitems(rest)
        # preserve list across multiline segments
    }
}
END {if (found!=1 || patched!=1 || env || bad) exit 7}
