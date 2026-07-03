# CLAUDE.md

このファイルは、Claude Code (claude.ai/code) がこのリポジトリで作業する際のガイダンスです。

## 概要

[torabo-tsuki LP](https://github.com/sekigon-gonnoc/torabo-tsuki-lp) 向けの ZMK ファームウェア設定リポジトリ。
nRF52（BMP ボード経由）を搭載したワイヤレス分割キーボードで、トラックボール（PAW3222、SPI）またはミニトラックパッド（IQS7211E、I2C）のポインティングデバイスを備える。

## ビルド

ローカルビルドコマンドはなく、GitHub Actions でビルドする。push することで CI がトリガーされる。

```
git push  # .github/workflows/build.yml を起動
```

ワークフローは `zmkfirmware/zmk/.github/workflows/build-user-config.yml@v0.3` を呼び出す。
ビルド成功後、Actions の実行結果から `.uf2` アーティファクトをダウンロードして書き込む。

**書き込み先:**

- `*_central` （右側・トラックボールがある方） → ポインティングデバイスが付いている半体に書き込む
- `*_peripheral` （左側） → 反対側の半体に書き込む

## リポジトリ構成

```
config/
  west.yml        # ZMK 本体および外部モジュールの依存関係
  keymap.keymap   # メインキーマップ（シールドからインクルードされる）
  layers.dtsi     # レイヤー番号の定数定義（DEFAULT_L, LEFT_L, SCROLL_L 等）

boards/shields/torabo_tsuki_lp/
  torabo_tsuki_lp.dtsi          # 共通ハードウェア定義（GPIO マトリクス・SPI/I2C ピン・S/M/L レイアウト）
  torabo_tsuki_lp.keymap        # config/keymap.keymap をインクルードするだけの薄いラッパー
  torabo_tsuki_lp_left.overlay  # 左（ペリフェラル）— 最小限の XY 反転設定
  torabo_tsuki_lp_right.overlay # 右（セントラル・トラックボール側）— pointing_listener の入力プロセッサ・scroll-snap・レイヤー別入力設定
  torabo_tsuki_lp_left.conf     # 左の Kconfig（ZMK Studio・BLE 送信電力・バッテリーProxy・スムーススクロール）
  torabo_tsuki_lp_right.conf    # 右の Kconfig（左と同内容）
  Kconfig.defconfig             # ZMK_SPLIT・SPI・INPUT・ZMK_MOUSE を有効化

snippets/
  input-trackball/      # PAW3222 トラックボール（SPI）
  input-trackpad/       # IQS7211E トラックパッド（I2C）
  input-trackpad-mini/  # IQS7211E ミニトラックパッド（I2C、scroller-mode 有効）
  input-listener/       # pointing_listener ノードを有効化してデバイスを接続
  input-split/          # ペリフェラル側でポインティングイベントを中継する input-split を有効化
  input-split-listener/ # セントラル側で split ポインティングイベントを受信
  split-central/        # CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y を設定

src/
  board.c                       # BLE 接続の電力管理（非アクティブ時間に応じた 4 段階スリープ）
  mini_trackpad_init_reg.c      # IQS7211E の初期化レジスタシーケンス
  mini_trackpad_iqs7211e_init.h # 初期化レジスタデータ
```

## アーキテクチャ

### 分割構成

ワイヤレス分割ペア。右（セントラル・トラックボール側）がホストへの BLE 接続を管理し、左（ペリフェラル）がキーイベントを BLE でセントラルに送信する。

ポインティングデバイスの接続経路は、ビルド時に適用するスニペットの組み合わせで決まる。

### キーボードサイズバリアント

`torabo_tsuki_lp.dtsi` に S（10×4）・M（12×4）・L（12×5）の 3 種類の物理レイアウトを定義。
`zmk,physical-layout = &physical_layout_l` で使用レイアウトを選択する。
右オーバーレイでは各トランスフォームに `col-offset = <7>` を適用し、右半体のマトリクス列を正しくオフセットする。

### ポインティングデバイス入力パイプライン

共通 `.dtsi` で `pointing_listener` は `status = "disabled"` として定義されている。スニペットで有効化してデバイスを接続する。右オーバーレイでレイヤーごとの入力プロセッサ分岐を設定している。

| レイヤー | 動作 |
| -------- | ---- |
| 通常時 | XY 反転・1/2 速度 |
| SLOWTRACKBALL_L | XY 反転・1/6 速度（低速カーソル） |
| SCROLL_L | スクロールモード（scroll-snap・X軸反転・1/5 速度） |
| FASTRSCROLL_L | 高速スクロール（scroll-snap・X軸反転・3/2 速度） |

[zmk-scroll-snap](https://github.com/kot149/zmk-scroll-snap) と [zmk-layout-shift](https://github.com/kot149/zmk-layout-shift) モジュール（kot149）を使用。

### キーマップ

`config/keymap.keymap` で全レイヤーを定義。記号キーは [zmk-layout-shift](https://github.com/kot149/zmk-layout-shift) の `layout_shift_map_us_to_jis` マップにより、US 配列前提のキーコード（`AT`・`DOUBLE_QUOTES` 等の標準キーコード）から JIS 配列 OS 向けのスキャンコードへ変換される。`&kp` は KeymapEditor 互換性のため `zmk,behavior-layout-shift-key-press` でオーバーライドしている。Bluetooth レイヤーに配置した `&tog_ls_on` を一度押すと変換が有効化され、以降は設定が永続化される。

レイヤー定数は `config/layers.dtsi` で定義し、キーマップと右オーバーレイの両方から参照する。

| 番号 | 定数名 | 用途 |
| ---- | ------ | ---- |
| 0 | DEFAULT_L | ベース QWERTY（Windows 用としてそのまま使う） |
| 1 | APPLE_L | macOS・iOS 共用バリアント（キーコードが同一のため統合） |
| 2 | LEFT_L | 記号（左親指で起動） |
| 3 | RIGHT_L | 数字・F キー（右親指で起動） |
| 4 | TRACKBALL_L | マウスボタン |
| 5 | SLOWTRACKBALL_L | 低速カーソルモード |
| 6 | SCROLL_L | スクロールモード |
| 7 | FASTRSCROLL_L | 高速スクロールモード |
| 8 | CENTER_L | ナビゲーション・クリップボード |
| 9 | CENTER2_L | ナビゲーション・クリップボード（Mac 用） |
| 10 | BT_L | Bluetooth プロファイル切り替え |
| 11 | SLOWSCROLL_L | 低速スクロールモード |

`out_bt_0`/`out_bt_1`（BT プロファイル 0・1）は `&to 0` で DEFAULT_L（Windows）に、`out_bt_2`〜`out_bt_4`（プロファイル 2〜4）は `&to 1` で APPLE_L に切り替える。プロファイルごとに最後に使ったレイヤーではなく常にこのデフォルトレイヤーへ戻る。

`config/keymap.keymap` 内の `&lt`/`&mo`/`&to` の第1引数は、[KeymapEditor](https://nickcoutsos.github.io/keymap-editor/) がCプリプロセッサを実行せず生テキストをそのまま解釈する仕様のため、上記マクロ名ではなく数値リテラルを直接記述している（`layers.dtsi` のマクロは右オーバーレイ側でのみ使用）。

### 電力管理（`src/board.c`）

非アクティブ時間に応じて BLE 接続パラメータを 4 段階（ACTIVE → SLEEP1 → SLEEP2 → SLEEP3）に調整するカスタム実装。
タイムアウトは 5秒 / 15秒 / 30秒。USB 給電中はアクティブモードを維持する。
`CONFIG_ZMK_SPLIT_ROLE_CENTRAL` が有効な場合のみコンパイルされる。

### 外部モジュール（`config/west.yml`）

| モジュール | 提供元 | 用途 |
| ---------- | ------ | ---- |
| zmk | zmkfirmware v0.3 | ZMK 本体 |
| zmk-component-bmp-boost | sekigon-gonnoc v0.2 | BMP ボードサポート |
| zmk-feature-status-led | sekigon-gonnoc | ステータス LED |
| zmk-driver-paw3222 | sekigon-gonnoc (torabo-tsuki ブランチ) | トラックボールドライバ |
| zmk-driver-iqs7211e | sekigon-gonnoc | トラックパッドドライバ |
| zmk-feature-cdc-acm-bootloader-trigger | sekigon-gonnoc v0.2 | USB ブートローダートリガー |
| zmk-feature-non-lipo-battery-management | sekigon-gonnoc | 非 LiPo バッテリー ADC 管理 |
| zmk-scroll-snap | kot149 v1 | スクロール軸ロック機能 |
| zmk-layout-shift | kot149 v2 | US -> JIS 等のキーコード変換（実行時レイアウトシフト） |

## キーマップ編集

ファームウェアを再ビルドせずにキーマップを編集する方法：

- **ZMK Studio** — USB 接続時にリアルタイム編集
- **keymap-editor** — Web UI で編集後、`config/keymap.keymap` を生成して commit・push
