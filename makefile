#
# TNFSTEST makefile
#

build_dir = build
cc        = wcc
cflags    = -bt=dos -ms -3 -s -wx

all: $(build_dir)/tnfstest.exe

$(build_dir):
	mkdir -p $(build_dir)

$(build_dir)/tnfstest.obj: tnfstest.c | $(build_dir)
	$(cc) $(cflags) -fo=$@ $<

$(build_dir)/tnfstest.exe: $(build_dir)/tnfstest.obj
	wlink system dos \
		option map=$(build_dir)/tnfstest.map \
		name $@ \
		file { $< }

clean:
	rm -f $(build_dir)/*.obj $(build_dir)/*.exe $(build_dir)/*.map $(build_dir)/*.err
