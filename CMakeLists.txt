if(CONFIG_SHIELD_TORABO_TSUKI_LP_LEFT OR CONFIG_SHIELD_TORABO_TSUKI_LP_RIGHT)
  zephyr_library()
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
  zephyr_library_sources(src/board.c src/mini_trackpad_init_reg.c)
endif()

# DYA Studio対応: zmk-feature-device-info向けのzephyr/version.h互換シム
# （include/zephyr/version.h）をグローバルなインクルードパスとして追加する。
# 上のzephyr_include_directories()はzephyr_library()呼び出し後のブロック内
# のためこのライブラリにしかスコープされず、他モジュール(zmk-feature-device-info)
# のソースからは見えなかった（詳細はmemos/dya-studio-integration.md）。
zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

# DYA Studio対応: zmk-feature-device-infoはCONFIG_BOARDを文字列マクロとして
# 直接参照するが、bmp_boostボードは（新しいhardware model v2ではなく）
# 旧来のKconfig.board形式（CONFIG_BOARD_BMP_BOOSTという真偽値）で定義されて
# おり、CONFIG_BOARD文字列マクロ自体が生成されない。CMakeのBOARD変数から
# 文字列マクロとして補う（詳細はmemos/dya-studio-integration.md）。
zephyr_compile_definitions(CONFIG_BOARD="${BOARD}")