# Copyright (c) 2011-2019 Columbia University, System Level Design Group
# SPDX-License-Identifier: Apache-2.0

ACCELERATORS_PATH      = $(ESP_ROOT)/accelerators
ACCELERATORS           = $(filter-out common, $(shell ls -d $(ACCELERATORS_PATH)/*/ | awk -F/ '{print $$(NF-1)}'))
ACCELERATORS-wdir      = $(addsuffix -wdir, $(ACCELERATORS))
ACCELERATORS-hls       = $(addsuffix -hls, $(ACCELERATORS))
ACCELERATORS-clean     = $(addsuffix -clean, $(ACCELERATORS))
ACCELERATORS-distclean = $(addsuffix -distclean, $(ACCELERATORS))
ACCELERATORS-sim       = $(addsuffix -sim, $(ACCELERATORS))
ACCELERATORS-plot      = $(addsuffix -plot, $(ACCELERATORS))


CHISEL_PATH                   = $(ESP_ROOT)/chisel
CHISEL_ACC_PATH               = $(CHISEL_PATH)/src/main/scala/esp/examples
CHISEL_ACCELERATORS           = $(shell ls $(CHISEL_ACC_PATH)/*.scala | awk -F/ '{print $$(NF)}' | sed 's/\.scala//g')
CHISEL_ACCELERATORS-clean     = $(addsuffix -clean, $(CHISEL_ACCELERATORS))
CHISEL_ACCELERATORS-distclean = $(addsuffix -distclean, $(CHISEL_ACCELERATORS))


THIRDPARTY_PATH                   = $(ESP_ROOT)/third-party/accelerators/dma$(NOC_WIDTH)
THIRDPARTY_ACCELERATORS           = $(shell ls $(THIRDPARTY_PATH))
THIRDPARTY_ACCELERATORS-clean     = $(addsuffix -clean, $(THIRDPARTY_ACCELERATORS))
THIRDPARTY_ACCELERATORS-distclean = $(addsuffix -distclean, $(THIRDPARTY_ACCELERATORS))

THIRDPARTY_VLOG       = $(foreach acc, $(THIRDPARTY_ACCELERATORS), $(THIRDPARTY_PATH)/$(acc)/$(acc)_wrapper.v)
THIRDPARTY_VLOG      += $(foreach acc, $(THIRDPARTY_ACCELERATORS), $(foreach rtl, $(shell strings $(THIRDPARTY_PATH)/$(acc)/$(acc).verilog),  $(shell f=$(THIRDPARTY_PATH)/$(acc)/out/$(rtl); if test -e $$f; then echo $$f; fi;)))
THIRDPARTY_INCDIR     = $(foreach acc, $(THIRDPARTY_ACCELERATORS), $(THIRDPARTY_PATH)/$(acc)/vlog_incdir)
THIRDPARTY_SVLOG      = $(foreach acc, $(THIRDPARTY_ACCELERATORS), $(foreach rtl, $(shell strings $(THIRDPARTY_PATH)/$(acc)/$(acc).sverilog), $(shell f=$(THIRDPARTY_PATH)/$(acc)/out/$(rtl); if test -e $$f; then echo $$f; fi;)))
THIRDPARTY_VHDL_PKGS  = $(foreach acc, $(THIRDPARTY_ACCELERATORS), $(foreach rtl, $(shell strings $(THIRDPARTY_PATH)/$(acc)/$(acc).pkgs),     $(shell f=$(THIRDPARTY_PATH)/$(acc)/out/$(rtl); if test -e $$f; then echo $$f; fi;)))
THIRDPARTY_VHDL       = $(foreach acc, $(THIRDPARTY_ACCELERATORS), $(foreach rtl, $(shell strings $(THIRDPARTY_PATH)/$(acc)/$(acc).vhdl),     $(shell f=$(THIRDPARTY_PATH)/$(acc)/out/$(rtl); if test -e $$f; then echo $$f; fi;)))

THIRDPARTY_INCDIR_MODELSIM = $(foreach dir, $(THIRDPARTY_INCDIR), +incdir+$(dir))
THIRDPARTY_INCDIR_XCELIUM  = $(foreach dir, $(THIRDPARTY_INCDIR), -INCDIR $(dir))
THIRDPARTY_INCDIR_INCISIVE = $(THIRDPARTY_INCDIR_XCELIUM)


ACCELERATORS-driver       = $(addsuffix -driver, $(ACCELERATORS)) $(addsuffix -driver, $(CHISEL_ACCELERATORS))
ACCELERATORS-driver-clean = $(addsuffix -driver-clean, $(ACCELERATORS)) $(addsuffix -driver-clean, $(CHISEL_ACCELERATORS))
ACCELERATORS-app          = $(addsuffix -app, $(ACCELERATORS)) $(addsuffix -app, $(CHISEL_ACCELERATORS))
ACCELERATORS-app-clean    = $(addsuffix -app-clean, $(ACCELERATORS)) $(addsuffix -app-clean, $(CHISEL_ACCELERATORS))
ACCELERATORS-barec        = $(addsuffix -barec, $(ACCELERATORS)) $(addsuffix -barec, $(CHISEL_ACCELERATORS))
ACCELERATORS-barec-clean  = $(addsuffix -barec-clean, $(ACCELERATORS)) $(addsuffix -barec-clean, $(CHISEL_ACCELERATORS))


print-available-accelerators:
	$(QUIET_INFO)echo "Available accelerators generated from Stratus HLS: $(ACCELERATORS)"
	$(QUIET_INFO)echo "Available accelerators generated from Chisel3: $(CHISEL_ACCELERATORS)"
	$(QUIET_INFO)echo "Available third-party accelerators: $(THIRDPARTY_ACCELERATORS)"


### Chisel ###
sbt-run:
	$(QUIET_RUN)
	@cd $(CHISEL_PATH); sbt run;

$(CHISEL_ACCELERATORS):
	$(QUIET_BUILD)
	@if ! test -e $(CHISEL_PATH)/build/$@; then \
		$(MAKE) sbt-run && rm -rf $(ESP_ROOT)/tech/$(TECHLIB)/acc/$@; \
		cp -r $(CHISEL_PATH)/build/$@ $(ESP_ROOT)/tech/$(TECHLIB)/acc/; \
	fi;
	@if test -e $(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log; then \
		sed -i '/$@/d' $(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log; \
	fi; \
	echo "$@" >> $(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log;

chisel-accelerators: $(CHISEL_ACCELERATORS)

$(CHISEL_ACCELERATORS-clean):
	$(QUIET_CLEAN)
	@cd $(CHISEL_PATH); $(RM) build/$(@:-clean=)

$(CHISEL_ACCELERATORS-distclean): %-distclean : %-clean
	$(QUIET_CLEAN)
	@$(RM) $(ESP_ROOT)/tech/$(TECHLIB)/acc/$(@:-distclean=);
	@if test -e $(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log; then \
		sed -i '/$(@:-distclean=)/d' $(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log; \
	fi;

chisel-accelerators-clean:
	$(QUIET_CLEAN)
	@cd $(CHISEL_PATH); sbt clean; $(RM) build

chisel-accelerators-distclean: chisel-accelerators-clean $(CHISEL_ACCELERATORS-distclean)

.PHONY: sbt-run chisel-accelerators chisel-accelerators-clean chisel-accelerators-distclean $(CHISEL_ACCELERATORS) $(CHISEL_ACCELERATORS-clean) $(CHISEL_ACCELERATORS-distclean)


### Third-Party ###
$(THIRDPARTY_ACCELERATORS):
	$(QUIET_BUILD)
	@if ! test -e $(THIRDPARTY_PATH)/$@/out; then \
		cd $(THIRDPARTY_PATH)/$@; \
		$(MAKE); \
	fi;

thirdparty-accelerators: $(THIRDPARTY_ACCELERATORS)

$(THIRDPARTY_ACCELERATORS-clean):
	$(QUIET_CLEAN)
	@cd $(THIRDPARTY_PATH)/$(@:-clean=); \
	$(MAKE) clean;

$(THIRDPARTY_ACCELERATORS-distclean): %-distclean : %-clean
	$(QUIET_CLEAN)
	@cd $(THIRDPARTY_PATH)/$(@:-distclean=); \
	$(MAKE) distclean;

thirdparty-accelerators-clean: $(THIRDPARTY_ACCELERATORS-clean)

thirdparty-accelerators-distclean: $(THIRDPARTY_ACCELERATORS-distclean)

.PHONY: thirdparty-accelerators thirdparty-accelerators-clean thirdparty-accelerators-distclean $(THIRDPARTY_ACCELERATORS) $(THIRDPARTY_ACCELERATORS-clean) $(THIRDPARTY_ACCELERATORS-distclean)


### Stratus HLS ###
$(ACCELERATORS-wdir):
	$(QUIET_MKDIR)mkdir -p $(ACCELERATORS_PATH)/$(@:-wdir=)/hls-work-$(TECHLIB)
	@cd $(ACCELERATORS_PATH)/$(@:-wdir=)/hls-work-$(TECHLIB); \
	if ! test -e project.tcl; then \
		cp ../stratus/* .; \
		rm -f project.tcl; \
		rm -f Makefile; \
		ln -s ../stratus/project.tcl; \
		ln -s ../stratus/Makefile; \
	fi;

$(ACCELERATORS-hls): %-hls : %-wdir
	$(QUIET_MAKE)ACCELERATOR=$(@:-hls=) TECH=$(TECHLIB) ESP_ROOT=$(ESP_ROOT) make -C $(ACCELERATORS_PATH)/$(@:-hls=)/hls-work-$(TECHLIB) memlib | tee $(@:-hls=)_memgen.log
	$(QUIET_INFO)echo "Running HLS for available implementations of $(@:-hls=)"
	$(QUIET_MAKE)ACCELERATOR=$(@:-hls=) TECH=$(TECHLIB) ESP_ROOT=$(ESP_ROOT) make -C $(ACCELERATORS_PATH)/$(@:-hls=)/hls-work-$(TECHLIB) hls_all | tee $(@:-hls=)_hls.log
	$(QUIET_INFO)echo "Installing available implementations for $(@:-hls=) to $(ESP_ROOT)/tech/$(TECHLIB)/acc/$(@:-hls=)"
	$(QUIET_MAKE)ACCELERATOR=$(@:-hls=) TECH=$(TECHLIB) ESP_ROOT=$(ESP_ROOT) make -C $(ACCELERATORS_PATH)/$(@:-hls=)/hls-work-$(TECHLIB) install
	@if test -e $(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log; then \
		sed -i '/$(@:-hls=)/d' $(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log; \
	fi;
	@echo "$(@:-hls=)" >> $(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log

$(ACCELERATORS-sim): %-sim : %-wdir
	$(QUIET_RUN)ACCELERATOR=$(@:-sim=) TECH=$(TECHLIB) ESP_ROOT=$(ESP_ROOT) make -C $(ACCELERATORS_PATH)/$(@:-sim=)/hls-work-$(TECHLIB) sim_all | tee $(@:-sim=)_sim.log

$(ACCELERATORS-plot): %-plot : %-wdir
	$(QUIET_RUN)ACCELERATOR=$(@:-plot=) TECH=$(TECHLIB) ESP_ROOT=$(ESP_ROOT) make -C $(ACCELERATORS_PATH)/$(@:-plot=)/hls-work-$(TECHLIB) plot

$(ACCELERATORS-clean): %-clean : %-wdir
	$(QUIET_CLEAN)ACCELERATOR=$(@:-clean=) TECH=$(TECHLIB) ESP_ROOT=$(ESP_ROOT) make -C $(ACCELERATORS_PATH)/$(@:-clean=)/hls-work-$(TECHLIB) clean
	@$(RM) $(@:-clean=)*.log

$(ACCELERATORS-distclean): %-distclean : %-wdir
	$(QUIET_CLEAN)ACCELERATOR=$(@:-distclean=) TECH=$(TECHLIB) ESP_ROOT=$(ESP_ROOT) make -C $(ACCELERATORS_PATH)/$(@:-distclean=)/hls-work-$(TECHLIB) distclean
	@$(RM) $(@:-distclean=)*.log
	@if test -e $(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log; then \
		sed -i '/$(@:-distclean=)/d' $(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log; \
	fi;

.PHONY: print-available-accelerators $(ACCELERATORS-wdir) $(ACCELERATORS-hls) $(ACCELERATORS-sim) $(ACCELERATORS-plot) $(ACCELERATORS-clean) $(ACCELERATORS-distclean)

accelerators: $(ACCELERATORS-hls)

accelerators-clean: $(ACCELERATORS-clean)

accelerators-distclean: $(ACCELERATORS-distclean)

.PHONY: accelerators accelerators-clean accelerators-distclean

### Common ###
$(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log:
	touch $@

SLDGEN_DEPS  = $(ESP_ROOT)/tech/$(TECHLIB)/acc/installed.log
SLDGEN_DEPS += $(ESP_ROOT)/utils/sldgen/sld_generate.py
SLDGEN_DEPS += $(wildcard $(ESP_ROOT)/utils/sldgen/templates/*.vhd)

## ESP Wrappers ##
sldgen: $(SLDGEN_DEPS)
	$(QUIET_MKDIR) $(RM) $@; mkdir -p $@
	$(QUIET_RUN)$(ESP_ROOT)/utils/sldgen/sld_generate.py $(NOC_WIDTH) $(ESP_ROOT)/tech/$(TECHLIB) $(ESP_ROOT)/third-party $(ESP_ROOT)/utils/sldgen/templates ./sldgen
	@touch $@

sldgen-clean:

sldgen-distclean: sldgen-clean
	$(QUIET_CLEAN)$(RM) sldgen

.PHONY: sldgen-clean sldgen-distclean

## Device Drivers ##
$(ACCELERATORS-driver): sysroot linux-build/vmlinux
	@if test -e $(DRIVERS)/$(@:-driver=)/linux/$(@:-driver=).c; then \
		echo '   ' MAKE $@; mkdir -p sysroot/opt/drivers; \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE_LINUX) KSRC=$(PWD)/linux-build $(MAKE) ESP_CORE_PATH=$(ESP_CORE_PATH) -C $(DRIVERS)/$(@:-driver=)/linux; \
		if test -e $(DRIVERS)/$(@:-driver=)/linux/$(@:-driver=).ko; then \
			echo '   ' CP $@; cp $(DRIVERS)/$(@:-driver=)/linux/$(@:-driver=).ko sysroot/opt/drivers/; \
		else \
			echo '   ' WARNING $@ compilation failed!; \
		fi; \
	else \
		echo '   ' WARNING $@ not found!; \
	fi;

$(ACCELERATORS-driver-clean):
	$(QUIET_CLEAN) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE_LINUX) KSRC=$(PWD)/linux-build $(MAKE) ESP_CORE_PATH=$(ESP_CORE_PATH) -C $(DRIVERS)/$(@:-driver-clean=)/linux clean


$(ACCELERATORS-app): sysroot
	@if [ `ls -1 $(DRIVERS)/$(@:-app=)/app/*.c 2>/dev/null | wc -l ` -gt 0 ]; then \
		echo '   ' MAKE $@; mkdir -p sysroot/applications/test/; \
		CROSS_COMPILE=$(CROSS_COMPILE_LINUX) $(MAKE) -C $(DRIVERS)/$(@:-app=)/app; \
		if [ `ls -1 $(DRIVERS)/$(@:-app=)/app/*.exe 2>/dev/null | wc -l ` -gt 0 ]; then \
			echo '   ' CP $@; cp $(DRIVERS)/$(@:-app=)/app/*.exe sysroot/applications/test/; \
		else \
			echo '   ' WARNING $@ compilation failed!; \
		fi; \
	else \
		echo '   ' WARNING $@ not found!; \
	fi;

$(ACCELERATORS-app-clean):
	$(QUIET_CLEAN) CROSS_COMPILE=$(CROSS_COMPILE_LINUX) $(MAKE) -C $(DRIVERS)/$(@:-app-clean=)/app clean

$(ACCELERATORS-barec): barec
	@if [ `ls -1 $(DRIVERS)/$(@:-barec=)/barec/*.c 2>/dev/null | wc -l ` -gt 0 ]; then \
		echo '   ' MAKE $@; \
		CROSS_COMPILE=$(CROSS_COMPILE_ELF) DESIGN_PATH=$(DESIGN_PATH) $(MAKE) -C $(DRIVERS)/$(@:-barec=)/barec; \
		if [ `ls -1 $(DRIVERS)/$(@:-barec=)/barec/*.bin 2>/dev/null | wc -l ` -gt 0 ]; then \
			echo '   ' CP $@; cp $(DRIVERS)/$(@:-barec=)/barec/*.bin barec; \
		fi; \
		if [ `ls -1 $(DRIVERS)/$(@:-barec=)/barec/*.exe 2>/dev/null | wc -l ` -gt 0 ]; then \
			echo '   ' CP $@; cp $(DRIVERS)/$(@:-barec=)/barec/*.exe barec; \
		else \
			echo '   ' WARNING $@ compilation failed!; \
		fi; \
	else \
		echo '   ' WARNING $@ not found!; \
	fi;

$(ACCELERATORS-barec-clean):
	$(QUIET_CLEAN) CROSS_COMPILE=$(CROSS_COMPILE_ELF) $(MAKE) -C $(DRIVERS)/$(@:-barec-clean=)/barec clean


.PHONY: $(ACCELERATORS-driver) $(ACCELERATORS-driver-clean) $(ACCELERATORS-app) $(ACCELERATORS-app-clean) $(ACCELERATORS-barec) $(ACCELERATORS-barec-clean)

accelerators-driver: $(ACCELERATORS-driver)

accelerators-driver-clean: $(ACCELERATORS-driver-clean) contig-clean esp-clean esp-cache-clean

accelerators-app: $(ACCELERATORS-app)

accelerators-app-clean: $(ACCELERATORS-app-clean) test-clean

accelerators-barec: $(ACCELERATORS-barec)

accelerators-barec-clean: $(ACCELERATORS-barec-clean) probe-clean

esp-clean:
	$(QUIET_CLEAN) CROSS_COMPILE=$(CROSS_COMPILE_LINUX) ARCH=$(ARCH) KSRC=$(PWD)/linux-build $(MAKE) -C $(DRIVERS)/esp clean

esp-cache-clean:
	$(QUIET_CLEAN) CROSS_COMPILE=$(CROSS_COMPILE_LINUX) ARCH=$(ARCH) KSRC=$(PWD)/linux-build $(MAKE) -C $(DRIVERS)/esp_cache clean

contig-clean:
	$(QUIET_CLEAN) CROSS_COMPILE=$(CROSS_COMPILE_LINUX) ARCH=$(ARCH) KSRC=$(PWD)/linux-build $(MAKE) -C $(DRIVERS)/contig_alloc clean-libcontig

probe-clean:
	$(QUIET_CLEAN) CROSS_COMPILE=$(CROSS_COMPILE_ELF) $(MAKE) -C $(DRIVERS)/probe clean

test-clean:
	$(QUIET_CLEAN) CROSS_COMPILE=$(CROSS_COMPILE_LINUX) $(MAKE) -C $(DRIVERS)/test clean

.PHONY: accelerators-driver accelerators-driver-clean accelerators-app accelerators-app-clean accelerators-barec accelerators-barec-clean probe-clean contig-clean esp-clean esp-cache-clean test-clean
