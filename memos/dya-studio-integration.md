# DYA Studio 対応メモ

> 作成: 2026-08-29
> 状態: 作業中（`dya-studio-level2` ブランチ、段階的にpush→CI確認しながら進行中）
> 作業ブランチ: `dya-studio-level2`（`master` から分岐。問題なければ `master` にマージする方針）

---

## 背景・目的

[DYA Studio](https://studio.dya.cormoran.works/)（cormoran氏が開発するZMK Studio互換のWeb UI）に対応させる。

参考資料:

- [DYA Studio 開発者ガイド](https://studio.dya.cormoran.works/developer-guide)（SPAのためWebFetchでは内容取得できず、実際の対応はGitHubリポジトリの実例から調査した）
- [note: DYA Studioの仕組み](https://note.com/cormoran/n/n888c547fc99b)
- [note: Custom Studio Protocolについて](https://note.com/cormoran/n/n08cbee00ea81)
- [cormoran/dya-studio](https://github.com/cormoran/dya-studio)
- [cormoran/zmk-keyboard-dya2](https://github.com/cormoran/zmk-keyboard-dya2)（実機のconfig/west-dependency.ymlを参考にした）
- [cormoran/zmk-module-runtime-input-processor](https://github.com/cormoran/zmk-module-runtime-input-processor)
- [cormoran/zmk-module-ble-management](https://github.com/cormoran/zmk-module-ble-management)
- [cormoran/zmk-feature-custom-settings](https://github.com/cormoran/zmk-feature-custom-settings)
- [cormoran/zmk-feature-default-layer](https://github.com/cormoran/zmk-feature-default-layer)（`codex/custom-rpc-rewrite` ブランチ）

## DYA Studioの対応レベル

DYA Studio開発者(cormoran氏)の定義による3段階:

| レベル | 内容 | 必要なもの |
| --- | --- | --- |
| Level 1 | キー割り当て・レイヤー名・物理レイアウト切替 | 標準ZMK Studioが有効なら動く |
| Level 2 | マクロ/コンボ編集・トラックボール設定・BLEプロファイル管理など | cormoran氏のZMKフォーク + Custom Studio Protocol対応モジュール |
| Level 3 | ハードウェア固有のカスタム設定画面 | 独自プロトコルモジュール |

**Level 1は本リポジトリで対応作業前から実質的に満たされていた**（`CONFIG_ZMK_STUDIO=y`、`CONFIG_ZMK_STUDIO_LOCKING=n`、`studio-rpc-usb-uart`スニペット、`keys`付き物理レイアウト定義が既に揃っていたため）。今回はLevel 2（トラックボール調整・BLEプロファイル管理・per-OSデフォルトレイヤー）まで対応する。

## Level 2で追加するモジュールと方針

### ZMKフォーク切り替え

`config/west.yml` の `zmk` プロジェクトを、公式 `zmkfirmware/zmk` の `v0.3` タグから、cormoran氏のフォーク `cormoran/zmk` の `v0.3-branch+custom-studio-protocol+ble` ブランチに切り替えた。

**選定理由・リスク評価:**

- このブランチは公式 `v0.3` タグに対して26コミット・27ファイル程度の差分で、内容はほぼ全て `#if IS_ENABLED(...)` で守られた**追加的（additive）パッチ**（`app/src/studio/*` の新規サブシステム、`app/src/ble.c`・`app/src/split/bluetooth/central.c`・`app/src/split/central.c` へのリレーイベント機構の追加等）。既存動作を変更する破壊的差分は無いことをdiffで直接確認済み。
- Zephyrのバージョンは公式v0.3と全く同じ `v3.5.0+zmk-fixes`。DYA本体（`zmk-keyboard-dya2`等）が使う `main+dya` ブランチは独自Zephyrフォーク（`v4.1.0+zmk-fixes+nrf-half-duplex-uart`、有線分割専用）まで要求するため、それに比べて遥かに変更範囲が小さい。今回は**あえて `main+dya` ではなくv0.3ベースの軽量フォークを選んだ**。
- `src/board.c`（独自のBLE分割電源管理、`bt_conn_le_param_update`を直接操作）が使うZephyr標準API層は今回の差分に含まれておらず、影響を受けない見込み。
- `zmk-module-ble-management` のREADMEが名指しでこのブランチ（`v0.3-branch+custom-studio-protocol+ble`）を要求しており、選定の妥当性を裏付けている。

### 追加予定モジュール（進行に応じて追記）

| モジュール | 用途 | ブランチ |
| --- | --- | --- |
| `zmk-feature-custom-settings` | 各種カスタム設定の永続化バックエンド（他モジュールの依存先） | `main`相当（実際はZephyr v3.5.0互換のコミットに固定、後述） |
| ~~`zmk-module-runtime-input-processor`~~ | ~~トラックボール速度・回転・軸反転等をWeb UIから実行時調整~~ → **断念**（ZMKコアAPI非互換、後述） | - |
| `zmk-module-ble-management` | BLEプロファイルの一覧表示・名前付け・切替・ペア解除をWeb UIから | `main` |
| `zmk-feature-default-layer` | per-OS（Windows/macOS/iOS等）デフォルトレイヤーの自動切替。既存の `&to 0`/`&to 1` マクロ方式を置き換え | `codex/custom-rpc-rewrite`（Studio RPC対応版） |
| `zmk-feature-os-detection` | `zmk-feature-default-layer`のper-OS機能が依存 | `main` |

### 進行方針

ローカルビルド環境が無くGitHub Actions頼みのため、変更を小さく分けてpush→CI確認を繰り返す：

1. **ZMKフォーク切り替えのみ**（他は変更なし）→ 既存ドライバ・モジュールがそのままビルドできるか確認。
2. トラックボール調整（`zmk-feature-custom-settings` + `zmk-module-runtime-input-processor`）を追加。
3. BLEプロファイル管理（`zmk-module-ble-management`）を追加。
4. per-OSデフォルトレイヤー（`zmk-feature-default-layer` + `zmk-feature-os-detection`）を追加し、既存のBTプロファイル別デフォルトレイヤー切替マクロ（`out_bt_0`〜`out_bt_4`内の`&to 0`/`&to 1`）を置き換えるか、共存させるか検討。

## 進捗ログ

### 2026-08-29: Step 1 — ZMKフォーク切り替え

`config/west.yml` の `zmk` を `cormoran/zmk` の `v0.3-branch+custom-studio-protocol+ble` に変更してpush。

**結果: ✅ CIビルド成功**（[run #41](https://github.com/tkgitpocket/zmk-keyboard-torabo-tsuki-lp/actions)）。既存のPAW3222トラックボール・IQS7211Eトラックパッド・BMP Boost・zmk-scroll-snap・zmk-layout-shift等、他社製モジュールを含めて問題なくビルドが通ることを確認。事前のdiff調査（追加的パッチのみ、Zephyrバージョン同一）通りの結果。

### 2026-08-29: Step 2 — トラックボール/トラックパッド速度調整（zmk-module-runtime-input-processor）

`config/west.yml` に `zmk-feature-custom-settings`・`zmk-module-runtime-input-processor` を追加。`torabo_tsuki_lp_right.conf` に `CONFIG_ZMK_POINTING=y`・`CONFIG_ZMK_RUNTIME_INPUT_PROCESSOR=y`・`CONFIG_ZMK_RUNTIME_INPUT_PROCESSOR_STUDIO_RPC=y`・`CONFIG_ZMK_LOW_PRIORITY_THREAD_STACK_SIZE=2048` を追加。

**設計判断:**

- モジュール付属の既定ノード（`mouse_runtime_input_processor`）はトラックボール・トラックパッドどちらの構成でも単一の共有インスタンスになってしまい、片方を調整すると両方の速度が一緒に動いてしまう。これを避けるため、`processor-label`を分けた**独自インスタンスを2つ**定義した：
  - `trackball_speed_rip`（`torabo_tsuki_lp_right.overlay` で定義） — 右トラックボール(`&pointing_listener`)の通常速度チェーン末尾に追加。
  - `trackpad_speed_rip`（`snippets/input-split-listener/input-split-listener.overlay` で定義） — 左トラックパッド(ミニ)のイベントが中継される`&pointing_device_split_listener`のチェーン末尾に追加。
- どちらも既定値 `scale-multiplier=1 / scale-divisor=1`（補正なし）としたため、**既存の速度設定（トラックボール1/2、SLOWTRACKBALL_L 1/6等）は一切変更していない**。DYA StudioのWeb UIから追加の倍率をかけられる「トリム」として追加しただけ。
- スクロール系（SCROLL_L/SLOWSCROLL_L/FASTRSCROLL_L のscroll-snapチェーン）とSLOWTRACKBALL_Lは今回は対象外とし、既存のレイヤー切り替えベースの調整をそのまま残した（変更範囲を絞ってリスクを抑える判断）。
- `&label { ... }` によるノード参照はビルドシステムのマージ順序に依存する可能性を考慮し、ノード定義と参照は同一ファイル内に収めるようにした（トラックパッド用ノードを`torabo_tsuki_lp_right.overlay`ではなくスニペット自体に置いたのはこのため）。

**結果: ❌ CIビルド失敗 → 原因特定・回避して再push**

[run](https://github.com/tkgitpocket/zmk-keyboard-torabo-tsuki-lp/actions)がleft/right両方とも失敗。ログを確認したところ、原因は自分の変更ではなく **`zmk-feature-custom-settings` の `main` ブランチHEAD自体のバグ** だった：

```
/tmp/zmk-config/zmk-feature-custom-settings/Kconfig:16: error: couldn't parse 'configdefault MAIN_STACK_SIZE': syntax error
```

該当箇所は本来 `config MAIN_STACK_SIZE` / `default 2048` と2行であるべきところが `configdefault MAIN_STACK_SIZE` と1行に結合された誤字（コミット `76fa47e` "Raise MAIN_STACK_SIZE default to 2048"で混入、2026-08-29時点のmain HEADでも未修正）。Kconfigはファイル全体を構文解析してから条件評価するため、`CONFIG_ZMK_CUSTOM_SETTINGS` を有効化していないビルド（左peripheral）even含めて**モジュールがwest workspaceに存在するだけで全ターゲットがビルド不能**になっていた。

**対応:** `config/west.yml` の `zmk-feature-custom-settings` を `main` からこのバグ混入直前のコミット `56ad4260388486e513f07c45a6ab1a404e3e7878` に固定。あわせて、このコミットが未収録の「起動時スタックオーバーフロー対策(MAIN_STACK_SIZEを1024→2048に)」を自前で `torabo_tsuki_lp_right.conf` に `CONFIG_MAIN_STACK_SIZE=2048` として追加。

**2件目の失敗と対応:** 修正後の再pushではleftは成功したが、rightが `fatal error: dt-bindings/zmk/input.h: No such file or directory` で失敗。zmk-module-runtime-input-processorのREADME記載の `#include <dt-bindings/zmk/input.h>` は誤りで、実際にモジュール自身のテストconfig（`tests/zmk-config/boards/shields/my_awesome_keyboard/my_awesome_keyboard.overlay`）を確認したところ正しいincludeは `<zephyr/dt-bindings/input/input-event-codes.h>` だった。`torabo_tsuki_lp_right.overlay`・`input-split-listener.overlay` 両方のincludeを修正。

**3件目の失敗と対応（より根本的な非互換性）:** さらに再pushしたところ、rightが `fatal_error: Must choose valid location for linker snippet.`（`zmk-feature-custom-settings/CMakeLists.txt:5 (zephyr_linker_sources)`）で失敗。原因を調査したところ、`zmk-feature-custom-settings` は内部で `zephyr_linker_sources(ROM_SECTIONS ...)` を呼んでいるが、**`ROM_SECTIONS` という配置先はこのキーボードが使うZephyr `v3.5.0+zmk-fixes`（公式v0.3系）の `extensions.cmake` には存在しない**（有効なのは `SECTIONS`/`RAM_SECTIONS`/`DATA_SECTIONS`/`ROM_START`/`NOINIT`/`RWDATA`/`RODATA`/`RAMFUNC_SECTION`/`NOCACHE_SECTION`/`PINNED_*` のみ）。`ROM_SECTIONS` はこのモジュールの「Simplification phase 4」というリファクタでのちに追加された比較的新しいZephyr機能に依存しており、Step 1で確認した「ZMKコア自体の追加的パッチ」とは別の、**依存モジュール側がより新しいZephyrを前提にしている**という構造的な非互換性だった。

対応として、`ROM_SECTIONS` 導入前のコミット `5be86dcfb903ecc12624b02cadb40bbdbc78abe3`（`DATA_SECTIONS` のみ使用）に再固定。ソース一式を確認し、他に新しいZephyr専用APIが無いことも確認した。この時点のコードはKconfigの誤字（`configdefault`）も混入前で、`ROM_SECTIONS`より前・`configdefault`バグより前の、両方の問題を回避できる地点。

**4件目の失敗と方針転換: zmk-module-runtime-input-processorを断念**

`zmk-feature-custom-settings` のZephyrバージョン問題を回避した後、今度は `zmk-module-runtime-input-processor` 自体のソース（`src/pointing/input_processor_runtime.c:173`）で `error: too many arguments to function 'zmk_keymap_layer_activate'` が発生。調べたところ、このモジュールは temp-layer機能（ポインティングデバイス操作時に一時的にレイヤーを起動する機能）で `zmk_keymap_layer_activate(layer, false)` と2引数で呼んでいるが、**このキーボードが使うZMKフォーク（`v0.3-branch+custom-studio-protocol+ble`、公式v0.3系）の `zmk_keymap_layer_activate` は1引数しか受け取らない**（`app/include/zmk/keymap.h`で確認）。2引数版はZMK本家の `main` ブランチで後から追加されたシグネチャと考えられる。

この呼び出しはtemp-layer機能を使うか否かに関わらず、`CONFIG_ZMK_RUNTIME_INPUT_PROCESSOR=y` にするとファイル全体がコンパイルされるため、devicetree側の設定では回避不能。

これは「ZMKコア自体の追加パッチ」の問題ではなく、**モジュール本体がZMK本家`main`ブランチ（DYA本体が使う`main+dya`相当）の新しいコアAPIを前提に書かれている**ことによる非互換性で、`zmk-feature-custom-settings`のROM_SECTIONS問題と同種の「依存モジュールが新しいZMK/Zephyrを前提にしている」パターンの2件目。過去コミットを遡って回避する対応も検討したが、この機能が使う `zmk_keymap_layer_activate` の引数追加がいつのモジュールコミットから入ったか特定して古いコミットまで遡る調査は際限がなく、遡っても数ヶ月分の改善・バグ修正を失うことになるため、**費用対効果が見合わないと判断し、このモジュール（トラックボール/トラックパッド速度のWeb UI調整）は今回のLevel 2対応から見送ることにした**。

`config/west.yml` から `zmk-module-runtime-input-processor` を削除し、`torabo_tsuki_lp_right.overlay`・`snippets/input-split-listener/input-split-listener.overlay`・`torabo_tsuki_lp_right.conf` に加えた関連の変更を取り消した。`zmk-feature-custom-settings` はビルドが通ることを確認済みで、後続のper-OSデフォルトレイヤー機能が依存するため west.yml に残している。

### 2026-08-29: Step 3 — BLEプロファイル管理（zmk-module-ble-management）

`config/west.yml` に `zmk-module-ble-management` を追加（`zmk-feature-custom-settings` には依存しない、README記載通り）。`torabo_tsuki_lp_right.conf` に `CONFIG_ZMK_BLE_MANAGEMENT=y`・`CONFIG_ZMK_BLE_MANAGEMENT_STUDIO_RPC=y` を追加。デバイスツリーの変更は不要（BLEプロファイルはZMK標準機構をそのまま使うRPCハンドラのため）。右（central）のみ有効化（ホストとのBLE接続を管理するのは central 側のため）。

このモジュールはREADMEで明示的に `zmk: revision: v0.3-branch+custom-studio-protocol+ble` を要求しており、Step 1で選定したフォークとの組み合わせが著者によって想定されている数少ない実例。カスタム名の永続化もZephyr標準settingsを直接使うとのことで、custom-settingsのZephyrバージョン問題の影響を受けない見込み。

**結果: ❌ CIビルド失敗 → 原因特定・回避**

```
/tmp/zmk-config/zmk-module-ble-management/src/studio/ble_management_handler.c:543: undefined reference to `zmk_endpoint_set_preferred_transport'
/tmp/zmk-config/zmk-module-ble-management/src/studio/ble_management_handler.c:564: undefined reference to `zmk_endpoint_get_preferred_transport'
```

`main` ブランチHEADの出力プライオリティ切替（Output Priority Toggle）機能が `zmk_endpoint_get/set_preferred_transport`（単数形 "endpoint"）というZMKコア関数を呼んでいるが、このキーボードが使うZMKフォーク（`v0.3-branch+custom-studio-protocol+ble`）には `zmk_endpoints_get_preferred_transport`・`zmk_endpoints_select_transport`（複数形 "endpoints"）しか存在しない。

コミット履歴を調査したところ、[コミット `659d389`](https://github.com/cormoran/zmk-module-ble-management/commit/659d389f1212fdb4647b432161127e4e436665e9)（2026-02-04、コミットメッセージ "Use zmk_endpoints_get_preferred_transport"）でまさにこの複数形の呼び出しに修正されており、**同じコミットのREADME差分でzmk依存を`v0.3+custom-studio-protocol`から今回選定した`v0.3-branch+custom-studio-protocol+ble`に更新している**——つまり著者自身がこの時点でこのフォークとの組み合わせを検証・修正した形跡そのものだった。しかしその後（2026-04-30以降のどこか）でZMK本家`main`側の関数名が単数形にリネームされたのに追従し、`main`ブランチは単数形呼び出しに戻ってしまっている（README記載の推奨revisionが更新されないまま取り残されている状態）。

対応として、複数形呼び出しのまま残っている最後のコミット `2147ba7d329253ef9d0cfe4c6b814add7915225c`（2026-02-06、次のコミットまで4ヶ月弱の空白期間があり、その間の変更が無いことを確認済み）に固定。

再pushの結果 **✅ CIビルド成功**。BLEプロファイル管理（一覧・名前付け・切替・ペア解除・出力プライオリティ切替）がDYA Studio Level 2機能として動作する状態になった（実機での動作確認は未実施）。

### 2026-08-29: Step 4 — per-OSデフォルトレイヤー（zmk-feature-default-layer）を調査した上で断念

`zmk-feature-default-layer` の `codex/custom-rpc-rewrite`（Studio RPC対応版）ブランチのソースを事前に確認したところ、`src/default_layer.c` が

- `zmk_keymap_layer_activate(layer_id, true)` / `zmk_keymap_layer_deactivate(layer_id, true)` — **2引数**呼び出し（このキーボードのZMKフォークは1引数版のみ、Step 3で断念したruntime-input-processorと全く同じ非互換パターン）
- `zmk_endpoint_get_selected()` — **単数形**の関数名（このキーボードのZMKフォークには存在せず、`zmk_endpoints_selected()`という別名の複数形版のみ存在。BLE管理のケースと同じ非互換パターン）

を使っていることが判明した。`zmk-module-ble-management` のケースでは「複数形呼び出しに直したコミット」まで遡ることで回避できたが、今回は**このブランチが分岐した最初のコミット（`566373a`, 2026-07-04）の時点で既にこれらの呼び出しが存在**しており、ブランチ内に互換性のある地点が無い。これ以上遡るには `codex/custom-rpc-rewrite` ブランチの分岐元まで遡る必要があり、その場合Studio RPC対応（Web UIからの編集機能）自体が失われる可能性が高く、実質的に得るものが無くなる。

CIにpushする前にソースレビューで判明したため、実際のビルド失敗は発生させていない。**per-OSデフォルトレイヤーのDYA Studio対応は今回見送り**、既存の `out_bt_0`〜`out_bt_4` マクロ（`&to 0`/`&to 1`によるBTプロファイル別デフォルトレイヤー切替、[layer-restructure-apple-windows.md](layer-restructure-apple-windows.md)参照）をそのまま維持する。

`zmk-feature-os-detection`（per-OS機能の依存先）は最初から追加していなかったため変更なし。

### 後片付け

`zmk-feature-custom-settings` は、依存していた `zmk-module-runtime-input-processor`（Step 3で断念）・`zmk-feature-default-layer`（Step 4で断念）の両方を見送ったことで**唯一の利用者がいなくなった**（`zmk-module-ble-management` はZephyr標準settingsを直接使うため不要）ため、`config/west.yml` から削除した。あわせて、custom-settings/runtime-input-processor向けに追加していた `CONFIG_ZMK_LOW_PRIORITY_THREAD_STACK_SIZE`・`CONFIG_MAIN_STACK_SIZE` も、必要とするモジュールが無くなったため `torabo_tsuki_lp_right.conf` から削除した。

## 最終結果まとめ（`dya-studio-level2` ブランチ）

| 機能 | 状態 | 備考 |
| --- | --- | --- |
| Level 1（キー割り当て・レイヤー名・物理レイアウト切替） | ✅ 対応済み | 実は対応作業前から既に満たされていた |
| BLEプロファイル管理（Level 2） | ✅ 対応 | `zmk-module-ble-management`、CIビルド成功。実機未検証 |
| トラックボール/トラックパッド速度調整（Level 2） | ❌ 見送り | `zmk-module-runtime-input-processor` がZMK本家mainの新しいコアAPI(`zmk_keymap_layer_activate`の2引数版)に依存しており非互換 |
| per-OSデフォルトレイヤー（Level 2） | ❌ 見送り | `zmk-feature-default-layer`(RPC版)が同様に新しいコアAPIに依存。既存のBTプロファイル別切替マクロを維持 |
| マクロ/コンボ編集（Level 2） | 未着手 | 今回のスコープに含めていない |

**分かったこと（今後の参考）:** cormoran氏のDYA Studio関連モジュール群は、READMEに `v0.3-branch+custom-studio-protocol(+ble)` のようなv0.3ベースの構成例が書かれていても、**実際のmainブランチの中身はZMK本家`main`（DYA本体が使う`main+dya`相当）の最新コアAPIを前提に書かれていることが多く、READMEの記載を鵜呑みにできない**。今回はBLE管理のみ「著者が実際にv0.3系フォークで動作確認・修正した形跡があるコミット」を発掘できたため救えたが、これは運が良かったケース。今後同様の対応をする場合、READMEより先にコミット履歴と実際のコア関数呼び出しを確認したほうが手戻りが少ない。
