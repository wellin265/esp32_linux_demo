# 4G 路由器 Web 管理模块编译配置
#
# 启用 Web 路由器时编译所有源文件（含 HTTP 认证、NVS 存储、STA 管理、Web 路由）
# 未启用时仅编译 app_main.c（排除 Web 管理相关文件）

if(CONFIG_EXAMPLE_ENABLE_WEB_ROUTER)
    idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS ".")
    spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
else()
    idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "."
                    EXCLUDE_SRCS
                        "modem_http_config.c"
                        "modem_http_config.h"
                        "http_auth.c"
                        "http_auth.h"
                        "nvs_storage.c"
                        "nvs_storage.h"
                        "sta_manager.c"
                        "sta_manager.h"
                        "http_helper.c"
                        "http_helper.h")
endif()
