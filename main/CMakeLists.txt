idf_component_register(SRCS "main.c" 
                            "udp_communication.c" 
                            "wifi_handler.c" 
                            "app_protocol.c"
                            "message.pb.c"
                            "led.c"
                       INCLUDE_DIRS "."
                       REQUIRES 
                            ieee802154
                            lwip            # For sys/socket.h, lwip/err.h
                            log             # For esp_log.h
                            driver          # For driver/ledc.h, driver/gpio.h
                            nvs_flash       # For nvs_flash.h
                            esp_wifi        # For wifi_handler logic
                            esp_event       # For event loops
                            esp_netif       # For TCP/IP interface
                            nikas-belogolov__nanopb # NanoPB for protobuf
                            json           # For JSON encoding/decoding
                            driver
                            led_strip
)