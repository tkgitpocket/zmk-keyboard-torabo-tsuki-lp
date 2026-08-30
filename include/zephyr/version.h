/*
 * DYA Studio対応: zmk-feature-device-info互換シム
 *
 * zmk-feature-device-info（src/device_info_collect.c 等）は
 * #include <zephyr/version.h> を使うが、このキーボードが使うZephyr
 * v3.5.0+zmk-fixes はビルド時に include/generated/version.h
 * （zephyr/ サブディレクトリなし）を生成する仕様であり、
 * <zephyr/version.h> というパス自体が存在しない
 * （zephyr/ プレフィックス付きの生成ヘッダーは後のZephyrリリースで
 * 導入された規約）。
 *
 * このファイルはCMakeLists.txtの zephyr_include_directories(...)/include
 * 経由で通常の <version.h> より優先して見つかる位置に置き、実体である
 * 生成済み <version.h> に単純に転送するだけの互換シム。
 * KERNEL_VERSION_* 等のマクロ定義自体はZephyrバージョンを問わず
 * 同一形式のため、転送するだけで問題なく動作する。
 */
#pragma once

#include <version.h>
