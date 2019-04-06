export ISIS_F1=$(HOME)/lib/isis/utils
export ISIS_F2=$(HOME)/lib/isis/plm80
export ISIS_F3=$(HOME)/lib/isis/asm80

all: xsdir.prl

xsdir.prl: dm.plm sn.plm dse.plm dsh.plm dso.plm dp.plm da.plm dts.plm
	@echo "***** Making $@ *****"
	isis :f2:plm80 dm.plm "object(dm) debug nolist"
	isis :f2:plm80 sn.plm "object(sn) debug nolist"
	isis :f2:plm80 dse.plm "object(dse) debug nolist"
	isis :f2:plm80 dsh.plm "object(dsh) debug nolist"
	isis :f2:plm80 dso.plm "object(dso) debug nolist"
	isis :f2:plm80 dp.plm "object(dp) debug nolist"
	isis :f2:plm80 da.plm "object(da) debug nolist"
	isis :f2:plm80 dts.plm "object(dts) debug nolist"
	isis :f1:link x0100,dm,sn,dse,dso,dsh,dp,da,dts,:f1:plm80.lib to d1.lnk
	isis :f1:locate d1.lnk "code(0100H) stacksize(50)"
	@rm d1.lnk
	isis :f1:objhex d1 to d1.hex
	isis :f1:link x0200,dm,sn,dse,dso,dsh,dp,da,dts,:f1:plm80.lib to d2.lnk
	isis :f1:locate d2.lnk "code(0200H) stacksize(50)"
	@rm d2.lnk
	isis :f1:objhex d2 to d2.hex
	zxcc objcpm d1
	cat d1.hex d2.hex > d.hex
	zxcc genmod d.hex xsdir.prl 
	rm *.hex

copy:
	cpmrm -f p112 /dev/fd0 0:xsdir.prl
	cpmcp -f p112 /dev/fd0 xsdir.prl 0:

clean:
	rm -f *.obj *.lst *.lin *.sym *.prl *~
	rm -f dm sn dse dsh dso dp da dts d1 d2
