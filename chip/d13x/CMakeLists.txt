set(AIC_VENDOR_DIR ${CMAKE_SOURCE_DIR}/../vendor/artinchip/chips/artinchips)
set(AIC_COMMON_DIR ${AIC_VENDOR_DIR}/common)
set(AIC_HAL_DIR ${AIC_VENDOR_DIR}/hal)
set(AIC_DRV_DIR ${AIC_VENDOR_DIR}/drv)
set(AIC_D12X_DIR ${AIC_VENDOR_DIR}/d12x)

set(SRCS
    d13x_head.S
    d13x_irq.c
    d13x_start.c
    d13x_boot.c
    d13x_serial.c
    ${AIC_COMMON_DIR}/utils/aic_time.c
    ${AIC_COMMON_DIR}/utils/aic_string.c
    ${AIC_COMMON_DIR}/utils/pinmux_utils.c
    ${AIC_COMMON_DIR}/private_param/private_param.c
    ${AIC_COMMON_DIR}/partition/disk_part.c
    ${AIC_COMMON_DIR}/partition/partition.c
    ${AIC_COMMON_DIR}/crc32/crc32.c
    ${AIC_D12X_DIR}/d12x_clk_cfg.c
    ${AIC_D12X_DIR}/d12x_reset_cfg.c
    ${AIC_D12X_DIR}/sys_freq.c
    ${AIC_D12X_DIR}/aic_gpio_id.c
    ${AIC_D12X_DIR}/boot_param.c
    ${AIC_HAL_DIR}/cmu/aic_hal_clk.c
    ${AIC_HAL_DIR}/cmu/aic_hal_fixed_rate_clk.c
    ${AIC_HAL_DIR}/cmu/aic_hal_pll_clk.c
    ${AIC_HAL_DIR}/cmu/v1/aic_hal_cpu_clk.c
    ${AIC_HAL_DIR}/cmu/v1/aic_hal_disp_clk.c
    ${AIC_HAL_DIR}/cmu/v1/aic_hal_fixed_parent_clk.c
    ${AIC_HAL_DIR}/cmu/v1/aic_hal_multi_parent_clk.c
    ${AIC_HAL_DIR}/cmu/v1/aic_hal_reset.c
    ${AIC_HAL_DIR}/gpio/aic_hal_gpio.c
    ${AIC_HAL_DIR}/uart/aic_hal_uart.c)

if(CONFIG_D13X_I2C)
  list(APPEND SRCS d13x_i2c.c)
endif()

if(CONFIG_D13X_PWM1)
  list(APPEND SRCS d13x_pwm.c)
endif()

if(CONFIG_D13X_WDT)
  list(APPEND SRCS
       ${AIC_HAL_DIR}/wdt/hal_wdt.c
       ${AIC_DRV_DIR}/wdt/aic_drv_wdt.c)
endif()

if(CONFIG_D13X_RTC)
  list(APPEND SRCS
       d13x_rtc.c
       ${AIC_HAL_DIR}/rtc/hal_rtc.c)
endif()

if(CONFIG_D13X_SDMC1)
  list(APPEND SRCS
       d13x_sdmc.c
       ${AIC_HAL_DIR}/sdmc/bouncebuf.c
       ${AIC_HAL_DIR}/sdmc/hal_sdmc.c
       ${AIC_DRV_DIR}/sdmc/aic_sdio.c)
endif()

if(CONFIG_AIC_USING_AUDIO)
  list(APPEND SRCS
       ${AIC_HAL_DIR}/audio/hal_audio.c
       ${AIC_DRV_DIR}/audio/aic_drv_audio.c)
endif()

if(CONFIG_AIC_USING_DMA)
  list(APPEND SRCS
       ${AIC_HAL_DIR}/dma/hal_dma.c
       ${AIC_DRV_DIR}/dma/aic_drv_dma.c)
endif()

if(CONFIG_AIC_AUDIO_DMIC)
  list(APPEND SRCS d13x_dmic.c)
endif()

if(CONFIG_D13X_DISPLAY)
  list(APPEND SRCS d13x_fb.c d13x_de.c d13x_lvds.c d13x_panel.c)
endif()

if(CONFIG_DRIVER_QSPI_HAL_EN)
  list(APPEND SRCS
       ${AIC_HAL_DIR}/qspi/qspi_internal.c)

  if(CONFIG_AIC_QSPI_DRV_V11 OR CONFIG_AIC_QSPI_DRV_V12)
    list(APPEND SRCS ${AIC_HAL_DIR}/qspi/hal_qspi_v1x.c)
  else()
    list(APPEND SRCS ${AIC_HAL_DIR}/qspi/hal_qspi_v2x.c)
  endif()
endif()

if(CONFIG_USING_MTD)
  list(APPEND SRCS
       ${AIC_DRV_DIR}/mtd/mtdcore.c
       ${AIC_DRV_DIR}/mtd/mtdparts.c)
endif()

if(CONFIG_AIC_SPINOR_DRV)
  list(APPEND SRCS
       d13x_spinor.c
       ${AIC_DRV_DIR}/spinor/spinor_flash.c
       ${AIC_DRV_DIR}/spinor/partition.c
       ${AIC_DRV_DIR}/spinor/aic_flash_xip_def.c)
endif()

if(CONFIG_USING_SFUD)
  list(APPEND SRCS
       ${AIC_DRV_DIR}/sfud/sfud.c
       ${AIC_DRV_DIR}/sfud/sfud_sfdp.c
       ${AIC_DRV_DIR}/sfud/sfud_qe.c
       ${AIC_DRV_DIR}/sfud/sfud_private.c)
endif()

target_sources(arch PRIVATE ${SRCS})

if(CONFIG_AIC_USING_DMA)
  target_compile_options(
    arch PRIVATE -include
                 ${CMAKE_CURRENT_LIST_DIR}/include/d13x_dma_compat.h)
endif()

if(CONFIG_ARCH_RV_ISA_ZICSR_ZIFENCEI)
  if(CONFIG_ARCH_FPU OR CONFIG_ARCH_DPFPU)
    set(D13X_MARCH -march=rv32imafdc_zicsr_zifencei)
  else()
    set(D13X_MARCH -march=rv32imac_zicsr_zifencei)
  endif()

  set_property(TARGET nuttx APPEND PROPERTY NUTTX_COMPILE_OPTIONS
                                            ${D13X_MARCH})
  if(TARGET nuttx_apps_interface)
    set_property(TARGET nuttx_apps_interface APPEND PROPERTY
                 APPS_COMPILE_OPTIONS ${D13X_MARCH})
  endif()
endif()

target_include_directories(
  arch
  PRIVATE ${NUTTX_CHIP_ABS_DIR}
          ${NUTTX_CHIP_ABS_DIR}/include
          ${AIC_VENDOR_DIR}
          ${AIC_VENDOR_DIR}/include
          ${AIC_VENDOR_DIR}/include/drv
          ${AIC_VENDOR_DIR}/include/drv/inc
          ${AIC_VENDOR_DIR}/include/hal
          ${AIC_VENDOR_DIR}/include/uapi
          ${AIC_VENDOR_DIR}/include/peripheral
          ${AIC_COMMON_DIR}/include
          ${AIC_COMMON_DIR}
          ${AIC_HAL_DIR}
          ${AIC_HAL_DIR}/cmu
          ${AIC_HAL_DIR}/qspi
          ${AIC_HAL_DIR}/uart
          ${AIC_HAL_DIR}/gpio
          ${AIC_HAL_DIR}/audio
          ${AIC_HAL_DIR}/dma
          ${AIC_HAL_DIR}/rtc
          ${AIC_HAL_DIR}/sdmc
          ${AIC_HAL_DIR}/wdt
          ${AIC_DRV_DIR}
          ${AIC_DRV_DIR}/audio
          ${AIC_DRV_DIR}/dma
          ${AIC_DRV_DIR}/wdt
          ${AIC_DRV_DIR}/qspi
          ${AIC_DRV_DIR}/spi
          ${AIC_DRV_DIR}/mtd
          ${AIC_DRV_DIR}/spinor
          ${AIC_DRV_DIR}/sfud
          ${AIC_DRV_DIR}/sdmc
          ${AIC_D12X_DIR})

# Work around missing zicsr/zifencei propagation in the current cmake flow.
# Keep the override local to the custom D13x chip target.
if(CONFIG_ARCH_RV_ISA_ZICSR_ZIFENCEI)
  if(CONFIG_ARCH_FPU OR CONFIG_ARCH_DPFPU)
    target_compile_options(arch PRIVATE -march=rv32imafdc_zicsr_zifencei)
  else()
    target_compile_options(arch PRIVATE -march=rv32imac_zicsr_zifencei)
  endif()
endif()

if(CONFIG_D13X_RTC)
  target_compile_definitions(arch PRIVATE AIC_RTC_DRV_V11)
endif()
