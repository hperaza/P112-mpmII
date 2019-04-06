export ISIS_F1=$(HOME)/lib/isis/utils
export ISIS_F2=$(HOME)/lib/isis/plm80
export ISIS_F3=$(HOME)/lib/isis/asm80

all: xmschd.prl sched.brs

sc: scbrs.plm scrsp.plm
	@touch sc

sched.brs: sc
	@echo "***** Making $<rsp.rsp *****"
	isis :f3:asm80 brspbi.asm "pagewidth(80) debug"
	mv brspbi.obj brspbi
	isis :f2:plm80 $<rsp.plm "pagewidth(80) debug"
	isis :f1:link $<rsp.obj to $<rsp.mod
	@rm $<rsp.obj
	isis :f1:locate $<rsp.mod to $<rsp1 "code(0000H) stacksize(0)"
	isis :f1:locate $<rsp.mod to $<rsp2 "code(0100H) stacksize(0)"
	@rm $<rsp.mod
	isis :f1:objhex $<rsp1 to $<rsp1.hex
	isis :f1:objhex $<rsp2 to $<rsp2.hex
	@rm $<rsp2
	zxcc objcpm $<rsp1
	@rm $<rsp1 $<rsp1.com
	cat $<rsp1.hex $<rsp2.hex > $<rsp.hex
	zxcc genmod $<rsp.hex $<rsp.rsp
	@rm $<rsp*.hex
	@echo "***** Making $<rsp.brs *****"
	isis :f2:plm80 $<brs.plm "pagewidth(80) debug"
	isis :f1:link $<brs.obj,brspbi,:f1:plm80.lib to $<brs.mod
	@rm $<brs.obj
	isis :f1:locate $<brs.mod to $<brs1 "code(0000H) stacksize(0)"
	isis :f1:locate $<brs.mod to $<brs2 "code(0100H) stacksize(0)"
	@rm $<brs.mod
	isis :f1:objhex $<brs1 to $<brs1.hex
	isis :f1:objhex $<brs2 to $<brs2.hex
	@rm $<brs2
	zxcc objcpm $<brs1
	@rm $<brs1 $<brs1.com
	cat $<brs1.hex $<brs2.hex > $<brs.hex
	zxcc genmod $<brs.hex $<brs.brs
	@rm $<brs*.hex
	@rm -f sc

xmschd.prl: mschd.plm
	@echo "***** Making $@ *****"
	isis :f2:plm80 mschd.plm nolist debug
	isis :f1:link mschd.obj,x0100,:f1:plm80.lib to mschd1.mod
	isis :f1:locate mschd1.mod "code(0100H) stacksize(100)"
	@rm mschd1.mod
	isis :f1:objhex mschd1 to mschd1.hex
	isis :f1:link mschd.obj,x0200,:f1:plm80.lib to mschd2.mod
	isis :f1:locate mschd2.mod "code(0200H) stacksize(100)"
	@rm mschd2.mod
	isis :f1:objhex mschd2 to mschd2.hex
	@rm mschd2
	zxcc objcpm mschd1
	@rm mschd1.com
	cat mschd1.hex mschd2.hex > mschd.hex
	@rm mschd1.hex mschd2.hex
	@#zxcc zero
	zxcc genmod mschd.hex xmschd.prl 
	rm *.hex

copy:
	cpmrm -f p112 /dev/fd0 0:xmschd.prl
	cpmcp -f p112 /dev/fd0 xmschd.prl 0:

clean:
	rm -f *.obj *.lst *.lin *.sym *.prl brspbi mschd1 *.brs *.rsp sc *~
