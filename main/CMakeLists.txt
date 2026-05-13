idf_component_register(SRCS "main.c" 
                            "udp_communication.c" 
                            "wifi_handler.c" 
                            "app_protocol.c"
                            "message.pb.c"
                            "led.c"
                            "pingpong/pingpong.c"     
                            "pretty_good_protocol/pretty_good_protocol.c"     
                            "zigbee_router/zigbee_router.c"     
                       INCLUDE_DIRS "." 
                                    "pingpong"        
                                    "pretty_good_protocol"       
                                    "zigbee_router"       
                       REQUIRES 
                            ieee802154
                            lwip
                            log
                            driver
                            nvs_flash
                            esp_wifi
                            esp_event
                            esp_netif
                            nikas-belogolov__nanopb
                            json
                            led_strip
)