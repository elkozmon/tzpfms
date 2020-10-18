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


LDDLLS := rt $(OS_LD_LIBS)
PKGS := libzfs libzfs_core tss2-esys tss2-rc
LDAR := $(LNCXXAR) $(foreach l,,-L$(BLDDIR)$(l)) $(foreach dll,$(LDDLLS),-l$(dll)) $(shell pkg-config --libs $(PKGS))
INCAR := $(foreach l,$(foreach l,,$(l)/include),-isystemext/$(l)) $(foreach l,,-isystem$(BLDDIR)$(l)/include) $(shell pkg-config --cflags $(PKGS))
VERAR := $(foreach l,TZPFMS,-D$(l)_VERSION='$($(l)_VERSION)')
BINARY_SOURCES := $(sort $(wildcard $(SRCDIR)bin/*.cpp $(SRCDIR)bin/**/*.cpp))
COMMON_SOURCES := $(filter-out $(BINARY_SOURCES),$(sort $(wildcard $(SRCDIR)*.cpp $(SRCDIR)**/*.cpp $(SRCDIR)**/**/*.cpp $(SRCDIR)**/**/**/*.cpp)))
# TEST_SOURCES := $(sort $(wildcard $(TSTDIR)*.cpp $(TSTDIR)**/*.cpp $(TSTDIR)**/**/*.cpp $(TSTDIR)**/**/**/*.cpp))
MANPAGE_SOURCES := $(sort $(wildcard $(MANDIR)*.md.pp))


.PHONY : all clean build build-test man
.SECONDARY:


all : build man # build-test test

#test: build-test
#	$(OUTDIR)tzpfms-test$(EXE)

clean :
	rm -rf $(OUTDIR)

build : $(subst $(SRCDIR)bin/,$(OUTDIR),$(subst .cpp,$(EXE),$(BINARY_SOURCES)))
#build-test : $(OUTDIR)tzpfms-test$(EXE)
man : $(OUTDIR)man/index.txt


#$(OUTDIR)tzpfms-test$(EXE) : $(subst $(TSTDIR),$(BLDDIR)test/,$(subst .cpp,$(OBJ),$(TEST_SOURCES))) $(subst $(SRCDIR),$(OBJDIR),$(subst .cpp,$(OBJ),$(filter-out $(SRCDIR)main.cpp,$(SOURCES)))) $(patsubst ext/fmt/src/%.cc,$(BLDDIR)fmt/obj/%$(OBJ),$(wildcard ext/fmt/src/*.cc))
#	$(CXX) $(CXXAR) -o$@ $^ $(PIC) $(LDAR)

$(OUTDIR)man/index.txt : $(MANDIR)index.txt $(patsubst $(MANDIR)%.pp,$(OUTDIR)man/%,$(MANPAGE_SOURCES))
	@mkdir -p $(dir $@)
	cp $< $(dir $@)
	$(RONN) --organization="tzpfms developers"    $(filter-out $<,$^)
	$(RONN) --organization="tzpfms developers" -f $(filter-out $<,$^)


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

$(OUTDIR)man/%.md : $(MANDIR)%.md.pp $(sort $(wildcard $(MANDIR)*.h))
	@mkdir -p $(dir $@)
	$(AWK) '/^#include/ {gsub("\"", "", $$2); while((getline inc < ("$(dir $<)" $$2)) == 1) print inc; next}  {print}' $< > $@
