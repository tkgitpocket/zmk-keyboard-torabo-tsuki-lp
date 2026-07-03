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

`&tog_ls_on` はセントラル側ファームウェアの単一グローバルフラグで、BTプロファイルに依存しない。ユーザーがプロファイル4でMacに接続した際に記号がUS配列相当になった件は、Mac側のOS設定（キーボードの種類/入力ソースがJIS認識になっていない）が原因である可能性が高いと回答した（ファームウェア側の問題ではない）。

## 未検証・要フォローアップ

- 実機での動作確認は未実施。特にレイヤー番号の振り直しにより `&lt`/`&mo`/`&to` の参照がずれていないか、実機でBTプロファイル切り替え・各レイヤー起動を一通り確認する必要がある。
- `center_layer`/`center2_layer` の統合は今回見送ったが、後日希望があれば同様の手順で統合可能。

## 関連

- [layout-shift-v2-migration.md](layout-shift-v2-migration.md)
- [keymap-editor-compatibility.md](keymap-editor-compatibility.md)
