
.text

	dc.w 	0x1111		/* match word         */
	jmp     _coldstart  /* cold start / reset */

	dc.l	0x0000ffff  /* unknown            */
	dc.w    0x0021      /* version            */
	dc.w    0x00c0      /* revision           */

    .globl _handleTRAP3
_handleTRAP3:
    rte

