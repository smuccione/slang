ifndef $(slang_new_dir)
	slang_new_dir=c:\slang\release\x64
endif

slang_new=$(slang_new_dir)\slang.exe
slink_new=$(slang_new_dir)\slink.exe
slib_new=$(slang_new_dir)\slib.exe

slang_old=c:\sdweb\slang.exe
slink_old=c:\sdweb\slink.exe
slib_old=c:\sdweb\slib.exe

all : $(addsuffix .flb,$(libs)) $(addsuffix .sll,$(libs))

old : $(addsuffix .flb,$(libs))

new : $(addsuffix .sll,$(libs))

clean :
	@erase *.fgx
	@erase *.flb
	@erase *.slo
	@erase *.slb
	@erase *.sll

%.flb : $(%_sources:.fgl=.fgx)
	$(slib_old) -$@ $(addprefix +,$^)

%.fgx : %.fgl
	$(slang_old) $<

%.sll : %.slb
	$(slink_new) $@ $^ -lib
	copy $@ $(agisent_dir)\libs

%.slb : $(%_sources:.sl=.slo,.fgl=.slo)
	$(slib_new) -$@ $(addprefix +,$^)
	copy $@ $(agisent_dir)\libs

%.slo : %.fgl
	$(slang_new) $<

