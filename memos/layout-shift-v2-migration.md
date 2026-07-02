# zmk-layout-shift v1 → v2 移行メモ

> 作成: 2026-07-03
> 状態: 移行完了・実機での動作確認は未実施（要: `&tog_ls_on` を一度押してJIS変換を有効化）

---

## 背景・経緯

- 以前から `config/keymap.keymap` は [zmk-layout-shift](https://github.com/kot149/zmk-layout-shift) v1 を `west.yml` に入れていたが、実際の記号キーは `&kpls` やレイアウトマップを使わず、**JP_xxx という独自 `#define` マクロ（`JP_DQUOTE`=`AT` 等）で直接キーコードを指定する方式**になっていた。
  - つまり v1 の変換機能自体は当時「うまく動かなかった」ため使わず、変換後の物理キーコードを手作業で調べて直書きしていたと推測される（本人談）。
  - 副作用: KeymapEditor が `JP_xxx` を認識できず、記号キーの表示が崩れる/分かりにくい問題があった。
- 今回、KeymapEditor 互換性を取り戻すため v2 に移行し、`JP_xxx` を廃止して v2 が提供する標準キーコード + `layout_shift_map_us_to_jis` に置き換えた。

## 調査で分かったこと

- v2 の組み込みマップ `layout_shift_map_us_to_jis`（`dts/layout_shift_maps.dtsi`）の変換テーブルは、**旧 `JP_xxx` マクロの変換先キーコードと完全一致**していた（例: `JP_DQUOTE`=`AT` ⇔ マップの `DOUBLE_QUOTES → AT_SIGN`）。
  - このため置き換えは「意味的に等価な標準キーコードへの機械的な読み替え」で済み、実際の出力（JIS OS上でどの記号が出るか）は変わらないはず。
  - 対応表は本メモ末尾を参照。
- v1 README にあった注意書き「`layout_shift_kp_override.dtsi` の include は `behaviors.dtsi` 等より **下** に置く必要がある。KeymapEditor が include順を並べ替えてしまうので、`&kp` の定義はキーマップに直接コピペするのが安全」という一文が v2 README にも引き継がれていた。
  - 本リポジトリは既にこの回避策（`&kp` の再定義をキーマップの `behaviors {}` 内に直書き）を実施済みだったため、**そのまま流用可能**。今回は `#include <layout_shift_kp_override.dtsi>` を `#include <layout_shift.dtsi>`（`&kp` を上書きしない生の include）に差し替えるだけで済ませた。
- v2 の有効/無効状態はモジュールのソース（`src/layout_shift_map.c`）で `data->active = false;` とハードコードされており、**devicetree側に「デフォルトでON」にするプロパティは存在しない**。
  - `CONFIG_LAYOUT_SHIFT_PERSISTENT_STATE=y`（デフォルト有効）により、一度 `&tog_ls_on` を押せば設定が不揮発領域に保存され、以後は再起動・再フラッシュ（全消去でない限り）でも保持される。
  - ユーザーと相談の上、モジュールをforkして規定値を変更する案は見送り、**「初回フラッシュ後に一度だけ `&tog_ls_on` を押す」運用で確定**した。

## 変更内容

- `config/west.yml`: `zmk-layout-shift` の `revision` を `v1` → `v2` に変更。
- `config/keymap.keymap`:
  - `#define JP_xxx` を全廃止。
  - `#include <layout_shift_kp_override.dtsi>` → `#include <layout_shift.dtsi>`。
  - `&tog_ls` / `&tog_ls_on` / `&tog_ls_off` に `layout-maps = <&layout_shift_map_us_to_jis>;` を設定。
  - 記号キーの `&kp JP_xxx` を、対応する標準キーコード（下表）に置換。
  - `Bluetooth` レイヤーの `out_bt_4` の隣（それまで `&trans`）に `&tog_ls_on` を配置。ここは初回セットアップ用の一時的な置き場所であり、恒久的な位置として意図したものではない。将来ここが不便なら移動して構わない。
  - `JP_KANA` / `JP_EISU` / `JP_HANZEN` / `JP_BSLH` はどのバインディングからも未使用、または US→JIS 変換テーブルに含まれない単純な直接キーコードだったため、`JP_BSLH` は `INT1` に置換、それ以外は削除（マクロごと不要になった）。
- `CLAUDE.md`: 外部モジュール表とキーマップ節の説明を v2 の内容に合わせて更新。

## 旧マクロ → 新キーコード対応表

| 旧マクロ (旧値) | 新キーコード (`&kp` に渡す値) | 備考 |
| --- | --- | --- |
| `JP_DQUOTE` (`AT`) | `DOUBLE_QUOTES` | map: `DOUBLE_QUOTES → AT_SIGN` |
| `JP_AMPERSAND` (`CARET`) | `AMPERSAND` | map: `AMPERSAND → CARET` |
| `JP_QUOTE` (`AMPERSAND`) | `SINGLE_QUOTE` | map: `SINGLE_QUOTE → AMPERSAND` |
| `JP_EQUAL` (`UNDER`) | `EQUAL` | map: `EQUAL → UNDERSCORE` |
| `JP_CARET` (`EQUAL`) | `CARET` | map: `CARET → EQUAL` |
| `JP_YEN` (`0x89`) | `BACKSLASH` | map: `BACKSLASH → 0x89` |
| `JP_PLUS` (`COLON`) | `PLUS` | map: `PLUS → COLON` |
| `JP_TILDE` (`PLUS`) | `TILDE` | map: `TILDE → PLUS` |
| `JP_PIPE` (`LS(0x89)`) | `PIPE` | map: `PIPE → LS(0x89)` |
| `JP_AT` (`LEFT_BRACKET`) | `AT` | map: `AT_SIGN → LEFT_BRACKET` |
| `JP_COLON` (`SINGLE_QUOTE`) | `COLON` | map: `COLON → SINGLE_QUOTE` |
| `JP_ASTERISK` (`DOUBLE_QUOTES`) | `ASTERISK` | map: `ASTERISK → DOUBLE_QUOTES` |
| `JP_BACKQUOTE` (`LEFT_BRACE`) | `GRAVE` | map: `GRAVE → LEFT_BRACE` |
| `JP_UNDERSCORE` (`LS(0x87)`) | `UNDERSCORE` | map: `UNDERSCORE → LS(0x87)` |
| `JP_LBRACKET` (`RIGHT_BRACKET`) | `LEFT_BRACKET` | map: `LEFT_BRACKET → RIGHT_BRACKET` |
| `JP_RBRACKET` (`BACKSLASH`) | `RIGHT_BRACKET` | map: `RIGHT_BRACKET → BACKSLASH` |
| `JP_LPAREN` (`ASTERISK`) | `LEFT_PARENTHESIS` | map: `LEFT_PARENTHESIS → ASTERISK` |
| `JP_RPAREN` (`LEFT_PARENTHESIS`) | `RIGHT_PARENTHESIS` | map: `RIGHT_PARENTHESIS → LEFT_PARENTHESIS` |
| `JP_LBRACE` (`RIGHT_BRACE`) | `LEFT_BRACE` | map: `LEFT_BRACE → RIGHT_BRACE` |
| `JP_RBRACE` (`PIPE`) | `RIGHT_BRACE` | map: `RIGHT_BRACE → PIPE` |
| `JP_BSLH` (`INT1`) | `INT1`（変更なし） | US→JIS変換テーブルに存在しないため素通し |
| `JP_KANA` / `JP_EISU` / `JP_HANZEN` | 削除（未使用） | どのバインディングからも参照されていなかった |

## 未検証・要フォローアップ

- 実機での動作確認（symbolキーが期待通りJIS配列で出力されるか）は未実施。特に `&tog_ls_on` を押す前は変換が無効なため、フラッシュ直後は記号がUS配列のまま出力される点に注意。
- v2への移行で `layout_shift.dtsi` の include 順序が実際に問題なくビルドできるか（CI通過）は次回 `git push` 時に要確認。
