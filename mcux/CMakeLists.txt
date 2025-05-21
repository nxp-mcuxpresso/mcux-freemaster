# FreeMASTER driver common code
if (CONFIG_MCUX_COMPONENT_middleware.fmstr)
    mcux_add_source(
        SOURCES
            ../src/common/freemaster.h
            ../src/common/freemaster_appcmd.c
            ../src/common/freemaster_defcfg.h
            ../src/common/freemaster_pipes.c
            ../src/common/freemaster_private.h
            ../src/common/freemaster_protocol.c
            ../src/common/freemaster_protocol.h
            ../src/common/freemaster_rec.c
            ../src/common/freemaster_rec.h
            ../src/common/freemaster_scope.c
            ../src/common/freemaster_tsa.c
            ../src/common/freemaster_tsa.h
            ../src/common/freemaster_ures.c
            ../src/common/freemaster_ures.h
            ../src/common/freemaster_utils.c
            ../src/common/freemaster_utils.h
            ../src/common/freemaster_can.c
            ../src/common/freemaster_can.h
            ../src/common/freemaster_sha.c
            ../src/common/freemaster_pdbdm.c
            ../src/common/freemaster_serial.c
            ../src/common/freemaster_serial.h
            ../src/common/freemaster_net.c
            ../src/common/freemaster_net.h
    )
    mcux_add_include(
        INCLUDES
            ../src/common
    )
endif()

# FreeMASTER driver code for 32bit platform
if (CONFIG_MCUX_COMPONENT_middleware.fmstr.platform_gen32le)
    mcux_add_source(
        SOURCES
            ../src/platforms/gen32le/freemaster_gen32le.h
            ../src/drivers/mcuxsdk/can/freemaster_flexcan.c
            ../src/drivers/mcuxsdk/can/freemaster_flexcanfd.h
            ../src/drivers/mcuxsdk/can/freemaster_flexcanfd.c
            ../src/drivers/mcuxsdk/can/freemaster_flexcan.h
            ../src/drivers/mcuxsdk/can/freemaster_mscan.c
            ../src/drivers/mcuxsdk/can/freemaster_mscan.h
            ../src/drivers/mcuxsdk/can/freemaster_mcan.c
            ../src/drivers/mcuxsdk/can/freemaster_mcan.h
            ../src/drivers/mcuxsdk/serial/freemaster_serial_uart.c
            ../src/drivers/mcuxsdk/serial/freemaster_serial_uart.h
            ../src/drivers/mcuxsdk/serial/freemaster_serial_lpsci.c
            ../src/drivers/mcuxsdk/serial/freemaster_serial_lpsci.h
            ../src/drivers/mcuxsdk/serial/freemaster_serial_lpuart.c
            ../src/drivers/mcuxsdk/serial/freemaster_serial_lpuart.h
            ../src/drivers/mcuxsdk/serial/freemaster_serial_usart.c
            ../src/drivers/mcuxsdk/serial/freemaster_serial_usart.h
            ../src/drivers/mcuxsdk/serial/freemaster_serial_miniusart.c
            ../src/drivers/mcuxsdk/serial/freemaster_serial_miniusart.h
            ../src/drivers/mcuxsdk/serial/freemaster_serial_usb.c
            ../src/drivers/mcuxsdk/serial/freemaster_serial_usb.h
            ../src/drivers/mcuxsdk/network/freemaster_net_segger_rtt.c
            ../src/drivers/mcuxsdk/network/freemaster_net_segger_rtt.h
            ../src/drivers/mcuxsdk/network/freemaster_net_lwip_udp.c
            ../src/drivers/mcuxsdk/network/freemaster_net_lwip_udp.h
            ../src/drivers/mcuxsdk/network/freemaster_net_lwip_tcp.c
            ../src/drivers/mcuxsdk/network/freemaster_net_lwip_tcp.h
    )
    mcux_add_include(
        INCLUDES
            ../src/platforms/gen32le
            ../src/drivers/mcuxsdk/can
            ../src/drivers/mcuxsdk/serial
    )

    # Configuration for 32bit platform
    mcux_add_include(
        INCLUDES ../src/template/gen32le
        TARGET_FILES freemaster_cfg.h
    )
    mcux_add_source(
        SOURCES ../src/template/gen32le/freemaster_cfg.h
        CONFIG TRUE
    )
endif()

# FreeMASTER driver code for DSC platform
if (CONFIG_MCUX_COMPONENT_middleware.fmstr.platform_56f800e)
    mcux_add_source(
        SOURCES
            ../src/platforms/56f800e/freemaster_56f800e.h
            ../src/drivers/mcuxsdk/can/freemaster_flexcan_dsc.c
            ../src/drivers/mcuxsdk/can/freemaster_flexcan_dsc.h
            ../src/drivers/mcuxsdk/can/freemaster_mscan_dsc.c
            ../src/drivers/mcuxsdk/can/freemaster_mscan_dsc.h
            ../src/drivers/mcuxsdk/serial/freemaster_serial_qsci.c
            ../src/drivers/mcuxsdk/serial/freemaster_serial_qsci.h
            ../src/drivers/mcuxsdk/serial/freemaster_56f800e_eonce.c
            ../src/drivers/mcuxsdk/serial/freemaster_56f800e_eonce.h
            ../src/drivers/mcuxsdk/serial/freemaster_serial_usb.c
            ../src/drivers/mcuxsdk/serial/freemaster_serial_usb.h
    )
    mcux_add_include(
        INCLUDES
            ../src/platforms/56f800e
            ../src/drivers/mcuxsdk/can
            ../src/drivers/mcuxsdk/serial
    )

    # Configuration for DSC platform
    mcux_add_include(
        INCLUDES ../src/template/56f800e
        TARGET_FILES freemaster_cfg.h
    )
    mcux_add_source(
        SOURCES ../src/template/56f800e/freemaster_cfg.h
        CONFIG TRUE
    )
endif()

# FreeMASTER driver code for S32 platform
if (CONFIG_MCUX_COMPONENT_middleware.fmstr.platform_s32)
    mcux_add_source(
        SOURCES
            ../src/platforms/gen32le/freemaster_gen32le.h
    )
    mcux_add_include(
        INCLUDES
            ../src/platforms/gen32le
    )
endif()

# FreeMASTER driver code for Power Architecture platform
if (CONFIG_MCUX_COMPONENT_middleware.fmstr.platform_pa32)
    mcux_add_source(
        SOURCES
            ../src/platforms/gen32be/freemaster_gen32be.h
    )
    mcux_add_include(
        INCLUDES
            ../src/platforms/gen32be
    )
endif()

# FreeMASTER driver code for S12Z platform
if (CONFIG_MCUX_COMPONENT_middleware.fmstr.platform_s12z)
    mcux_add_source(
        SOURCES
            ../src/platforms/s12z/freemaster_s12z.h
    )
    mcux_add_include(
        INCLUDES
            ../src/platforms/s12z
    )
endif()

# Needs to be defined to satisfy project dependency
if (CONFIG_MCUX_PRJSEG_middleware.fmstr.generated_config)
endif ()
