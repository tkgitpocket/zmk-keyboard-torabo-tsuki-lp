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

## 実機接続トラブル（2026-08-30）

`dya-studio-level2` ブランチの最新ビルドを両半体に書き込んで接続を試したところ、**USB・BLEどちらでもデバイス選択後にタイムアウト**する症状が発生。

原因調査のため、それまで参照していなかった実際の開発者ガイド本文（`https://studio.dya.cormoran.works/developer-guide/level-1` / `level-2`、SPAのためWebFetchでは中身が取れず、`cormoran/dya-studio` リポジトリの `src/content/developerGuide.ts` を直接取得して内容を確認した）と、公式サンプルリポジトリ [`cormoran/zmk-config-dya-studio-sample`](https://github.com/cormoran/zmk-config-dya-studio-sample) のチュートリアルPR差分を確認した。分かったこと：

1. **`developer-guide/level-2` ページの「スタックとバッファの推奨設定」を追加していなかった。** Custom Studio Protocol対応モジュール（今回は`zmk-module-ble-management`）を使う場合、以下の設定が推奨されている（未設定だとスタック不足でクラッシュ/無応答になり得ると明記）:
   ```
   CONFIG_ZMK_SPLIT_RELAY_EVENT=y
   CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE=10000
   CONFIG_ZMK_SPLIT_BLE_CENTRAL_SPLIT_RUN_STACK_SIZE=768
   CONFIG_ZMK_LOW_PRIORITY_THREAD_STACK_SIZE=4096
   CONFIG_ZMK_STUDIO_RPC_RX_BUF_SIZE=256
   CONFIG_ZMK_STUDIO_RPC_TX_BUF_SIZE=256
   CONFIG_ZMK_STUDIO_RPC_CUSTOM_SUBSYSTEM_REQUEST_PAYLOAD_MAX_BYTES=256
   CONFIG_ZMK_SPLIT_RELAY_EVENT_DATA_LEN=240
   CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=4096
   CONFIG_ZMK_STUDIO_RPC_THREAD_STACK_SIZE=6000
   ```
   （`CONFIG_ZMK_CUSTOM_SETTINGS_LARGE_VALUE_MAX_SIZE=256` はzmk-feature-custom-settings専用のため、それを使っていない本リポジトリでは除外）。USB・BLE両方でタイムアウトしていたことから、Custom Studio Protocolの基盤（カスタムサブシステム列挙等）がバッファ/スタック不足で応答できていない可能性が高いと判断し、`torabo_tsuki_lp_left.conf`・`torabo_tsuki_lp_right.conf` の両方に追加（`CONFIG_ZMK_SPLIT_RELAY_EVENT`関連は分割キーボードのため両側に必要、[zmk-feature-custom-settings](https://github.com/cormoran/zmk-feature-custom-settings)のREADMEにも同様の記載あり）。CIビルドは成功。**実機再検証は次回のフラッシュ後に確認予定。**

2. **Level 2の公式推奨構成は`main+dya`＋専用Zephyrフォーク（`v4.1.0+zmk-fixes+nrf-half-duplex-uart`）であり、今回選んだ`v0.3-branch+custom-studio-protocol+ble`ではない。** 公式サンプルの[Level 2 PR](https://github.com/cormoran/zmk-config-dya-studio-sample/pull/3)もこの構成を使っている。これまで遭遇した一連のコアAPI非互換（`zmk_keymap_layer_activate`の引数、`zmk_endpoint_*`命名等）は、この「非公式ルート」を選んだことに起因する可能性が高い。

再pushしたバッファ/スタック設定を書き込んだところ、**（何度か試した末に）DYA Studioへの接続自体は成功**した。Keymapタブは表示・編集できる。

ただし新たな問題が2つ判明:

- トラックボールタブで「このキーボードではランタイム入力プロセッサーサブシステムを利用できません。ファームウェアでcormoran/zmk-module-runtime-input-processorが有効になっていることを確認してください」と表示される。これは**想定通り**（Step 3で意図的に見送った機能のため）。
- **実機が不安定になる**（マウスカーソルの動きが遅くなる、キーが全く反応しなくなる）症状が発生。トラブルシューティングページの「キーボードがリセットされたり、フリーズ後に復帰したりする: stack overflowまたは時間のかかる処理によってWatchdog timerが動作した可能性が高い」に該当する可能性が高い。

追加したバッファ/スタック設定のうち、`CONFIG_ZMK_SPLIT_BLE_CENTRAL_SPLIT_RUN_STACK_SIZE`（デフォルト512→768、増加のみ）や各種RPCバッファ（デフォルトより増加のみ）は、値を縮めてしまっている箇所ではないことを確認済み。一方 `CONFIG_ZMK_SPLIT_RELAY_EVENT=y` は、`zmk-module-ble-management`のソース（`ble_management_handler.c`）を確認したところ**一切使用していない**ことが判明した。この設定は分割キーボードの左右間に新しいBLE GATTキャラクタリスティック（`relay_event_subscribe_params`）の購読を追加し、split接続の「完全にsubscribeできた」判定条件にも組み込まれる（`app/src/split/bluetooth/central.c`の`split_central_chrc_discovery_func`参照）。「custom-studio-protocol+ble」という比較的検証の薄い拡張コードパスに、不要な購読処理を追加してしまっていたことになる。**不要かつ疑わしいため`CONFIG_ZMK_SPLIT_RELAY_EVENT`・`CONFIG_ZMK_SPLIT_RELAY_EVENT_DATA_LEN`を削除**し、実機での再検証待ち。

もしこれでも改善しない場合は`main+dya`への切替も選択肢として検討する（ただしZephyr自体の変更を伴うより大きな変更になる）。

**`CONFIG_ZMK_SPLIT_RELAY_EVENT`削除後の再検証結果: ❌ 変化なし。** USB・BLEとも再び接続不可（USBはタイムアウト、BLEはデバイス自体見つからず）に戻り、不安定症状（カーソル遅延・キー無反応）も「あまり変わらない」との報告。さらに重要な点として、**この不安定症状はDYA Studioへの接続を試みていない、普段の使用中にも発生している**ことが判明（日常使用に支障が出るレベル）。ユーザーは別のキーボードで代用しつつ調査継続を希望。

これにより「`CONFIG_ZMK_SPLIT_RELAY_EVENT`が原因」という仮説は外れた。不安定症状が具体的にいつ出始めたかを整理すると:

- Step 3（`zmk-module-ble-management`のみ、バッファ/スタック拡張設定なし）: 接続はタイムアウトしていたが、この時点で普段使用の不安定性は未報告（確認していないだけの可能性もあり不明）。
- バッファ/スタック拡張設定を追加後: 接続は（リトライの末）成功したが、普段使用でも不安定症状が発生。
- `CONFIG_ZMK_SPLIT_RELAY_EVENT`のみ削除後: 接続は再び失敗、不安定症状は継続。

**次の切り分けとして、バッファ/スタック拡張設定を全て削除し、`zmk-module-ble-management`の最小構成（`CONFIG_ZMK_BLE_MANAGEMENT`・`CONFIG_ZMK_BLE_MANAGEMENT_STUDIO_RPC`のみ）に戻したビルドを用意した。** 今回はDYA Studioへの接続可否は一旦度外視し、**普段使用時の安定性のみ**を確認してもらう。これで不安定症状が消えれば拡張設定（スタックサイズ増加によるRAM圧迫等）が原因、消えなければ`zmk-module-ble-management`自体かZMKフォーク本体（`v0.3-branch+custom-studio-protocol+ble`）が原因という切り分けになる。後者の場合、Step 1（フォーク切替のみ、ble-managementなし）まで遡っての検証が次の一手になる。

3. **`&studio_unlock`はユーザー側で既にキーマップに追加済み**（コミット`697edcd`）だった。`CONFIG_ZMK_STUDIO_LOCKING=n`の場合はZMKのソース上は起動時から常にunlocked状態になるため技術的には無くても動作するはずだが、公式サンプルにも明記されている手順であり実害もないため残置。

## 方針転換: torabo-tsuki-lp本家のDYA Studio対応ブランチを参考実装として採用（2026-08-30）

普段使用時の不安定症状について再確認したところ、実は「接続直後の一時的な不安定さ」を「常時不安定」と誤認していたことが判明（再検証で安定動作を確認）。ただしその後、より詳細な検証で **やはり両バージョンとも常用に耐えないレベルの不安定さ（トラックボール反応低下、左側の認識不良）がある** ことが再度報告された。DYA Studio非対応版（`master`）は安定して動作している。

この過程でユーザーから重要な情報提供があった: **torabo-tsuki-lpの本家リポジトリ（sekigon-gonnoc氏、このキーボードのハードウェア設計者）に、非公式ながら実際にDYA Studio対応済みのブランチが存在する**:

- <https://github.com/sekigon-gonnoc/zmk-keyboard-torabo-tsuki-lp/tree/v0.3+dya-studio>
- 参考: cormoran氏自身のキーボード <https://github.com/cormoran/zmk-keyboard-dya-dash>, <https://github.com/cormoran/zmk-keyboard-dya2>（マイコンは異なる）

本家の実装を調査した結果、重要な差分が判明した:

- **ZMKフォークは`v0.3-branch+custom-studio-protocol+ble`ではなく`v0.3-branch+dya`。** 自己判断で選んだ前者は「custom-studio-protocol」機能に絞った狭い拡張だったが、本家が使う`v0.3-branch+dya`は`main+dya`（DYA本体が使う本流ブランチ）の**v0.3版フルバックポート**で、より完全にDYA Studio機能をサポートしている。Zephyrバージョンは`v3.5.0+zmk-fixes`のままで公式v0.3と同一（`main+dya`が要求する独自Zephyrフォーク`v4.1.0+...`は不要）と確認済み。
- **`src/board.c`（独自のBLE分割電源管理）は本家のDYA Studio対応ブランチでも完全に無変更のまま存在**（diffで確認、内容が一致）。つまりboard.cはDYA Studio対応と衝突しない。
- **Studio RPC関連のKconfigは、両半体共通の`.conf`ではなく`snippets/split-central/split-central.conf`（centralロール専用スニペット）に集約**するのが本家の設計パターン。`CONFIG_ZMK_RUNTIME_INPUT_PROCESSOR`・`CONFIG_ZMK_SETTINGS_RPC_STUDIO`・`CONFIG_ZMK_BLE_MANAGEMENT_STUDIO_RPC`・`CONFIG_ZMK_RUNTIME_INPUT_PROCESSOR_STUDIO_RPC`はここに置く。
- **`CONFIG_ZMK_SPLIT_RELAY_EVENT`は実際には`zmk-module-settings-rpc`が`default y`で要求する設定だった**（Step 3〜4で「ble-managementが使わないから不要」と判断して削除したのは、settings-rpcを導入していなかった当時の情報に基づく誤った判断）。settings-rpc導入に伴い復活させる。
- **本家は`zmk-module-runtime-input-processor`（トラックボール速度調整）を実際に使えている。** ただし本家のwest.ymlは`revision: main`と書かれているが、これは2026-02-24時点のCIが最後に成功した時のHEADを指しているだけで、**現在のmain HEADは以前調査した通りZMK本家mainの新しいコアAPI(`zmk_keymap_layer_activate`の2引数版)に依存しており、`v0.3-branch+dya`（今も1引数のまま）とはリンクエラーになる**ことを実際に確認した。モジュールのコミット履歴を調査し、この非互換が入る直前のコミット（2026-02-24、`dbf92f764de8b6ffd60bf5850514302875fe2570`、ちょうど本家の最終成功ビルドと同日）に固定した。同様に`zmk-module-ble-management`も新しいAPI依存が入る前のコミット（Step 3で発掘済みの`2147ba7`）を継続使用。`zmk-module-settings-rpc`は該当するリスクのあるAPI呼び出しが無いことをソースで確認できたため`main`のまま使用。

### 対応内容

- `config/west.yml`: `zmk`を`v0.3-branch+dya`に変更。`zmk-module-ble-management`（`2147ba7`固定）・`zmk-module-settings-rpc`（`main`）・`zmk-module-runtime-input-processor`（`dbf92f7`固定）を追加。
- `torabo_tsuki_lp_left.conf`・`torabo_tsuki_lp_right.conf`: 本家の設定に合わせて`CONFIG_BT_MAX_CONN=5`・`CONFIG_BT_MAX_PAIRED=5`・`CONFIG_ZMK_BLE_MANAGEMENT=y`・`CONFIG_ZMK_BATTERY_SKIP_IF_USB_POWERED=n`・`CONFIG_ZMK_SETTINGS_RPC=y`・`CONFIG_ZMK_SPLIT_RELAY_EVENT=y`・`CONFIG_ZMK_SPLIT_BLE_CENTRAL_SPLIT_RUN_STACK_SIZE=768`・`CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE=10000`を設定。
- `snippets/split-central/split-central.conf`: centralロール専用のStudio RPC設定（`CONFIG_ZMK_RUNTIME_INPUT_PROCESSOR=y`等4行）を追加。
- `torabo_tsuki_lp_right.overlay`: トラックボール用`trackball_speed_rip`ノードとpointing_listenerへの組み込みを復活（既存のレイヤー別scroll-snap設定は維持、既定値1/1で速度は変えない）。
- `snippets/input-split-listener/input-split-listener.overlay`: トラックパッド用`trackpad_speed_rip`ノードを同様に復活。
- `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT`（12時間、独自カスタマイズ）は変更していない。ただし`zmk-module-settings-rpc`はDYA Studio経由でアイドル/スリープタイムアウトを実行時変更できる機能を提供する（`zmk_activity_set_idle_ms`/`set_sleep_ms`）ため、Web UI側でうっかり変更されないよう留意（設定を変えた場合は本家配線と同様に手元の`.conf`のデフォルトへ差分が出る点は許容）。

**結果: ビルド試行錯誤の末 ✅ CIビルド成功。**

1回目の失敗: `torabo_tsuki_lp_left.conf`/`_right.conf`の`CONFIG_ZMK_BATTERY_SKIP_IF_USB_POWERED=n`が未定義シンボルへの代入としてKconfigエラー（`zmk-module-battery-history`を導入していないため存在しないシンボル）。削除して対応。

2回目の失敗: leftは成功したがrightが `error: static assertion failed: "processor_label trackball property +1 exceeds maximum length 8"`。`zmk-module-runtime-input-processor`の`CONFIG_ZMK_RUNTIME_INPUT_PROCESSOR_NAME_MAX_LEN`（既定8文字、終端文字込みで実質7文字までが目安）を、独自定義した`trackball_speed_rip`/`trackpad_speed_rip`ノードの`processor-label = "trackball"`（9文字）/`"trackpad"`（8文字）が超過していた。`"tball"`/`"tpad"`に短縮して解消。

なお`CONFIG_ZMK_SPLIT_BLE_CENTRAL_SPLIT_RUN_STACK_SIZE`・`CONFIG_ZMK_POINTING_SMOOTH_SCROLLING`のような「centralロール専用設定をperipheral側の.confにも書いてしまっている」役割不一致のKconfig警告は、本家リポジトリも同じパターンで実際にビルドが通っていることから**非致命的（ビルドを止めない）**と判明。「Kconfig警告=即ビルド失敗」ではなく、未定義シンボルへの代入など一部の警告のみが実際にfatalになる模様。

**実機での動作確認結果:** 安定性は問題なし。ただしDYA Studioへの接続は依然失敗（USBは一覧に出るが繋がらず、BLEは一覧にすら出ない）。

## 根本原因判明: `CONFIG_ZMK_STUDIO_LOCKING=n`がBLE検出を妨げていた（2026-08-30）

`v0.3-branch+dya`の`app/src/studio/Kconfig`・`core.c`を確認したところ、次の仕組みが判明した:

```
config ZMK_STUDIO_LOCK_BLE_DIRECT_ADVERTISING_ON_UNLOCK
    bool "Enable Directed Advertising on Unlock"
    default y if ZMK_STUDIO_LOCKING && ZMK_BLE
    help
      When enabled, the keyboard will enable directed advertising to active profile
      during unlock. It's required to detect device from web bluetooth API for some browsers.
```

`zmk_studio_core_unlock()`（`&studio_unlock`バインディング実行時に呼ばれる）が、この設定が有効な場合にのみ`zmk_ble_set_directed_advertising(true)`を呼んでBLE広告を開始する。この設定のデフォルトは`ZMK_STUDIO_LOCKING`が有効な場合のみ`y`になる。

本リポジトリは以前から（DYA Studio対応より前から）`CONFIG_ZMK_STUDIO_LOCKING=n`（常時アンロック、KeymapEditor等での利便性のため）に設定していた。この場合`zmk_studio_core_unlock()`自体が一度も呼ばれない（起動時から既にunlocked状態のため、ロック→アンロックの"遷移"が発生しない）ため、BLE広告のトリガーが一度も発生しない。**これがBLEで一覧にすら出ない直接の原因。**

本家(sekigon-gonnoc)のDYA Studio対応ブランチでは`CONFIG_ZMK_STUDIO_LOCKING=n`ではなく単にコメントアウト（＝デフォルトの有効状態）になっており、この差異に対応していなかったことが今回の見落としだった。

**対応:** `torabo_tsuki_lp_left.conf`・`torabo_tsuki_lp_right.conf`の`CONFIG_ZMK_STUDIO_LOCKING=n`を削除し、ロック機能を有効化。ユーザーが既にキーマップに追加済みの`&studio_unlock`（Bluetoothレイヤー、コミット`697edcd`）を接続前に押す運用に変更。これによりKeymapEditor等での「都度アンロック不要」という従来の利便性は失われるが、DYA Studio対応（特にBLE検出）とはトレードオフの関係にあるため、DYA Studio対応を優先した。

USBが一覧には出るが繋がらない件も、DYA Studioクライアント側がトランスポート種別によらず共通で「アンロック待ち」のような扱いをしている可能性があり、この変更で合わせて改善するか実機で要確認。
