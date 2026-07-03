# mac/ios統合・Windowsレイヤー削除・BTプロファイル別デフォルトレイヤー化

> 作成: 2026-07-04
> 状態: 実装済み・実機未検証

---

## 背景

[cormoran/zmk-keyboard-dya-dash](https://github.com/cormoran/zmk-keyboard-dya-dash) を参考にmac/ios/windows用のデフォルトレイヤーを分けていたが、以下が判明:

- 本リポジトリの `mac_default_layer` と `ios_default_layer` は display-name 以外完全に同一。dya-dash側の実装も同様に両者が完全一致していたため、そもそも分ける必要がないと判断し統合した。
- `windows_default_layer` は全キーが `&trans`（デフォルトレイヤーへの委譲のみ）だったため、デフォルトレイヤー(0番)自体が事実上Windows用として機能しており、削除した。
- 同様の理由で `center_layer`/`center2_layer`（"Center SPACE(mac)"）も完全に同一だが、**今回はユーザーの希望により統合していない**（残置）。

## 変更内容

### レイヤー番号の振り直し（`config/layers.dtsi`）

| 旧番号 | 旧定数 | 新番号 | 新定数 |
| --- | --- | --- | --- |
| 0 | DEFAULT_L | 0 | DEFAULT_L（変更なし） |
| 1 | WINDOWS_L | - | 削除 |
| 2 | MAC_L | 1 | APPLE_L（mac/ios統合） |
| 3 | IOS_L | - | 削除（APPLE_Lに統合） |
| 4 | LEFT_L | 2 | LEFT_L |
| 5 | RIGHT_L | 3 | RIGHT_L |
| 6 | TRACKBALL_L | 4 | TRACKBALL_L |
| 7 | SLOWTRACKBALL_L | 5 | SLOWTRACKBALL_L |
| 8 | SCROLL_L | 6 | SCROLL_L |
| 9 | FASTRSCROLL_L | 7 | FASTRSCROLL_L |
| 10 | CENTER_L | 8 | CENTER_L |
| 11 | CENTER2_L | 9 | CENTER2_L |
| 12 | BT_L | 10 | BT_L |
| 13 | SLOWSCROLL_L | 11 | SLOWSCROLL_L |

`config/keymap.keymap` 内の `&lt`/`&mo`/`&to` はKeymapEditor対策で数値リテラル直書きにしているため（[keymap-editor-compatibility.md](keymap-editor-compatibility.md)参照）、該当箇所を全て新番号に手動で置き換えた。`torabo_tsuki_lp_right.overlay` 側は `layers.dtsi` のマクロ経由で参照しているため変更不要。

### BTプロファイル別デフォルトレイヤー

[Zenn記事](https://zenn.dev/shakupan/articles/261ce435251607) を参考に検討したが、記事の手法（レイヤートグルをON/OFF個別に切り替えるマクロを新設）よりシンプルな方法を採用した。本リポジトリには元々BT接続先切り替え用マクロ `out_bt_0`〜`out_bt_4` があったため、ここにZMK標準の `&to <layer>`（デフォルトレイヤーを直接切り替える）を追記するだけで実現した。

- `out_bt_0`・`out_bt_1`（プロファイル0・1、Windows想定） → `&to 0`
- `out_bt_2`・`out_bt_3`・`out_bt_4`（プロファイル2〜4、Apple想定） → `&to 1`

### zmk-layout-shift(JIS変換)はBTプロファイルと独立

`&tog_ls_on` はセントラル側ファームウェアの単一グローバルフラグで、BTプロファイルに依存しない。ユーザーがプロファイル4でMacに接続した際に記号がUS配列相当になった件は、Mac側のOS設定（キーボードの種類がJIS認識になっていなかったこと）が原因で、macOS側で「キーボードの種類を変更」からJISを選択したところ解決した（ファームウェア側の問題ではなかった）。

### center2_layer（Mac用センターレイヤー）の有効化とショートカット差し替え

`apple_default_layer` のSPACE位置がまだ `&trans` のままで `center_layer`（Windows用）に委譲されており、`center2_layer` がどこからも呼ばれていなかったため、`apple_default_layer` のSPACE位置を `<lt 9 SPACE>`（CENTER2_L）に変更して有効化した。

あわせて `center2_layer` の内容を、Windows前提のCtrl系ショートカットからMacの標準ショートカットに置き換えた（ユーザーはWindows側でEmacs風にCtrl押下状態を模したナビゲーション配置をしていたため、Mac側もCmd相当に統一）。

| 用途 | center_layer（Windows） | center2_layer（Mac） |
| --- | --- | --- |
| 行頭移動 | `HOME` | `LG(LEFT)`（Cmd+←） |
| 行末移動 | `END` | `LG(RIGHT)`（Cmd+→） |
| 元に戻す | `LC(Y)`（Ctrl+Y） | `LG(LS(Z))`（Cmd+Shift+Z） |
| 保存 | `LC(S)`（Ctrl+S） | `LG(S)`（Cmd+S） |
| 元に戻す(undo) | `LC(Z)`（Ctrl+Z） | `LG(Z)`（Cmd+Z） |
| 切り取り | `LC(X)`（Ctrl+X） | `LG(X)`（Cmd+X） |
| コピー | `LC(C)`（Ctrl+C） | `LG(C)`（Cmd+C） |
| 貼り付け | `LC(V)`（Ctrl+V） | `LG(V)`（Cmd+V） |
| スクリーンショット | `PRINTSCREEN` | `LS(LG(N5))`（Cmd+Shift+5） |

矢印キー・PageUp/Down・Backspace/Delete・Enter・マウスボタン・`<lt 10 SPACE>`（BT_Lへの入れ子アクセス）はOS差が無いためcenter_layerと同一のまま維持。

## 未検証・要フォローアップ

- 実機での動作確認は一部実施済み（BTプロファイル3→Mac接続、かな/英数、スクロール系レイヤーは問題なし。JIS記号もMac側のキーボード種別設定変更で解決）。
- `center2_layer` のMac向けショートカット差し替えは今回のセッションで実装したのみで実機未検証。
- `center_layer`/`center2_layer` の統合は見送り、今回はむしろOS差分の受け皿として活用する方針に転換した。

## 関連

- [layout-shift-v2-migration.md](layout-shift-v2-migration.md)
- [keymap-editor-compatibility.md](keymap-editor-compatibility.md)
