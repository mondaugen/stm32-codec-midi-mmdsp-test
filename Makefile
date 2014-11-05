# The driver is compiled in different ways, depending on what chip we are
# compiling for.

DEBUG_BUILD?=1
STM_CHIP_SET=STM32F429_439xx
STM_DRIVER_PATH = $(HOME)/Documents/archives/STM32F4xx_DSP_StdPeriph_Lib_V1.3.0/Libraries/STM32F4xx_StdPeriph_Driver
STM_DRIVER_HDRS_STD = stm32f4xx_adc.h \
					  stm32f4xx_crc.h \
					  stm32f4xx_dbgmcu.h \
					  stm32f4xx_dma.h \
					  stm32f4xx_exti.h \
					  stm32f4xx_flash.h \
					  stm32f4xx_gpio.h \
					  stm32f4xx_i2c.h \
					  stm32f4xx_iwdg.h \
					  stm32f4xx_pwr.h \
					  stm32f4xx_rcc.h \
					  stm32f4xx_rtc.h \
					  stm32f4xx_sdio.h \
					  stm32f4xx_spi.h \
					  stm32f4xx_syscfg.h \
					  stm32f4xx_tim.h \
					  stm32f4xx_usart.h \
					  stm32f4xx_wwdg.h \
					  misc.h
STM_DRIVER_HDRS_F40_41 = stm32f4xx_cryp.h \
						 stm32f4xx_hash.h \
						 stm32f4xx_rng.h \
						 stm32f4xx_can.h \
						 stm32f4xx_dac.h \
						 stm32f4xx_dcmi.h \
						 stm32f4xx_fsmc.h
STM_DRIVER_HDRS_F427_437 = stm32f4xx_cryp.h \
						   stm32f4xx_hash.h \
						   stm32f4xx_rng.h \
						   stm32f4xx_can.h \
						   stm32f4xx_dac.h \
						   stm32f4xx_dcmi.h \
						   stm32f4xx_dma2d.h \
						   stm32f4xx_fmc.h \
						   stm32f4xx_sai.h
STM_DRIVER_HDRS_F429_439 = stm32f4xx_cryp.h \
						   stm32f4xx_hash.h \
						   stm32f4xx_rng.h \
						   stm32f4xx_can.h \
						   stm32f4xx_dac.h \
						   stm32f4xx_dcmi.h \
						   stm32f4xx_dma2d.h \
						   stm32f4xx_fmc.h \
						   stm32f4xx_ltdc.h \
						   stm32f4xx_sai.h
STM_DRIVER_SRCS = $(patsubst %.h, $(STM_DRIVER_PATH)/src/%.c, $(STM_DRIVER_HDRS_STD))

ifeq ($(STM_CHIP_SET), STM32F40_41xxx)
	STM_DRIVER_SRCS += $(patsubst %.h, $(STM_DRIVER_PATH)/src/%.c, $(STM_DRIVER_HDRS_F40_41))
endif
ifeq ($(STM_CHIP_SET), STM32F427_437xx)
	STM_DRIVER_SRCS += $(patsubst %.h, $(STM_DRIVER_PATH)/src/%.c, $(STM_DRIVER_HDRS_F427_437))
endif
ifeq ($(STM_CHIP_SET), STM32F429_439xx)
	STM_DRIVER_SRCS += $(patsubst %.h, $(STM_DRIVER_PATH)/src/%.c, $(STM_DRIVER_HDRS_F429_439))
endif

STM_DRIVER_OBJS = $(STM_DRIVER_SRCS:$(STM_DRIVER_PATH)/src/%.c=objs/%.o)
STM_DRIVER_INC  = $(STM_DRIVER_PATH)/inc
STM_DRIVER_DEP  = inc/stm32f4xx_conf.h inc/stm32f4xx.h $(wildcard $(STM_DRIVER_INC)*.h)

CMSIS_PATH = $(HOME)/Documents/archives/CMSIS

MMMIDI_PATH      = $(HOME)/Documents/development/mmmidi
MMMIDI_SRCS_PATH = $(MMMIDI_PATH)/src
MMMIDI_SRCS      = $(wildcard $(MMMIDI_SRCS_PATH)/*.c)
MMMIDI_INC_PATH  = $(MMMIDI_PATH)/inc
MMMIDI_OBJS      = $(MMMIDI_SRCS:$(MMMIDI_SRCS_PATH)/%.c=objs/%.o)
MMMIDI_DEP       = $(wildcard $(MMMIDI_INC_PATH)/*.h)

MMDSP_PATH      = $(HOME)/Documents/development/mm_dsp
MMDSP_SRCS_PATH = $(MMDSP_PATH)/src
MMDSP_SRCS      = $(wildcard $(MMDSP_SRCS_PATH)/*.c)
MMDSP_INC_PATH  = $(MMDSP_PATH)/inc
MMDSP_OBJS      = $(MMDSP_SRCS:$(MMDSP_SRCS_PATH)/%.c=objs/%.o)
MMDSP_DEP       = $(wildcard $(MMDSP_INC_PATH)/*.h)

MMPRIMITIVES_PATH      = $(HOME)/Documents/development/mm_primitives
MMPRIMITIVES_SRCS_PATH = $(MMPRIMITIVES_PATH)/src
MMPRIMITIVES_SRCS      = $(wildcard $(MMPRIMITIVES_SRCS_PATH)/*.c)
MMPRIMITIVES_INC_PATH  = $(MMPRIMITIVES_PATH)/inc
MMPRIMITIVES_OBJS      = $(MMPRIMITIVES_SRCS:$(MMPRIMITIVES_SRCS_PATH)/%.c=objs/%.o)
MMPRIMITIVES_DEP       = $(wildcard $(MMPRIMITIVES_INC_PATH)/*.h)

NEDATASTRUCTURES_PATH      = $(HOME)/Documents/development/ne_datastructures
NEDATASTRUCTURES_SRCS_PATH = $(NEDATASTRUCTURES_PATH)/src
NEDATASTRUCTURES_SRCS      = $(wildcard $(NEDATASTRUCTURES_SRCS_PATH)/*.c)
NEDATASTRUCTURES_INC_PATH  = $(NEDATASTRUCTURES_PATH)/inc
NEDATASTRUCTURES_OBJS      = $(NEDATASTRUCTURES_SRCS:$(NEDATASTRUCTURES_SRCS_PATH)/%.c=objs/%.o)
NEDATASTRUCTURES_DEP       = $(wildcard $(NEDATASTRUCTURES_INC_PATH)/*.h)

PROJ_INC_PATH = ./inc

INC  = $(PROJ_INC_PATH) $(CMSIS_PATH)/Include $(STM_DRIVER_INC) $(MMMIDI_INC_PATH) \
	   $(MMDSP_INC_PATH) $(MMPRIMITIVES_INC_PATH) $(NEDATASTRUCTURES_INC_PATH)

PROJ_SRCS_PATH = src
PROJ_SRCS = $(wildcard $(PROJ_SRCS_PATH)/*.c)
PROJ_OBJS = $(patsubst $(PROJ_SRCS_PATH)/%, objs/%, $(addsuffix .o, $(basename $(PROJ_SRCS))))

PROJ_SRCS_ASM = $(wildcard $(PROJ_SRCS_PATH)/*.s)
PROJ_OBJS_ASM = $(patsubst $(PROJ_SRCS_PATH)/%, objs/%, $(addsuffix .o, $(basename $(PROJ_SRCS_ASM))))

PROJ_DEP = $(wildcard $(PROJ_INC_PATH)/*.h)

PROJ_DATA_PATH = ./data
PROJ_DATA_SRCS = #$(PROJ_DATA_PATH)/grand_piano_59.s #$(wildcard $(PROJ_DATA_PATH)/*.s)
PROJ_DATA_OBJS = $(patsubst $(PROJ_DATA_PATH)/%, objs/%, $(addsuffix .o, $(basename $(PROJ_DATA_SRCS))))

OBJS = $(STM_DRIVER_OBJS) $(PROJ_OBJS) $(PROJ_OBJS_ASM) $(MMMIDI_OBJS) \
	   $(MMDSP_OBJS) $(MMPRIMITIVES_OBJS) $(NEDATASTRUCTURES_OBJS) $(PROJ_DATA_OBJS)

BIN = main.elf

# building for stm32f407 which is part of the family of chips with similar
# peripherals, therefore the following is defined
DEFS    = USE_STDPERIPH_DRIVER $(STM_CHIP_SET)#STM32F429_439xx
CFLAGS  = -ggdb3 -gdwarf-4 -Wall -ffunction-sections -fdata-sections
CFLAGS += -mlittle-endian -mthumb -mcpu=cortex-m4 -mthumb-interwork
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -dD
CFLAGS += $(foreach inc,$(INC),-I$(inc))
CFLAGS += $(foreach def,$(DEFS),-D$(def))

LDSCRIPT = STM32F429ZI_FLASH.ld
LDFLAGS = -T$(LDSCRIPT) -Xlinker
ifeq ($(DEBUG_BUILD),0)
	LDFLAGS += --gc-sections
endif

AS = arm-none-eabi-as
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
LD = arm-none-eabi-ld

OCD	= sudo openocd \
		-f /usr/share/openocd/scripts/board/stm32f4discovery.cfg

all: proj

driver: $(STM_DRIVER_OBJS)

proj: $(BIN)

# compile stm driver
$(STM_DRIVER_OBJS): objs/%.o: $(STM_DRIVER_PATH)/src/%.c $(STM_DRIVER_DEP)
	$(CC) -c $(CFLAGS) $< -o $@

# compile mmmidi
$(MMMIDI_OBJS): objs/%.o: $(MMMIDI_SRCS_PATH)/%.c $(MMMIDI_DEP)
	$(CC) -c $(CFLAGS) $< -o $@
	
# compile mmdsp 
$(MMDSP_OBJS): objs/%.o: $(MMDSP_SRCS_PATH)/%.c $(MMDSP_DEP)
	$(CC) -c $(CFLAGS) $< -o $@
	
# compile mm_primitives
$(MMPRIMITIVES_OBJS): objs/%.o: $(MMPRIMITIVES_SRCS_PATH)/%.c $(MMPRIMITIVES_DEP)
	$(CC) -c $(CFLAGS) $< -o $@

# compile ne_datastructures 
$(NEDATASTRUCTURES_OBJS): objs/%.o: $(NEDATASTRUCTURES_SRCS_PATH)/%.c $(NEDATASTRUCTURES_DEP)
	$(CC) -c $(CFLAGS) $< -o $@

# compile asm
$(PROJ_OBJS_ASM): objs/%.o: $(PROJ_SRCS_PATH)/%.s $(PROJ_DEP)
	$(CC) -c $(CFLAGS) $< -o $@

# compile c
$(PROJ_OBJS): objs/%.o: $(PROJ_SRCS_PATH)/%.c $(PROJ_DEP)
	$(CC) -c $(CFLAGS) $< -o $@

# "compile" raw data
$(PROJ_DATA_OBJS): objs/%.o: $(PROJ_DATA_PATH)/%.s
# this copys the binary (raw) data to the format we want for linking without
# needing to specify the architecture
	$(CC) -c $< -o $@;
# this changes the section name so the data is put into flash instead of ram	
	$(OBJCOPY) --rename-section .text=.rodata $@ $@

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

flash: $(BIN)
	$(OCD) -c init \
		-c "reset halt" \
	    -c "flash write_image erase $(BIN)" \
		-c "reset run" \
	    -c shutdown

clean:
	rm objs/*

ctags:
	ctags -R . $(STM_DRIVER_PATH) $(CMSIS_PATH) $(MMMIDI_PATH) $(MMDSP_PATH) \
		$(MMPRIMITIVES_PATH) $(NEDATASTRUCTURES_PATH)
