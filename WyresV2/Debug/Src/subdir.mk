################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/hal_core.c \
../Src/hal_cortex.c \
../Src/hal_gpio.c \
../Src/hal_pwr.c \
../Src/hal_pwr_ex.c \
../Src/hal_rcc.c \
../Src/hal_rcc_ex.c \
../Src/hal_subghz.c \
../Src/main.c \
../Src/platform_port_stub.c \
../Src/radio.c \
../Src/radio_board_if.c \
../Src/radio_driver.c \
../Src/radio_fw.c \
../Src/stm32wlxx_it.c \
../Src/subghz.c \
../Src/syscalls.c \
../Src/sysmem.c \
../Src/system_init.c \
../Src/timer.c \
../Src/wyresv2_node.c 

OBJS += \
./Src/hal_core.o \
./Src/hal_cortex.o \
./Src/hal_gpio.o \
./Src/hal_pwr.o \
./Src/hal_pwr_ex.o \
./Src/hal_rcc.o \
./Src/hal_rcc_ex.o \
./Src/hal_subghz.o \
./Src/main.o \
./Src/platform_port_stub.o \
./Src/radio.o \
./Src/radio_board_if.o \
./Src/radio_driver.o \
./Src/radio_fw.o \
./Src/stm32wlxx_it.o \
./Src/subghz.o \
./Src/syscalls.o \
./Src/sysmem.o \
./Src/system_init.o \
./Src/timer.o \
./Src/wyresv2_node.o 

C_DEPS += \
./Src/hal_core.d \
./Src/hal_cortex.d \
./Src/hal_gpio.d \
./Src/hal_pwr.d \
./Src/hal_pwr_ex.d \
./Src/hal_rcc.d \
./Src/hal_rcc_ex.d \
./Src/hal_subghz.d \
./Src/main.d \
./Src/platform_port_stub.d \
./Src/radio.d \
./Src/radio_board_if.d \
./Src/radio_driver.d \
./Src/radio_fw.d \
./Src/stm32wlxx_it.d \
./Src/subghz.d \
./Src/syscalls.d \
./Src/sysmem.d \
./Src/system_init.d \
./Src/timer.d \
./Src/wyresv2_node.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DSTM32WLSINGLE -DSTM32WL -DSTM32WLE5xx -DSTM32WLE5JCIx -DSTM32 -DUSE_HAL_DRIVER -c -I../Inc -I../../Loraprojet/Inc/STM32Cube_FW_WL_V1.5.0/Drivers/CMSIS/Include -I../../Loraprojet/Inc/STM32Cube_FW_WL_V1.5.0/Drivers/CMSIS/Device/ST/STM32WLxx/Include -I../../Loraprojet/Inc/STM32Cube_FW_WL_V1.5.0/Drivers/STM32WLxx_HAL_Driver/Inc -I../../Loraprojet/Inc/STM32Cube_FW_WL_V1.5.0/Drivers/STM32WLxx_HAL_Driver/Inc/Legacy -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/hal_core.cyclo ./Src/hal_core.d ./Src/hal_core.o ./Src/hal_core.su ./Src/hal_cortex.cyclo ./Src/hal_cortex.d ./Src/hal_cortex.o ./Src/hal_cortex.su ./Src/hal_gpio.cyclo ./Src/hal_gpio.d ./Src/hal_gpio.o ./Src/hal_gpio.su ./Src/hal_pwr.cyclo ./Src/hal_pwr.d ./Src/hal_pwr.o ./Src/hal_pwr.su ./Src/hal_pwr_ex.cyclo ./Src/hal_pwr_ex.d ./Src/hal_pwr_ex.o ./Src/hal_pwr_ex.su ./Src/hal_rcc.cyclo ./Src/hal_rcc.d ./Src/hal_rcc.o ./Src/hal_rcc.su ./Src/hal_rcc_ex.cyclo ./Src/hal_rcc_ex.d ./Src/hal_rcc_ex.o ./Src/hal_rcc_ex.su ./Src/hal_subghz.cyclo ./Src/hal_subghz.d ./Src/hal_subghz.o ./Src/hal_subghz.su ./Src/main.cyclo ./Src/main.d ./Src/main.o ./Src/main.su ./Src/platform_port_stub.cyclo ./Src/platform_port_stub.d ./Src/platform_port_stub.o ./Src/platform_port_stub.su ./Src/radio.cyclo ./Src/radio.d ./Src/radio.o ./Src/radio.su ./Src/radio_board_if.cyclo ./Src/radio_board_if.d ./Src/radio_board_if.o ./Src/radio_board_if.su ./Src/radio_driver.cyclo ./Src/radio_driver.d ./Src/radio_driver.o ./Src/radio_driver.su ./Src/radio_fw.cyclo ./Src/radio_fw.d ./Src/radio_fw.o ./Src/radio_fw.su ./Src/stm32wlxx_it.cyclo ./Src/stm32wlxx_it.d ./Src/stm32wlxx_it.o ./Src/stm32wlxx_it.su ./Src/subghz.cyclo ./Src/subghz.d ./Src/subghz.o ./Src/subghz.su ./Src/syscalls.cyclo ./Src/syscalls.d ./Src/syscalls.o ./Src/syscalls.su ./Src/sysmem.cyclo ./Src/sysmem.d ./Src/sysmem.o ./Src/sysmem.su ./Src/system_init.cyclo ./Src/system_init.d ./Src/system_init.o ./Src/system_init.su ./Src/timer.cyclo ./Src/timer.d ./Src/timer.o ./Src/timer.su ./Src/wyresv2_node.cyclo ./Src/wyresv2_node.d ./Src/wyresv2_node.o ./Src/wyresv2_node.su

.PHONY: clean-Src

