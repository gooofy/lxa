
.text

	/* The Kickstart header as programs read it (SysInfo; probe exec/machine:
	   AmigaOS 3.1 on the reference has a 512K ROM, Kickstart 40.70). */
	dc.w 	0x1114		/* match word: 512K ROM */
	jmp     _coldstart  /* cold start / reset */

	dc.l	0x0000ffff  /* unknown            */
	dc.w    40          /* Kickstart version  */
	dc.w    70          /* Kickstart revision */

/*
    .global ___restore_a4
___restore_a4:
    _geta4:     lea ___a4_init,a4
                rts
*/

