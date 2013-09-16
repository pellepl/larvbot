BINARY = robot

############
#
# STM Device settings
#
############

FLAGS = -DSTM32F40XX
FLAGS += -DUSE_STDPERIPH_DRIVER
STARTUP = startup_stm32f40xx.s

############
#
# Paths
#
############

sourcedir = src
builddir = build

basetoolsdir = /home/petera/toolchain/gcc/arm-elf-tools-4.8.1
#basetoolsdir = /home/petera/toolchain/gcc/arm-elf-tools-4.7.1
#basetoolsdir = /usr/local/gcc/arm-elf-tools-4.6.3
#/home/petera/toolchain/gcc/arm-elf-tools-4.6.2
#codir = ${basetoolsdir}/lib/gcc/arm-none-eabi/4.8.1/

hfile = ${sourcedir}/config_header.h

stmlibdir = ../stm32f4_lib/STM32F4xx_DSP_StdPeriph_Lib_V1.1.0/Libraries
stmdriverdir = ${stmlibdir}/STM32F4xx_StdPeriph_Driver
stmcmsisdir = ${stmlibdir}/CMSIS/Device/ST/STM32F4xx
stmcmsisdircore = ${stmlibdir}/CMSIS/Include

tools = ${basetoolsdir}/bin

CPATH =
SPATH =
INC =
SFILES =
CFILES =
RFILES = 

#############
#
# Build tools
#
#############

CROSS_COMPILE=${tools}/arm-none-eabi-
#CROSS_COMPILE=${tools}/arm-elf-
CC = $(CROSS_COMPILE)gcc $(COMPILEROPTIONS)
AS = $(CROSS_COMPILE)gcc $(ASSEMBLEROPTIONS)
LD = $(CROSS_COMPILE)ld
GDB = $(CROSS_COMPILE)gdb
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
SIZE = $(CROSS_COMPILE)size
MKDIR = mkdir -p
RM = rm -f

###############
#
# Build configs
#
###############

INCLUDE_DIRECTIVES = 
COMPILEROPTIONS = $(INCLUDE_DIRECTIVES) $(FLAGS) -mcpu=cortex-m4 -mno-thumb-interwork -mthumb -Wall -gdwarf-2
#-ffunction-sections -fdata-sections
COMPILEROPTIONS += -O2
# -nostartfiles -nostdlib 
ASSEMBLEROPTION = $(COMPILEROPTIONS)
LINKERSCRIPT = arm.ld
LINKEROPTIONS = --gc-sections -cref
OBJCOPYOPTIONS_HEX = -O ihex ${builddir}/$(BINARY).elf
OBJCOPYOPTIONS_BIN = -O binary ${builddir}/$(BINARY).elf

BUILD_NUMBER_FILE=build-number.txt

###############
#
# Files and libs
#
###############

include config.mk

# app files
CPATH 		+= ${sourcedir}
SPATH 		+= ${sourcedir}
INC			+= -I./${sourcedir}
CFILES 		+= main.c
CFILES 		+= processor.c
CFILES 		+= stm32f4xx_it.c
CFILES		+= gpio_stm32f4.c
CFILES		+= timer.c
CFILES		+= cli.c

# stm32 lib files
SPATH 		+= ${stmcmsisdir}/Source/Templates/gcc_ride7
SFILES 		+= $(STARTUP)

CPATH		+= ${stmdriverdir}/src ${stmcmsisdir}/Source/Templates ${stmcmsisdircore}
INC			+= -I./${stmdriverdir}/inc 
INC			+= -I./${stmcmsisdir}/Include 
INC			+= -I./${stmcmsisdircore}

CFILES		+= misc.c
CFILES		+= stm32f4xx_adc.c
CFILES		+= stm32f4xx_can.c
CFILES		+= stm32f4xx_crc.c
CFILES		+= stm32f4xx_cryp_aes.c
CFILES		+= stm32f4xx_cryp_des.c
CFILES		+= stm32f4xx_cryp_tdes.c
CFILES		+= stm32f4xx_cryp.c
CFILES		+= stm32f4xx_dac.c
CFILES		+= stm32f4xx_dbgmcu.c
CFILES		+= stm32f4xx_dcmi.c
CFILES		+= stm32f4xx_dma.c
CFILES		+= stm32f4xx_exti.c
CFILES		+= stm32f4xx_flash.c
CFILES		+= stm32f4xx_fsmc.c
CFILES		+= stm32f4xx_gpio.c
CFILES		+= stm32f4xx_hash_md5.c
CFILES		+= stm32f4xx_hash_sha1.c
CFILES		+= stm32f4xx_hash.c
CFILES		+= stm32f4xx_i2c.c
CFILES		+= stm32f4xx_iwdg.c
CFILES		+= stm32f4xx_pwr.c
CFILES		+= stm32f4xx_rcc.c
CFILES		+= stm32f4xx_rng.c
CFILES		+= stm32f4xx_rtc.c
CFILES		+= stm32f4xx_sdio.c
CFILES		+= stm32f4xx_spi.c
CFILES		+= stm32f4xx_syscfg.c
CFILES		+= stm32f4xx_tim.c
CFILES		+= stm32f4xx_usart.c
CFILES		+= stm32f4xx_wwdg.c
		
# cmsis files
CFILES += 	system_stm32f4xx.c

LIBS = 

BINARYEXT = .hex

############
#
# Tasks
#
############


vpath %.c $(CPATH)
vpath %.s $(SPATH)
INCLUDE_DIRECTIVES += $(INC)

SOBJFILES = $(SFILES:%.s=${builddir}/%.o)
OBJFILES = $(CFILES:%.c=${builddir}/%.o)
ROBJFILES = $(RFILES:%.c=${builddir}/%.o)

DEPFILES = $(CFILES:%.c=${builddir}/%.d)
DEPFILES += $(RFILES:%.c=${builddir}/%.d)

ALLOBJFILES  = $(SOBJFILES)
ALLOBJFILES += $(OBJFILES)
ALLOBJFILES += $(ROBJFILES)

DEPENDENCIES = $(DEPFILES) 

# link object files, create binary for flashing
$(BINARY): $(ALLOBJFILES)
	@echo "... build info"
	@if ! test -f $(BUILD_NUMBER_FILE); then echo 0 > $(BUILD_NUMBER_FILE); fi
	@echo $$(($$(cat $(BUILD_NUMBER_FILE)) + 1)) > $(BUILD_NUMBER_FILE)	
	@echo "... linking"
	@${LD} $(LINKEROPTIONS) $(BUILD_NUMBER_LDFLAGS) -T $(LINKERSCRIPT) -Map ${builddir}/$(BINARY).map -o ${builddir}/$(BINARY).elf $(ALLOBJFILES) $(LIBS)
	@echo "... objcopy"
	@${OBJCOPY} $(OBJCOPYOPTIONS_BIN) ${builddir}/$(BINARY).out
	@${OBJCOPY} $(OBJCOPYOPTIONS_HEX) ${builddir}/$(BINARY)$(BINARYEXT) 
	@echo "... disasm"
	@${OBJDUMP} -hd -j .text -j.data -j .bss -j .bootloader_text -j .bootloader_data -d -S ${builddir}/$(BINARY).elf > ${builddir}/$(BINARY)_disasm.s
	@echo "${BINARY}.out is `stat -c%s ${builddir}/${BINARY}.out` bytes on flash"

-include $(DEPENDENCIES)	   	

# compile assembly files, arm
$(SOBJFILES) : ${builddir}/%.o:%.s
		@echo "... assembly $@"
		@${AS} -c -o $@ $<
		
# compile c files
$(OBJFILES) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${CC} -c -o $@ $<

# compile c files designated for ram
$(ROBJFILES) : ${builddir}/%.o:%.c
		@echo "... ram compile $@"
		@${CC} -c -o $@ $< 

# make dependencies
$(DEPFILES) : ${builddir}/%.d:%.c
		@echo "... depend $@"; \
		${RM} $@; \
		${CC} $(COMPILEROPTIONS) -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		${RM} $@.$$$$

all: info mkdirs ${hfile} $(BINARY)

info:
	@echo "* Building to ${builddir}"
	@echo "* Compiler options:  $(COMPILEROPTIONS)" 
	@echo "* Assembler options: $(ASSEMBLEROPTIONS)" 
	@echo "* Linker options:    $(LINKEROPTIONS)" 
	@echo "* Linker script:     ${LINKERSCRIPT}"
	
mkdirs:
	-@${MKDIR} ${builddir}
	
clean:
	@echo ... removing build files in ${builddir}
	@${RM} ${builddir}/*.o
	@${RM} ${builddir}/*.d
	@${RM} ${builddir}/*.out
	@${RM} ${builddir}/*.hex
	@${RM} ${builddir}/*.elf
	@${RM} ${builddir}/*.map
	@${RM} ${builddir}/*_disasm.s
	@${RM} _stm32flash.script

install: binlen = $(shell stat -c%s ${builddir}/${BINARY}.out)
install: $(BINARY)
	@echo "binary length of install is ${binlen} bytes.."
	@sed 's/BUILDFILE/${builddir}\/${BINARY}.out/;s/LENGTH/${binlen}/' stm32flash.script > _stm32flash.script
	@echo "script _stm32flash.script" | nc localhost 4445
	@${RM} _stm32flash.script
	

debug: $(BINARY)
	@${GDB} ${builddir}/${BINARY}.elf -x debug.gdb

${hfile}: config.mk
	@echo "* Generating config header ${hfile}.."
	@echo "// Auto generated file, do not tamper" > ${hfile}
	@echo "#ifdef INCLUDE_CONFIG_HEADER" >> ${hfile}
	@echo "#ifndef _CONFIG_HEADER_H" >> ${hfile}
	@echo "#define _CONFIG_HEADER_H" >> ${hfile}
	@sed -nr 's/([^ \t]*)?[ \t]*=[ \t]*1/#define \1/p' config.mk >> ${hfile}
	@echo "#endif" >> ${hfile}
	@echo "#endif" >> ${hfile}

build-info:
	@echo "*** INCLUDE PATHS"
	@echo "${INC}"
	@echo "*** SOURCE PATHS"
	@echo "${CPATH}"
	@echo "*** ASSEMBLY PATHS"
	@echo "${SPATH}"
	@echo "*** SOURCE FILES"
	@echo "${CFILES}"
	@echo "*** ASSEMBLY FILES"
	@echo "${SFILES}"
	@echo "*** FLAGS"
	@echo "${FLAGS}"
	
############
#
# Build info
#
############

BUILD_NUMBER_LDFLAGS  = --defsym __BUILD_DATE=$$(date +'%Y%m%d')
BUILD_NUMBER_LDFLAGS += --defsym __BUILD_NUMBER=$$(cat $(BUILD_NUMBER_FILE))
