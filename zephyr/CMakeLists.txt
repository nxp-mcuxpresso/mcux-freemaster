# FreeMASTER driver common code
if (CONFIG_USE_FREEMASTER)

zephyr_library()

zephyr_library_sources(
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
        ../src/platforms/zephyr/freemaster_zephyr.h
        ../src/platforms/zephyr/freemaster_zephyr.c
)

zephyr_include_directories(
        ../src/common
        ../src/platforms/zephyr
        ../src/drivers/zephyr
)

if (CONFIG_FMSTR_SERIAL_ZEPHYR_UART OR CONFIG_FMSTR_SERIAL_ZEPHYR_USB)
    zephyr_library_sources(
        ../src/drivers/zephyr/freemaster_zephyr_serial.h
        ../src/drivers/zephyr/freemaster_zephyr_serial.c
    )
endif()

if (CONFIG_FMSTR_CAN_ZEPHYR_GENERIC)
    zephyr_library_sources(
        ../src/drivers/zephyr/freemaster_zephyr_can.h
        ../src/drivers/zephyr/freemaster_zephyr_can.c
    )
endif()

if (CONFIG_FMSTR_SEGGER_RTT_ZEPHYR)
    zephyr_library_sources(
        ../src/drivers/zephyr/freemaster_zephyr_rtt.h
        ../src/drivers/zephyr/freemaster_zephyr_rtt.c
    )
endif()

if (CONFIG_FMSTR_NET_ZEPHYR_UDP)
    zephyr_library_sources(
        ../src/drivers/zephyr/freemaster_zephyr_net_udp.h
        ../src/drivers/zephyr/freemaster_zephyr_net_udp.c
    )
endif()

if (CONFIG_FMSTR_NET_ZEPHYR_TCP)
    zephyr_library_sources(
        ../src/drivers/zephyr/freemaster_zephyr_net_tcp.h
        ../src/drivers/zephyr/freemaster_zephyr_net_tcp.c
    )
endif()

if (CONFIG_FMSTR_USE_PIPE_SHELL)
    zephyr_library_sources(
        ../src/platforms/zephyr/freemaster_shell.c
        ../src/platforms/zephyr/freemaster_shell.h
    )
endif()

if (CONFIG_FMSTR_USE_TSA)
    zephyr_linker_sources(SECTIONS tsa-tables.ld)
endif()

endif() # CONFIG_USE_FREEMASTER
