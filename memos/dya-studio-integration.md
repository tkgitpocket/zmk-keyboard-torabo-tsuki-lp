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
| `zmk-feature-custom-settings` | 各種カスタム設定の永続化バックエンド（他モジュールの依存先） | `main` |
| `zmk-module-runtime-input-processor` | トラックボール速度・回転・軸反転等をWeb UIから実行時調整 | `main` |
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
