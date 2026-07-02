# KeymapEditor 互換性についての調査メモ

> 作成: 2026-07-03
> 状態: レイヤー番号の数値化は完了。保存(コミット)機能は実機テストで安全性を確認済み

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

- `&tog_ls_on` のような今回追加した独自 behavior は、KeymapEditor の `zmk-behaviors.json` に登録が無いため赤字+警告アイコンで表示される（辞書には `&kp &lt &mo &mt &to &tog &sk &sl &bt &bl &out &trans &none &caps_word &key_repeat &ext_power &bootloader &reset &rgb_ug` 程度しか無い模様）。外部モジュール側の behavior はツールが認識できないので、これは表示上直しようがない。
- `LANG2` のような一部キーコードも同辞書に見当たらず `⊘` になる（`mac_default_layer`/`ios_default_layer` の `&lt RIGHT_L LANG2` で発生）。ZMKの正式名（エイリアスではない方）`LANGUAGE_2` に変更したところ解決した。同様に `LANG1`/`LANG3` 等の短縮エイリアスを使う場合は `LANGUAGE_1`/`LANGUAGE_3` 等の正式名を使うと安全と思われる。
- ただし `&mkp`（マウスキー）は静的辞書には見当たらないにもかかわらず実際のスクリーンショットでは正常表示されていた。ユーザーが実際に使っている KeymapEditor のインスタンス／バージョンが手元で確認した `main` ブランチのコードと完全に一致しない可能性がある。**この辺りの挙動は推測込みであり、100%の確証はない。**

### 2.5 副産物として発見したバグ: `mac_default_layer`/`ios_default_layer` のタップキーコード欠落

`&lt RIGHT_L LANG2`（現在は `&lt 5 LANGUAGE_2`）の並びで表示を確認していたところ、同じ行の `&lt LEFT_L`（現在は `&lt 4`）に**タップ時のキーコードが1つ欠落**していることが判明（KeymapEditorのQuick Assignモーダルで「Key Code」パラメータが無効表示になっていた）。

- CIのビルド履歴（GitHub Actions）を確認したところ、このバグがあってもビルド自体は成功していた（`west build`のキーマップ生成はセルの欠落に対してエラーにならず、単に意図したキーコードが送出されない状態だった模様）。
- Windows想定のデフォルトレイヤー（layer 0）では該当位置が `&lt 4 INT_HENKAN`（変換）/ `&lt 5 INT_MUHENKAN`（無変換）。Mac版レイヤーは無変換側を `LANGUAGE_2`（英数）に上書き済みだったので、対称性から変換側も `LANGUAGE_1`（かな）に上書きするのが妥当と判断し追加した。
- 教訓: KeymapEditorの「未認識」表示（⊘）は、単なるツール側の辞書不足だけでなく、こういう**本当にパラメータが欠落している実バグ**の発見にも役立つ。表示がおかしい箇所は「ツールの限界」と決めつけず、まず実際のソースを見て確認する価値がある。

### 3. 実機テストの結果: 保存(コミット)は安全だった（`main`ブランチのソース読解による懸念は外れ）

`nickcoutsos/keymap-editor` の `main` ブランチのソース（`api/services/github/files.js` の `commitChanges`）を読むと、`config/keymap.json` + `*.keymap.template`（無ければ最小テンプレート）から `config/keymap.keymap` を丸ごと再生成するように見えたため、combos・macros・behaviors・display-name・レイヤーノード名が失われるおそれを懸念していた。

しかし `keymap-editor-test` ブランチで実際に「デフォルトレイヤーのBをAに変更して保存」を行ってもらったところ（コミット [`a7175dc`](https://github.com/tkgitpocket/zmk-keyboard-torabo-tsuki-lp/commit/a7175dcd6f7e0eb9c1e1e32e68fd5dd30c017713)）、実際の差分は:

- 意図した `&kp B` → `&kp A` の変更
- 触れたレイヤーのバインディング表の列幅の再フォーマット（空白の詰め直し）
- `&tog_ls_on { ... };` の前後など、一部に空行が1つ追加

のみで、**combos・macros・behaviors ブロック・`&tog_ls*` の設定・各レイヤーの `display-name`・レイヤーノード名はすべて変更されずに保持されていた**。`config/keymap.json` ファイルが新規作成されることもなかった。

→ 実際にホストされているサービスは、手元で読んだ `main` ブランチのテンプレート丸ごと再生成ロジックとは異なる（おそらくAST的に既存の `.keymap` を差分編集する）実装になっている可能性が高い。**「ソースコードを読んだ推測」より「実機での保存テスト結果」を信用するべき**、という教訓。

### 結論

- KeymapEditorでの保存は通常利用で問題なさそう。保存のたびに列幅がわずかに変わったり空行が増減したりする程度の副作用はある（機能に影響なし）。
- `*.keymap.template` の作成は不要と判断。
- 念のため、大きな変更をKeymapEditorで保存した際は一度 `git diff` で意図しない変更が無いか確認する習慣を推奨。

## 関連

- [layout-shift-v2-migration.md](layout-shift-v2-migration.md) — 今回の調査のきっかけになった `JP_xxx` 置き換え作業
