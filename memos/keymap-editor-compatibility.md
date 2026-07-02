# KeymapEditor 互換性についての調査メモ

> 作成: 2026-07-03
> 状態: レイヤー番号の数値化は完了。保存(コミット)機能の安全性は未検証・要フォローアップ

---

## 背景

[zmk-layout-shift v2 移行](layout-shift-v2-migration.md)の過程で `JP_xxx` マクロを標準キーコードに置き換えたところ、[KeymapEditor](https://nickcoutsos.github.io/keymap-editor/) 上で今度は `layers.dtsi` のレイヤー番号マクロ（`DEFAULT_L`・`SLOWSCROLL_L` 等）が `⊘` 表示になっていることが判明。原因を [nickcoutsos/keymap-editor](https://github.com/nickcoutsos/keymap-editor) のソース（`main` ブランチ）を実際に読んで調査した。

## 分かったこと

### 1. KeymapEditorはCプリプロセッサを実行しない

`api/services/zmk/keymap.js` の `parseKeyBinding` は正規表現で `&behavior param1 param2` を分解し、`param`をそのまま文字列としてサーバー同梱の静的辞書（`api/services/zmk/data/zmk-keycodes.json` / `zmk-behaviors.json`）と突き合わせているだけ。実際の `west build` のようにCプリプロセッサや `dtc` を通していない。

- そのため `#define` で定義した独自マクロ（旧 `JP_xxx`、今回の `layers.dtsi` のレイヤー定数）はすべて「未知のトークン」として `⊘` 表示になる。
- ファームウェアのビルド自体（west build時にCプリプロセッサが実際に走る）には影響しない。表示上の問題のみ。
- 対応: `config/keymap.keymap` 内で `&lt`/`&mo`/`&to` の第1引数（レイヤー番号）を直接の数値リテラルに変更済み。`layers.dtsi` のマクロ自体は右オーバーレイ側（`torabo_tsuki_lp_right.overlay`）でそのまま使い続けている。

### 2. 独自behavior・一部キーコードはマクロを解決しても直らない

- `&tog_ls_on` のような今回追加した独自 behavior は、KeymapEditor の `zmk-behaviors.json` に登録が無いため赤字+警告アイコンで表示される（辞書には `&kp &lt &mo &mt &to &tog &sk &sl &bt &bl &out &trans &none &caps_word &key_repeat &ext_power &bootloader &reset &rgb_ug` 程度しか無い模様）。西武 モジュール側の behavior はツールが認識できないので、これは表示上直しようがない。
- `LANG2` のような一部キーコードも同辞書に見当たらず `⊘` になる（`mac_default_layer`/`ios_default_layer` の `&lt RIGHT_L LANG2` で発生）。
- ただし `&mkp`（マウスキー）は静的辞書には見当たらないにもかかわらず実際のスクリーンショットでは正常表示されていた。ユーザーが実際に使っている KeymapEditor のインスタンス／バージョンが手元で確認した `main` ブランチのコードと完全に一致しない可能性がある。**この辺りの挙動は推測込みであり、100%の確証はない。**

### 3. 【重要・未解決】保存(コミット)機能を使うと独自devicetree定義が失われるおそれ

`api/services/github/files.js` の `commitChanges` を見ると、KeymapEditorで保存すると:

1. `config/keymap.json`（レイヤーのバインディング配列などの構造化データ）
2. `config/*.keymap.template`（存在すれば。無ければ `api/services/zmk/defaults.js` の最小テンプレート）

の2つから **`config/keymap.keymap` を丸ごと再生成**する。テンプレートの `{{rendered_layers}}` プレースホルダー部分だけがレイヤーのバインディングに置き換わり、それ以外は生テンプレートの内容がそのまま出力される。

本リポジトリには `*.keymap.template` が存在しないため、**現状のまま保存機能を使うと以下がすべて失われる**:

- `combos {}` ブロック（tab・shift_tab・mb4・mb5・semicolon・colon・comma・dot・mb3）
- `macros {}` ブロック（`to_layer_0`・`out_bt_0`〜`out_bt_4`・`out_bt_nxt`）
- `behaviors {}` ブロック（`&mt` override、`&kp` の layout-shift オーバーライド、`lt_to_layer_0`）
- `&tog_ls` / `&tog_ls_on` / `&tog_ls_off` の `layout-maps` 設定
- 各レイヤーの `display-name`（生成コードは `bindings` しか書き出さない模様）
- 各レイヤーのノード名（`windows_default_layer` 等の命名。生成時は `default_layer` / `layer_<layer_names[i]>` という機械的な命名になり、現状の名前とは一致しない）

### 対応方針（未着手）

`*.keymap.template` を用意し `{{behaviour_includes}}` / `{{rendered_layers}}` プレースホルダーで現状の構造を保持する案が考えられるが、上記の「レイヤーノード名」「display-name」が生成ロジック側で決め打ちになっている点は `*.keymap.template` だけでは解決できない可能性が高い（`config/keymap.json` の `layer_names` 配列の内容次第で変わるため、そちらも要調整）。

**保存機能を実際に使う前に、一度テスト用ブランチで「編集して保存」を試し、生成される `config/keymap.keymap` の差分を確認してから本運用するのが安全。**

## 関連

- [layout-shift-v2-migration.md](layout-shift-v2-migration.md) — 今回の調査のきっかけになった `JP_xxx` 置き換え作業
