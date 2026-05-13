idf_component_register(SRCS "main.c" 
                            "udp_communication.c" 
                            "wifi_handler.c" 
                            "app_protocol.c"
                            "message.pb.c"
                            "led.c"
                            "pingpong/pingpong.c"     # 1. Add the path to the source file
                       INCLUDE_DIRS "." 
                                    "pingpong"        # 2. Add the directory for the header
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