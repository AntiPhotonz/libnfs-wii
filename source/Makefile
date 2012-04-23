#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

ifeq ($(PLATFORM),wii)
include $(DEVKITPPC)/wii_rules
endif

ifeq ($(PLATFORM),cube)
include $(DEVKITPPC)/gamecube_rules
endif

#---------------------------------------------------------------------------------
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
BUILD           ?=      build 
SOURCES         :=      .

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS          =       -O2 -Wall -Wno-unused $(MACHDEP) $(INCLUDE) -DGEKKO -DWORDS_BIGENDIAN
CXXFLAGS        =       $(CFLAGS)
ASFLAGS         :=      -g
export EXT2BIN  :=      $(LIBDIR)/$(PLATFORM)/libnfs.a

ifeq ($(BUILD),cube_debug)
CFLAGS += -DDEBUG_GEKKO
CXXFLAGS += -DDEBUG_GEKKO
endif
ifeq ($(BUILD),wii_debug)
CFLAGS += -DDEBUG_GEKKO
CXXFLAGS += -DDEBUG_GEKKO
endif

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS            :=

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS         :=

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export DEPSDIR  :=      $(CURDIR)/$(BUILD)

export VPATH    :=      $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                        $(foreach dir,$(DATA),$(CURDIR)/$(dir))

CFILES          :=      $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))

export OFILES   :=      $(CFILES:.c=.o)

export INCLUDE  :=      -I$(CURDIR)/$(BUILD) \
                        -I$(LIBOGC_INC)

export LIBPATHS :=      -L$(LIBOGC_LIB)

.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr wii_debug wii_release cube_debug cube_release $(LIBDIR)

all: $(EXT2BIN)

install:
	cp nfs.h $(DEVKITPRO)/libogc/include
	cp ../lib/wii/libnfs.a $(DEVKITPRO)/libogc/lib/wii
	cp ../lib/cube/libnfs.a $(DEVKITPRO)/libogc/lib/cube

wii-install:
	cp nfs.h $(DEVKITPRO)/libogc/include
	cp ../lib/wii/libnfs.a $(DEVKITPRO)/libogc/lib/wii

#---------------------------------------------------------------------------------
else

DEPENDS :=      $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(EXT2BIN): $(OFILES) $(LIBDIR)/$(PLATFORM)
	@rm -f "../$(EXT2BIN)"
	@$(AR) rcs "../$(EXT2BIN)" $(OFILES)
	@echo built ... $(notdir $@)

$(LIBDIR)/$(PLATFORM):
	mkdir -p ../$(LIBDIR)/$(PLATFORM)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------

