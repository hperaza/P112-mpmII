export ISIS_F1=$(HOME)/lib/isis/utils
export ISIS_F2=$(HOME)/lib/isis/plm80
export ISIS_F3=$(HOME)/lib/isis/asm80

all: xtod.prl

xtod.prl:
	isis :f2:plm80 tod.plm nolist debug
	isis :f1:link tod.obj,x0100,:f1:plm80.lib to tod1.mod
	isis :f1:locate "tod1.mod code(0100H) stacksize(100)"
	@rm tod1.mod
	isis :f1:objhex tod1 to tod1.hex
	isis :f1:link tod.obj,x0200,:f1:plm80.lib to tod2.mod
	isis :f1:locate "tod2.mod code(0200H) stacksize(100)"
	@rm tod2.mod
	isis :f1:objhex tod2 to tod2.hex
	@rm tod2
	zxcc objcpm tod1
	@rm tod1.com
	@cat tod1.hex tod2.hex > tod.hex
	@rm tod1.hex tod2.hex
	@#zxcc zero
	zxcc genmod tod.hex xtod.prl
	@rm *.hex

copy:
	cpmrm -f p112 /dev/fd0 0:xtod.prl
	cpmcp -f p112 /dev/fd0 xtod.prl 0:

clean:
	rm -f *.obj *.lst *.lin *.sym tod1* xtod.prl *~
