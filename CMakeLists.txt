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