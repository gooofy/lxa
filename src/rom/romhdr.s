
    .global ___restore_a4

.text

	dc.w 	0x1111		/* match word         */
	jmp     _coldstart  /* cold start / reset */

	dc.l	0x0000ffff  /* unknown            */
	dc.w    0x0021      /* version            */
	dc.w    0x00c0      /* revision           */

    .globl _handleTRAP3
_handleTRAP3:
    rte

___restore_a4:
    _geta4:     lea ___a4_init,a4
                rts


