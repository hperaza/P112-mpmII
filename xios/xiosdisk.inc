;=======================================================================;
;	B A S I C   D I S K - D R I V E   R O U T I N E S		;
;									;
;  Non-interrupt driven support routines for the SMC diskette chip.	;
;=======================================================================;
;   DISKOPQ - Version for BIOS, with FORMAT disabled to save space.
;
;   This code is designed to work without interrupts (compatible with
; the ROM bootstrap code). Consequently, timing functions are done by 
; polling and/or delay loops.
;   The DMA is used for data transfer, and is polled for completion (check
; that the byte counter has decremented to zero).
;
; Call:
;	A  - OPCODE:	0 - Reset disk system: all drives uncalibrated
;			1 - Turn motors off (leave drives calibrated)
;			2 - Read sector/s
;			3 - Write sector/s
;			4 - Verify sector/s
;			5 - Format track
;			6 - Translate logical address to physical
;	B  - No. of sectors to read/write
;	C  - Track No. (even tracks are Side 0, odd tracks are Side 1)
;	D  - Drive No. (0..3)
;	E  - Sector No.
;	HL - Ptr. to data buffer (logical address)
;	IX - Ptr. to Drive Parameter Block (DPB)
;
; Return:
;	If successful:
;		CY = 0
;		A    undefined
;		HL   advanced past data block
;
;	If errors:
;		CY = 1
;		A    status (see below)
;		HL   unchanged
; 
;	In either case:
;		DE -> Result vector (7 bytes)
;		IX, IY unchanged
;		BC     trashed

;INCFORMAT	equ	0	; Define this to include the FORMAT cmnd.

;======================================================================
;	L O C A L   D A T A
 
; Return-error status values (in A, if CY=1)

CALERR	equ	1	; Error in a recalibration function
IOERR	equ	2	; Error posted by Result Phase: see (HL)
BADSECT	equ	3	; Sector/Length will overrun track
LOSTDAT	equ	4	; DMA did not transfer full block
TIMEOUT	equ	5	; Interrupt timed out
BADCMD	equ	6	; Invalid function code
SEKERR	equ	7	; Seek error

TRIES	equ	5	; No. of retries on IO errors

;======================================================================
;	F . D . C .    C O M M A N D   C O D E S

CMD_RECAL	equ	7	; Recalibrate
CMD_SENSE	equ	8	; Sense Interrupt
CMD_SEEK	equ	0FH	; Seek to track
CMD_READ	equ	46H	; Read (MFM, implied seek)
CMD_WRITE	equ	45H	; Write, ditto
CMD_VERIFY	equ	56H	; Verify, ditto
CMD_FORMAT	equ	4DH	; Format a track
CMD_RDID	equ	4AH	; Read Disk Sector ID Data

;	D . M . A .   C O M M A N D S

DMA_INCMD	equ	0CDH	; IO  => MEM
DMA_OUTCMD	equ	0F1H	; MEM => IO
DMA_RUN		equ	63H	; Kick off Channel-0

;======================================================================
;	U T I L I T Y   S U B R O U T I N E S

; Poll "Interrupt" from FDC controller, return 0FFh if ready, 00h if not.
; TODO: implement a timeout flag!

poldsk:
	in0	a,(DRC)		; Get interrupt lines
	and	40h		; BIT 6,A ! JR Z,exit
	xor	40h
	ret	z
	ld	a,0FFh
	ret

;--------------------------------
; Check results of I/O operations
;  IY -> test-block for status:  (see "launch" for details)

TestResult:
	push	bc
	push	de
	push	iy
	ld	c,poll
	ld	e,pldsk
	call	xdos		; Wait for completion
	pop	iy
	pop	de
	pop	bc
;	ld	a,TIMEOUT
;	jr	c,Exit		; Failed (CY posted to caller)

; Get 7 result bytes after read/write

	push	bc
	push	de
	ld	de,resTab	; Point to Result Table
	ld	b,7		; No. of expected bytes
Lc3:	call	GetByte
	ld	(de),a
	inc	de		; Post it
	djnz	Lc3
	pop	de
	pop	bc
	ld	hl,resTab	; Pointer to results block
	push	hl

	ld	a,(hl)		; Get ST0
	and	0F8h		; Kill variable bits (just errors)
	cp	(iy+TST0)	; Check to expected value
	jr	nz,Bad
	inc	hl
	ld	a,(hl)		; Get ST1
	and	0FDh		;;  (Kill W/P Bit se we can read W/P Disks)
	cp	(iy+TST1)	; Check to expected value
	jr	nz,Bad
	inc	hl
	ld	a,(hl)
	sub	0		; No "funny" errors (ever!)
	jr	nz,Bad

;; Don't check the count to avoid problems with commands that don't use DMA
;; (ReadID)

	ld	a,CMD_RDID
	cp	(iy+0)
	jr	z,skip1
	in0	l,(BCR0L)
	in0	a,(BCR0H)
	or	l		; Check that all was transferred
	ld	a,LOSTDAT
	jr	nz,Bad2		; Should have counted back to zero

skip1:	pop	hl
	xor	a		; It all succeeded!
	jr	Exit		; OK: return CY=0 (A=0)

;;Bad1:	ld	a,BADSECT	; Bad sector specification
;;	scf
;;	jr	Exit

Bad:	ld	a,IOERR		; Error post: see block at (HL)
Bad2:	pop	hl
	scf
Exit:	ret			; CY = error

;--------------------------------
; Send A to Cmnd/data port

SndByt:	push	af
Scb1:	in0	a,(_MSR)	; Check FDC status
	bit	7,a		; Wait till ready
	jr	z,Scb1
	pop	af		; Recover the cmnd. byte
	out0	(_DATA),a
	ret

;--------------------------------
; Read Cmnd/data port to A

GetByte:
	in0	a,(_MSR)
	bit	7,a		; Ready?
	jr	z,GetByte
	in0	a,(_DATA)	; Get it
	ret

;--------------------------------
; Send string of commands from HL

SndCmStr:
	push	bc		; 1st. byte is count
	ld	B,(hl)		; Save count
SndC1:	inc	hl
	ld	a,(hl)		; Get cmnd. byte
	call	SndByt		; Send it
	djnz	SndC1		; Repeat
	pop	bc
	ret

;--------------------------------
; Force a recalibrate on this drive

Uncal:	push	hl
	ld	hl,calibd	;First force a recalibrate
	ld	a,(MASK)
	cpl
	and	(hl)		;Turn of the "calibrated" bit
	ld	(hl),a
	pop	hl
	ret

;--------------------------------
; Set up DMA buffers for input

DMAInp:	call	absaddr		;Get physical addr. for DMA
	out0	(dar0l),l
	out0	(dar0h),h
	out0	(dar0b),a	;Destination pointers
	ld	a,_DMA
	out0	(sar0l),a	;Source: FDC's DMA port
	xor	a
	out0	(sar0h),a
	out0	(sar0b),a	;Controlled by /DREQ0
	ret
 
;--------------------------------
; Set up DMA buffers for output

DMAOut:	call	absaddr		;Get physical addr. for DMA
	out0	(sar0l),l
	out0	(sar0h),h
	out0	(sar0b),a	;Source pointers
	ld	a,_DMA
	out0	(dar0l),a	;Destn.: FDC's DMA port
	xor	a
	out0	(dar0h),a
	out0	(dar0b),a	;Controlled by /DREQ0
	ret
 
;--------------------------------
; Offsets for inline arguments

OPCODE	equ	0		; Command to FDC
TST0	equ	1		; Match Status 0
TST1	equ	2		;   "	  "   1
SKIP	equ	3		; No. of bytes to skip at return

; Initiate a read or write command

Launch:	ex	(sp),iy		; Save IY, point at argts.
	ld	a,e		; Find final sector for transfer
	add	a,b
	dec	a		; Last sector to do
;; Delete following to permit offset Sector Numbers
;;	dec	a		; 1 under final
;;	cp	(ix+EOT)	; This MUST be < true final
;;	jr	nc,Bad1
;;	inc	a		; True final sector
	ld	h,a		; Save it

	ld	a,(iy+OPCODE)
	call	SndByt		; Command code
	ld	a,d		; Drive
	bit	0,c		; Odd track (ie Side 1)?
	jr	z,Lc1
	set	2,a
Lc1:	call	SndByt		; Drive & head
	ld	a,c
	srl	a		; Cylinder
	call	SndByt
	ld	a,c
	and	1
	call	SndByt		; Head
	ld	a,e
	call	SndByt		; Sector
	ld	a,(ix+SECSZE)
	call	SndByt		; Sector length (encoded)
	ld	a,h
	call	SndByt		; Final sector no.
	ld	a,(ix+GPL)
	call	SndByt		; Gap length

	ld	a,DMA_RUN	; Activate DMA Ch-0
	out0	(DSTAT),a

	ld	a,0FFh		; Data transfer length (default)
	call	SndByt		; Final byte: FDC & DMA running

	call	TestResult	; Find out what happened
	push	bc
	push	af
	ld	bc,SKIP
	add	iy,bc		; Jump past the inline params.
	pop	af
	pop	bc
	ex	(sp),iy		; Recover IY, post link
	ret

;--------------------------------
; Determine buffer size (DMA count set)

TranSze:
	push	hl
	push	de
	push	bc
	ld	hl,64
	ld	b,(ix+SECSZE)	;Size N, => 128*2^N
	inc	b
tz1:	add	hl,hl
	djnz	tz1
	ld	d,h
	ld	e,l		;Sector size in DE
	ld	hl,0
	pop	bc		;No. of sectors in B
	push	bc
tz2:	add	hl,de		;Form total in HL
	djnz	tz2
	out0	(bcr0l),l	;Set DMA counter
	out0	(bcr0h),h
	ld	(blksze),hl	;Save block size
 
	pop	bc
	pop	de		;Restore regs.
	pop	hl
	ret
 
;--------------------------------
; Finish of a seek/recalibrate function

EndSeek:
	push	ix
	push	iy
	push	hl
	push	de
	push	bc
	ld	c,poll
	ld	e,pldsk
	call	xdos		;Wait for it
	pop	bc
	pop	de
	pop	hl
	pop	iy
	pop	ix
;	ld	a,timeout
;	ret	c		;Timed out
 
	ld	a,cmd_sense	;"Sense Interrupt" at the end
	call	SndByt
	call	getbyte
	ld	b,a		;ST0 in B
	call	getbyte		;PCN in A
 
	push	bc		;Verify correct cylinder
	srl	c
	cp	c		;Yes?
	pop	bc
	ld	a,sekerr
	scf
	ret	nz		;Quit if unequal
 
	ld	a,b		;Get back ST0
	and	0E0h
	cp	20h		;Look for success
	ret	z
	ld	a,calerr
	scf
	ret			;CY=1 if it failed
 
;--------------------------------
DORON	equ	0Ch		; Basic state: no drives, no reset, DMA

; Prepare for a Read or Write operation

FSetup:
	push	bc		; Save them...
	push	hl

	ld	b,d		; Drive no.
	inc	b
	ld	a,08H
Su1:	add	a,a
	djnz	Su1		; Shift drive bit as reqd.
	ld	(mask),a	; Save drive indicator bit
	ld	b,a		; Local copy

	xor	a
	ld	(motorcnt),a	; prevent timerint routine from messing up

	in0	a,(_DOR)
	ld	c,a
	and	0FCh		; Kill select bits
	or	d		; Select reqd. drive
	or	DORON		; No reset, DMA
	out0	(_DOR),a	; Selected

	ld	a,c
	and	b		; Is this drive running?
	jr	nz,Su2
	in0	a,(_DOR)
	or	b		; Start it up
	out0	(_DOR),a

; Wait for motor to run-up.

	IF	0	; for some reason this doesn't work with banked BDOS
;	push	bc
	push	de
	push	hl
	push	ix
	push	iy
	ld	c,xdelay
	ld	e,(ix+MTRON)	; Load Motor On delay constant
	ld	d,0
	call	xdos
	pop	iy
	pop	ix
	pop	hl
	pop	de
;	pop	bc
	ELSE
	ld	bc,0		; Prime for inner loop
	in0	a,(CCR)
	rla			; Hi-Speed CPU?
	ld	a,(ix+MTRON)	; Load Basic Delay Constant
	jr	c,Mo1		; ..jump if Low-Speed
	add	a,a		; Else Scale for Inner Loop (double count)
Mo1:	djnz	Mo1
	dec	c
	jr	nz,Mo1
	dec	a
	jr	nz,Mo1
	ENDIF

Su2:
	ld	a,100		; 5 secs
	ld	(motorcnt),a

; Send the SPECIFY command (all from DPB)

	ld	a,(ix+DRATE)
	out0	(_CCR),a	; Data rate as reqd.

	ld	a,03h		; "Specify"
	call	SndByt

	ld	a,(NVRate)
	bit	7,a		; Valid Non-Volatile Configuration?
	jr	nz,Su2A		; ..jump if Not
	inc	d
	dec	d		; Drive A: (Unit 0)?
	ld	a,(flpy0T)	;  (prepare for Yes)
	jr	z,Su20		; ..jump if So
	ld	a,(flpy1T)	; Else get Timings for Drive B: (Unit 1)
Su20:	push	af		;  (save)
	and	0Fh		;  keep only SRT
	neg			;   make 2's complement
	rla			; Move
	rla			;  to
	rla			;   High
	rla			;    Nibble
	or	0Fh		;     and add Maximum HUT
	call	SndByt
	pop	af		; Restore timings byte for HLT
	rra			; Move HLT
	rra			;  down
	rra			;   to count * 4
	and	1Eh		;    kill possible junk, insure DMA bit=0
	jr	Su2B		;   ..and jump to Set

Su2A:	ld	a,(ix+SPEC_1)
	call	SndByt
	ld	a,(ix+SPEC_2)
	res	0,a		; Be sure DMA is activated
Su2B:	call	SndByt

	ld	hl,calibd
	ld	a,(mask)
	and	(hl)		; Is this drive calibrated?
	jr	nz,Su3		; Done if so

	ld	a,CMD_RECAL
	call	SndByt		; Send the "recal" command
	ld	a,d
	call	SndByt		; Drive no.

	ld	bc,0		;;;
	call	EndSeek		; Finish the operation
	jr	c,Su5		; Jump if error
	ld	a,(mask)
	or	(hl)
	ld	(hl),a		; Flag it calibrated, now
Su3:	xor	a		; Success...

Su5:	and	a		; Set the Z bit
	pop	hl
	pop	bc
	ret			; Done: Z set if OK

;======================================================================
;	C O M M A N D   D I S P A T C H E R
 
DiskOp:	cp	MAXCMD
	jr	c,Do1
	ld	a,BADCMD
	scf
	ret			; Illegal command

Do1:	call	Entry		; Enter the slave routine
	push	af		; Save exit status
	call	c,Reset		; If errors, reset the FDC
	pop	af
	ret	c		; If error, take exit code
	xor	a		; If OK, return zero
	ret			; Back to caller

Entry:	push	hl		; Save it
	ld	hl,dTab
	add	a,a		; Scale opcode by 2
	add	a,l
	ld	l,a		; Access the jumptable
	jr	nc,Do2
	inc	h		; Propagate carry
Do2:	ld	a,(hl)		; Look into the table
	inc	hl
	ld	h,(hl)
	ld	l,a

	ex	(sp),hl		; Recover HL: routine ptr. on stack
	ret			; Switch to routine

dTab:	dw	Reset		; Pointers to function routines
	dw	Motors
	dw	XRead		; I/O functions: call wrappers
	dw	XWrite
	dw	XVerf
	dw	XForm
	dw	AbsAddr

MAXCMD	equ	($-dTab)/2	; No. of valid commands

;======================================================================
;	I / O   C O M M A N D   W R A P P E R   F U N C T I O N
 
;  The READ, WRITE, VERIFY and FORMAT commands are essentially identical.
; The dispatcher calls a stub, which sets up a common wrapper to
; run the required sub-function, and to implement re-tries in the
; event of data errors.
 
; Wrapper stubs. The common tail is called with the user's HL stacked,
; and HL pointing to the required sub-function.

JPOPC	equ	0C3h		; A "JP" opcode

XRead:	push	hl
	ld	hl,XReadS	; Sub-function pointer
	jr	ComTail

XWrite:	push	hl
	ld	hl,XWritS
	jr	ComTail

XVerf:	push	hl
	ld	hl,Verify
	jr	ComTail

XForm:	push	hl
	ld	hl,Format

; Common tail of the "wrapper" function

ComTail:
	ld	(Switch+1),hl	; Target for the dispatcher
	pop	hl		; Caller's value
	push	af
	ld	a,JPOPC
	ld	(Switch),a	; Complete the "JP" instruction

	push	hl
	ld	hl,0
	ld	(blkSze),hl	; Default value
	pop	hl

	ld	a,TRIES
Again:	ld	(rtCnt),a	; Retry counter
	pop	af

	push	hl
	call	Switch		; To the sub-function
	pop	hl		; Save HL (no others changed)

	jr	nc,Ok		; Exit: no error
	cp	IOERR		; Was it an IO error?
	jr	z,More
	cp	SEKERR		; Or a seek error
	jr	nz,Fatal	; No: other errors are fatal

More:	push	af		; Seek or IO errors: retry
	call	Reset		;;;-----;;;
	call	Uncal		; Reset the "calibrated" bit
	ld	a,(rtCnt)
	dec	a		; Count attempts
	jr	nz,Again	; More to go

	pop	af		; Exhausted: get the error code
Fatal:
	call	perror		; display the error cause
	scf			; Hard errors
	ld	de,resTab	; Pass the result pointer
	ret

Ok:	ld	de,(blkSze)
	add	hl,de		; Success: index HL past block
	ld	de,resTab	; Return DE -> status block
	ret

;-------------------------------
; Common stub for sub-functions to exit with error

Fails:	scf			; Return Carry for Error
	ret

;======================================================================
;	R E S E T   T H E   D I S K E T T E   S Y S T E M

Reset:	push	hl
	xor	a
	ld	(calibd),a	; All drives uncalibrated
	out0	(_DOR),a	; Force reset
	ld	a,DORON
	out0	(_DOR),a	; All drives off (DMA asserted)
	ld	hl,cnfgTab
	call	SndCmStr	; "Configure"
	xor	a		; Post OK
	pop	hl
	ret

cnfgTab:
	db	ECFT-$-1	; Length
	db	13H		; "Configure"
	db	00H		; Required
	db	71H		; No FIFO in use
	db	43		; Write precomp point
ECFT	equ	$
 
;======================================================================
;	T U R N   O F F   A L L   D R I V E - M O T O R S

;  This command is really only needed until interrups are supported,
; since it is then possible to automatically turn off a drive which has
; not been accessed for some time.
;  This command is used by the elementary BIOS keyboard routine: the
; drives are turned off if the system has waited for keyboard input
; for too long - a simple method!

Motors:	in0	a,(_DOR)
	and	0Fh		; Drop motor lines: keep selection
	out0	(_DOR),a
	xor	a		; "Success"
	ret

;======================================================================
;	R E A D   O N E   O R   M O R E   S E C T O R S

XReadS:	call	FSetup		; Drive selected & ready
	jr	nz,Fails

	call	DMAInp		; Set up DMA pointers for input
	call	TranSze		; Get total buffer size

	ld	a,DMA_INCMD
	out0	(DMODE),a	; DMA set up: IO => MEM

	call	Launch		; Go fire it off
	db	CMD_READ		; Command code
	db	40H			; Expected ST0
	db	80H			; Expected ST1
	ret			; CY=fault: HL -> result phase

;======================================================================
;	W R I T E   O N E   O R   M O R E   S E C T O R S

XWritS:	call	FSetup		; Drive selected & ready
	jr	nz,Fails

	call	DMAOut		; Set up DMA pointers for output
	call	TranSze		; Get total buffer size

	ld	a,DMA_OUTCMD
	out0	(DMODE),a	; DMA set up: MEM => IO

	call	Launch		; Go fire it off
	db	CMD_WRITE		; Command code
	db	40H			; Expected ST0
	db	80H			; Expected ST1
	ret			; CY=fault: HL -> result phase

;======================================================================
;	V E R I F Y   S E C T O R S  -  N O   T R A N S F E R

Verify:	call	FSetup		; Drive selected & ready
	jr	nz,Fails

	call	Launch		; Go fire it off
	db	CMD_VERIFY		; Command code
	db	0			; Expected ST0
	db	0			; Expected ST1
	ret			; CY=fault: HL -> result phase

;======================================================================
;	F O R M A T   A   T R A C K   O F   T H E   D I S K E T T E

Format:
	IFDEF	INCFORMAT	; Do we include FORMAT ?
	call	FSetup		; Drive running, & calibrated
	jp	nz,fails
 
	ld	a,cmd_seek	; Seek to the required track
	call	SndByt		; We cannot use "implied seeks" here
	ld	a,d
	bit	0,c
	jr	z,fm1
	set	2,a		; A = drive & head bits
fm1:	call	SndByt
	ld	a,c
	srl	a		; Cylinder no.
	call	SndByt
 
	call	endseek		; Wait for seek to complete
	ret	c		; Failed
 
	call	DMAOut		; DMA pointers for output
 
	push	hl
	ld	l,(ix+EOT)	; No. of sectors
	ld	h,0
	add	hl,hl
	add	hl,hl		; 4 bytes/sector
	out0	(bcr0l),l
	out0	(bcr0h),h	; Set up DMA counter
	ld	(blksze),hl
	pop	hl
 
	ld	a,dma_outcmd
	out0	(dmode),a	; DMA armed to output
 
	ld	a,cmd_format	; Now initiate the FDC
	call	SndByt
	ld	a,d
	bit	0,c
	jr	z,fm2
	set	2,a		; Side select
fm2:	call	SndByt		; Drive & side
	ld	a,(ix+SECSZE)
	call	SndByt		; Sector size
	ld	a,(ix+EOT)
	call	SndByt		; Final sector no.
	ld	a,(ix+GAPFMT)
	call	SndByt		; Gap-3 length
 
	ld	a,dma_run
	out0	(dstat),a	; Kick off the DMA
 
	ld	a,(ix+DATFMT)
	call	SndByt		; Last byte: FDC running
 
	push	iy
	ld	iy,fmtres	; Expected results
	call	testresult	; Wait for it...
	pop	iy
	ret			; A and CY show result

fmtres:	db	0		; Filler
	db	0		; ST-0
	db	0		; ST-1

	ELSE
	jr	Fails		; No FORMAT command
	ENDIF

;======================================================================
;	R E A D    D I S K    S E C T O R    I D    D A T A
; Enter: D = Disk Number (A=0, B=1, etc)
;	IX -> Disk Definition Table (Original characteristics table)
; Exit : A = 0, Zero Set (Z), Carry Clear (NC) if ID Read Correctly
;	   Carry Set (C) if Timeout (Reg A and Zero Flag indeterminate)
;	   A <> 0, Zero Reset (NZ) if Error Reading ID (Carry indeterminate)

ReadID:	call	FSetup		; Start Motors, Select, etc...
	jr	nz,Fails	; ..signify Error on return if Bad
	ld	a,CMD_RDID
	call	SndByt		; Command the Controller
	ld	a,d
	call	SndByt		;  with Drive #
	ld	iy,RIDres	;    (check results with this)
	call	TestResult	;   get controller response
	ret	c		;  ..return Carry set if Timeout

; Sort out the data, determining which Disk format if possible

	ld	hl,resTab	; The Command results went here
	ld	de,6
	add	hl,de		;  offset to Sector Size
	ld	a,(hl)		;   fetch
	or	A		;    and insure Carry Clear
	ret

RIDres:	db	4AH		; Command code
	db	00		; Expected ST0
	db	00		; Expected ST1

;======================================================================
; C O N V E R T   L O G I C A L   T O   P H Y S I C A L   A D D R E S S

;  This utility converts a logical address (in HL) to a 20-bit physical
; address (in AHL). Other registers are unchanged. Absolute addresses
; are used by the Z182 DMA.
;  The routine accesses the memory-mapping hardware registers, to obtain
; the current translations.
;	NB This code assumes that CA1 > BA > CA0

absaddr:			; Converts HL to AHL
	push	hl
	ld	a,h		; First, see which logical bank
	and	0F0h
	ld	h,a		; Save 4kB page
	in0	a,(cbar)
	and	0F0h		; CA1 logical base
	ld	l,a
	ld	a,h
	sub	l		; Form Addr - Base
	jr	c,not1		; If CY: Addr < Base, so don't use
	in0	a,(cbr)		; It is CA1 - take base
	jr	mod
 
not1:	in0	a,(cbar)
	add	a,a
	add	a,a
	add	a,a
	add	a,a		; BA logical base in A[7:4]
	ld	l,a
	ld	a,h
	sub	l		; Form Addr - Base
	ld	a,0
	jr	c,mod		; Not BA, must be CA0
	in0	a,(bbr)		; Else, it is BA
mod:
	pop	hl		; Now do the adjustment
	push	bc		; Save it
	push	hl		; Save HL again
	ld	c,h
	ld	b,0		; BC = HL >> 8
	ld	l,a
	ld	h,b		; HL = physical base
	add	hl,hl
	add	hl,hl
	add	hl,hl
	add	hl,hl		; HL = physical base << 4
	add	hl,bc		; Top address in HL
	ld	a,h
	and	0Fh		; Bits 19:16 in A
	ld	b,l		; Save bits 15:8
	pop	hl
	ld	h,b		; Full address in AHL
	pop	bc		; Restore BC
	and	a		; CY=0 (i.e. no errors)
	ret			; Done

;--------------------------------------------------------------------

perror:	push	af
	ld	(errc),a	; save error code
	ld	a,c
	ld	(ptrkn),a	; save track number
	ld	a,e
	ld	(psecn),a	; save sector number
	push	hl
	push	de
	push	bc
	ld	hl,emsg1
	call	putmsg
	ld	a,(sekdsk)
	add	a,'A'
	ld	c,a
	ld	d,0		; console #0
	call	conout		; display drive name
	ld	hl,emsg2
	call	putmsg
	ld	a,(ptrkn)
	call	adec		; display track number as decimal
	ld	hl,emsg3
	call	putmsg
	ld	a,(psecn)
	call	adec		; display sector number as decimal
	ld	hl,emsg4
	call	putmsg
	call	errmsg		; show the error cause
	pop	bc
	pop	de
	pop	hl
	pop	af
	ret

emsg1:	db	0dh,0ah,'Error on drive ',0
emsg2:	db	', track ',0
emsg3:	db	', sector ',0
emsg4:	db	': ',0

ahex:	push	af
	rrc	a
	rrc	a
	rrc	a
	rrc	a
	call	ahex1
	pop	af
ahex1:	and	0Fh
	cp	10
	jr	c,ahex2
	add	a,7
ahex2:	add	a,'0'
	push	bc
	ld	c,a
	ld	d,0		; console #0
	call	conout
	pop	bc
	ret

adec:	ld	c,'0'
ad2:	cp	10
	jr	c,ad1
	inc	c
	sub	10
	jr	ad2
ad1:	push	af
	call	putc
	pop	af
	add	a,'0'
	ld	c,a
putc:	push	bc
	ld	d,0		; console #0
	call	conout
	pop	bc
	ret

e1:	db	'Recalibration error',0
e3:	db	'Bad sector length',0
e4:	db	'DMA underrun',0
e5:	db	'Interrupt timeout',0
e6:	db	'Invalid function code',0
e7:	db	'Seek error',0

errtab:	dw	e1,eu,e3,e4,e5,e6,e7

e21:	db	'CRC error',0
e22:	db	'DMA overrun',0
e23:	db	'Sector not found',0
e24:	db	'Write protect',0
e25:	db	'Missing address mark',0

errt02:	dw	e25,e24,e23,eu,e22,e21

eu:	db	'Unknown',0

errmsg:	ld	a,(errc)
	or	a
	ret	z
	cp	2
	jr	z,err2
	cp	8
	jr	nc,unknw
	ld	hl,errtab
em2:	dec	a
	jr	z,em1
	inc	hl
	inc	hl
	jr	em2
em1:	ld	a,(hl)
	inc	hl
	ld	h,(hl)
	ld	l,a
	jp	putmsg

unknw:	ld	hl,eu
	call	putmsg
	ld	c,' '
	ld	d,0		; console #0
	call	conout
	ld	a,(errc)
	jp	ahex

err2:	ld	a,(restab+1)
	and	37h
	ld	hl,errt02
em3:	rrc	a
	jr	c,em1
	inc	hl
	inc	hl
	jr	em3

errc:	ds	1
ptrkn:	ds	1
psecn:	ds	1

