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

`config/west.yml` の `zmk` を `cormoran/zmk` の `v0.3-branch+custom-studio-protocol+ble` に変更してpush。（結果は次回更新）
