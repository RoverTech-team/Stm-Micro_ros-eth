TARGET = Polispace_Stm_CM4_Dual_Smoke

DEBUG = 1
OPT = -Og

BUILD_DIR = build

C_SOURCES =  \
../../CM4/Core/Src/main_smoke_dualcore_cm4.c \
../../CM4/Core/Src/stm32h7xx_it.c \
../../CM4/Core/Src/stm32h7xx_hal_msp.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hsem.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdma.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_exti.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_usart.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_usart_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart.c \
../../Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c \
../../CM4/Core/Src/sysmem.c \
../../CM4/Core/Src/syscalls.c

ASM_SOURCES = \
../CM4/startup_stm32h755xx_CM4.s

PREFIX = arm-none-eabi-
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16
FLOAT-ABI = -mfloat-abi=soft
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

C_DEFS = \
-DCORE_CM4 \
-DUSE_HAL_DRIVER \
-DSTM32H755xx \
-DUSE_PWR_DIRECT_SMPS_SUPPLY

C_INCLUDES = \
-I../../CM4/Core/Inc \
-I../../Drivers/STM32H7xx_HAL_Driver/Inc \
-I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy \
-I../../Drivers/BSP/STM32H7xx_Nucleo \
-I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include \
-I../../Drivers/CMSIS/Include

ASFLAGS = $(MCU) $(OPT) -Wall -fdata-sections -ffunction-sections
CFLAGS += $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

LDSCRIPT = ../CM4/stm32h755xx_flash_CM4.ld

LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir $@

clean:
	-rm -fR $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)
