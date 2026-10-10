        section code,code
start:  moveq   #0,d0
        rts
tag:    dc.w    $4afc
        dc.l    tag
        dc.l    endtag
        dc.b    0,1,9,0
        dc.l    name
        dc.l    name
        dc.l    init
endtag:
name:   dc.b    "probeinit.library",0
        even
init:   movem.l d2-d7/a2-a6,-(sp)
        move.l  4.w,a6
        lea     vecs(pc),a0
        sub.l   a1,a1
        sub.l   a2,a2
        moveq   #34,d0
        moveq   #0,d1
        jsr     -84(a6)
        tst.l   d0
        beq.s   done
        move.l  d0,a1
        lea     name(pc),a0
        move.l  a0,10(a1)
        move.b  #9,8(a1)
        move.w  #1,20(a1)
        jsr     -396(a6)
        moveq   #1,d0
done:   movem.l (sp)+,d2-d7/a2-a6
        rts
open:   addq.w  #1,32(a6)
        move.l  a6,d0
        rts
close:  subq.w  #1,32(a6)
        moveq   #0,d0
        rts
null:   moveq   #0,d0
        rts
vecs:   dc.w    -1
        dc.w    open-vecs
        dc.w    close-vecs
        dc.w    null-vecs
        dc.w    null-vecs
        dc.w    -1
