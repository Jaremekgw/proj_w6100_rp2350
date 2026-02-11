
# libraries

P

    ### Ethernet
- add_library(ETHERNET_FILES STATIC)
- add_library(W6100_FILES STATIC)

    ### Loopback
- add_library(LOOPBACK_FILES STATIC)

    ### DHCP
- add_library(AAC_FILES STATIC)
- add_library(DHCP_FILES STATIC)
- add_library(DHCP6_FILES STATIC)

    ### DNS
- add_library(DNS_FILES STATIC)

    ### HTTP Server
- add_library(HTTPSERVER_FILES STATIC)

    ### MQTT
- add_library(MQTT_FILES STATIC)

    ### SNTP
- add_library(SNTP_FILES STATIC)

    ### SNMP
- add_library(SNMP_FILES STATIC)

    ### TFTP Server
- add_library(TFTP_FILES STATIC)

    ### multicast
- add_library(MULTI_FILES STATIC)





for ETHERNET_FILES we have:

# Ethernet
add_library(ETHERNET_FILES STATIC)

target_sources(ETHERNET_FILES PRIVATE
        ${WIZNET_DIR}/Ethernet/socket.c
        ${WIZNET_DIR}/Ethernet/wizchip_conf.c
        )

target_include_directories(ETHERNET_FILES SYSTEM PRIVATE
        ${WIZNET_DIR}/Ethernet
        ${WIZNET_DIR}/Ethernet/W6100
        )

target_sources(ETHERNET_FILES PRIVATE
        ${WIZNET_DIR}/Ethernet/W6100/w6100.c
        )

target_include_directories(ETHERNET_FILES SYSTEM PRIVATE
        ${WIZNET_DIR}/Ethernet/W6100
        )


# Proposition
add_library(ETHERNET_FILES STATIC
    socket.c
    wizchip_conf.c
)

if(WIZNET_CHIP STREQUAL W6100)
    add_library(WIZCHIP_FILES STATIC
        W6100/w6100.c
    )
elseif(WIZNET_CHIP STREQUAL W6300)
    add_library(WIZCHIP_FILES STATIC
        W6300/w6300.c
    )
endif()

target_link_libraries(ETHERNET_FILES PUBLIC WIZCHIP_FILES)


