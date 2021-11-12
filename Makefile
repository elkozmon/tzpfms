# The MIT License (MIT)

# Copyright (c) 2020 наб <nabijaczleweli@nabijaczleweli.xyz>

# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


include configMakefile


LDDLLS := rt tspi $(OS_LD_LIBS)
PKGS := libzfs libzfs_core tss2-esys tss2-rc
LDAR := $(LNCXXAR) $(foreach l,,-L$(BLDDIR)$(l)) $(foreach dll,$(LDDLLS),-l$(dll)) $(shell pkg-config --libs $(PKGS))
INCAR := $(foreach l,$(foreach l,,$(l)/include),-isystemext/$(l)) $(foreach l,,-isystem$(BLDDIR)$(l)/include) $(shell pkg-config --cflags $(PKGS))
VERAR := $(foreach l,TZPFMS,-D$(l)_VERSION='$($(l)_VERSION)')
BINARY_SOURCES := $(sort $(wildcard $(SRCDIR)bin/*.cpp $(SRCDIR)bin/**/*.cpp))
COMMON_SOURCES := $(filter-out $(BINARY_SOURCES),$(sort $(wildcard $(SRCDIR)*.cpp $(SRCDIR)**/*.cpp $(SRCDIR)**/**/*.cpp $(SRCDIR)**/**/**/*.cpp)))
MANPAGE_HEADERS := $(sort $(wildcard $(MANDIR)*.h))
MANPAGE_SOURCES := $(sort $(wildcard $(MANDIR)*.[012345678].pp))
INITRD_HEADERS := $(sort $(wildcard $(INITRDDIR)*.h))


.PHONY : all clean build shellcheck i-t dracut manpages htmlpages
.SECONDARY:


all : build manpages htmlpages shellcheck i-t dracut

shellcheck : i-t dracut
	find $(OUTDIR)initramfs-tools/ $(OUTDIR)dracut -name '*.sh' -exec echo $(SHELLCHECK) --exclude SC1091 {} + | sh -x

clean :
	rm -rf $(OUTDIR)

build : $(subst $(SRCDIR)bin/,$(OUTDIR),$(subst .cpp,$(EXE),$(BINARY_SOURCES)))
manpages : $(patsubst $(MANDIR)%.pp,$(OUTDIR)man/%,$(MANPAGE_SOURCES))
htmlpages : $(patsubst $(MANDIR)%.pp,$(OUTDIR)man/%.html,$(MANPAGE_SOURCES)) $(OUTDIR)man/style.css
i-t : $(OUTDIR)initramfs-tools/usr/share/initramfs-tools/hooks/tzpfms $(OUTDIR)initramfs-tools/usr/share/tzpfms/initramfs-tools-zfs-patch.sh
dracut : $(patsubst $(INITRDDIR)dracut/%,$(OUTDIR)dracut/usr/lib/dracut/modules.d/91tzpfms/%,$(sort $(wildcard $(INITRDDIR)dracut/*.sh)))


$(OUTDIR)initramfs-tools/usr/share/initramfs-tools/hooks/tzpfms: $(INITRDDIR)initramfs-tools/hook $(INITRD_HEADERS)
	@mkdir -p $(dir $@)
	$(AWK) -f pp.awk $< > $@
	chmod --reference $< $@

$(OUTDIR)initramfs-tools/usr/share/tzpfms/initramfs-tools-zfs-patch.sh: $(INITRDDIR)initramfs-tools/zfs-patch.sh $(INITRD_HEADERS)
	@mkdir -p $(dir $@)
	$(AWK) -f pp.awk $< > $@
	chmod --reference $< $@

# The d-v-o-s string starts at "BSD" (hence the "BSD General Commands Manual" default); we're not BSD, so hide it
# Can't put it at the very top, since man(1) only loads mdoc *after* the first mdoc macro (.Dd in our case)
$(OUTDIR)man/% : $(MANDIR)%.pp $(MANPAGE_HEADERS)
	@mkdir -p $(dir $@)
	$(AWK) -f pp.awk $< | $(AWK) '/^$$/ {prev_empty=1; next} $$1 == "#" && $$2 ~ /^[0-9]*$$/ {prev_empty=0; next}  {if(prev_empty) print ""; prev_empty=0; print}' | $(AWK) '$$0 == ".Dd" {$$2 = "$(TZPFMS_DATE)"}  $$1 == ".Dt" { print ".ds doc-volume-operating-system" }  $$0 == ".Os" {$$2 = "tzpfms"; $$3 = "$(TZPFMS_VERSION)"}  {print}' > $@
	! $(MANDOC) -Tlint $@ 2>&1 | grep -vE -e 'mandoc: outdated mandoc.db' -e 'STYLE: referenced manual not found' -e 'STYLE: operating system explicitly specified: Os tzpfms' -e 'WARNING: cross reference to self: Xr zfs-tpm.*-change-key 8' -e 'STYLE: input text line longer than 80 bytes'
# The "WARNING: unknown font, skipping request: TS.+fC[RBI]" one: see https://bugs.debian.org/992002

$(OUTDIR)man/%.html : $(OUTDIR)man/%
	@mkdir -p $(dir $@)
	( cd $(OUTDIR)man/ && $(MANDOC) -Thtml -Ostyle="style.css",man="%N.%S.html;https://manpages.debian.org/bullseye/%N.%S" ) < $< | \
		$(AWK) '/^<h1/ {in_syn = $$0 ~ /id="SYNOPSIS"/}  /^<br/ {if(in_syn) next}  {print}' | \
		$(SED) -Ee 's/ title=".."//g' -e 's/<a class="permalink" href="#([^"]*)"><span class="No" id="([^"]*)">/<a><span class="No">/g' -e 's#manpages.debian.org/[^/]*/ESYS_CONTEXT.3#mankier.com/3/ESYS_CONTEXT#g' > $@

$(OUTDIR)man/style.css : man/style.css
	@mkdir -p $(dir $@)
	cp $^ $@


$(OUTDIR)%$(EXE) : $(subst $(SRCDIR),$(OBJDIR),$(subst .cpp,$(OBJ),$(SRCDIR)bin/%.cpp $(COMMON_SOURCES)))
	@mkdir -p $(dir $@)
	$(CXX) $(CXXAR) -o$@ $^ $(PIC) -Wl,--as-needed $(LDAR)
	$(STRIP) $(STRIPAR) $@

$(OBJDIR)%$(OBJ) : $(SRCDIR)%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXAR) $(INCAR) $(VERAR) -c -o$@ $^

$(BLDDIR)test/%$(OBJ) : $(TSTDIR)%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXAR) $(INCAR) -I$(SRCDIR) $(VERAR) -c -o$@ $^

$(OUTDIR)dracut/usr/lib/dracut/modules.d/91tzpfms/% : $(INITRDDIR)dracut/% $(INITRD_HEADERS)
	@mkdir -p $(dir $@)
	$(AWK) -f pp.awk $< > $@
	chmod --reference $< $@
