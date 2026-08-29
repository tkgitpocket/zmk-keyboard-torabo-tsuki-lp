# zmk-battery-center で電池残量が常に100%表示になる件の調査プラン

## 症状
- [zmk-battery-center](https://github.com/kot149/zmk-battery-center) v0.10.0 (Windows) で本キーボードの電池残量が常に100%表示。
- roBa・Zonkey（LiPoバッテリー機種）では正しく反映されていた → LiPo機種との違いが原因の手がかり。

## 本機種の関連設定（既存コードから確認済み）
- `boards/shields/torabo_tsuki_lp/torabo_tsuki_lp.dtsi`
  - `zmk,battery = &non_lipo_battery;`（デフォルトの `vbatt` は `status = "disabled"` で無効化）
  - `non_lipo_battery` ノード: `compatible = "zmk,non-lipo-battery"`, `io-channels = <&adc 4>;`
  - → 標準の ZMK LiPo バッテリー監視ではなく、外部モジュール `zmk-feature-non-lipo-battery-management`（sekigon-gonnoc）による独自ドライバを使用。ソースはこのリポジトリ内には無く、west 経由で取得されるため未確認。
- `boards/shields/torabo_tsuki_lp/torabo_tsuki_lp_left.conf` / `_right.conf`（※`Kconfig.defconfig`ではなくこちらに記載。訂正）
  - `CONFIG_ZMK_NON_LIPO_MIN_MV=1000`
  - `CONFIG_ZMK_NON_LIPO_LOW_MV=900`
  - `CONFIG_ZMK_NON_LIPO_MAX_MV` の指定は**リポジトリ内のどこにも無い** → ドライバのデフォルト値がそのまま使われる。
- `torabo_tsuki_lp_left.conf` / `_right.conf` 双方に
  - `CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_PROXY=y`
  - `CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y`
  - → 左右分割の電池レベルを central 経由で中継する構成。zmk-battery-center 側がこの構成（1台のBLEデバイスに2つの電池レベルが乗る形）をどう扱っているかも切り分け対象。

## 仮説（優先度順）
1. **非LiPoドライバの電圧→%変換設定不足**: `CONFIG_ZMK_NON_LIPO_MIN_MV` / `LOW_MV` はあるが満充電側の基準電圧設定が無い/デフォルトのままで、実際の電池構成（本数・種類）と合っていないため常に100%と判定されている。
2. **ADC分圧回路のスケーリング不一致**: `io-channels = <&adc 4>` のハード側分圧比とドライバ側のDT bindingパラメータ（output-ohms/full-ohms等）が合っておらず、実電圧より高い値を読んでいる。
3. **ドライバ自体が未サンプリング/固定値**: `zmk,non-lipo-battery` ドライバがセンサー読み取りを行わず初期値100%のまま更新していないバグ・設定漏れ。
4. **split proxy構成とアプリ側の対応不備**: ファームウェア側は正しい値をGATTに反映しているが、zmk-battery-center側がこの機種のsplit battery proxy構成（もしくは非LiPo特有のGATT報告方法）に対応できていない。
5. **Windows側のBLE GATTキャッシュ**: 一度100%で読んだ値をキャッシュしていて、再ペアリングで直る可能性（切り分けの一つとして低優先度で確認）。

## 切り分け・調査ステップ（詳細調査は別モデルに委譲）
1. 使用している電池の種類・本数・公称電圧をユーザーに確認する（単4×n、リチウム一次電池等、電圧レンジの妥当性判断に必須）。
2. nRF Connect等のBLEスキャナで左右それぞれのBattery Level特性（UUID `0x2A19`）を直接読み、ファームウェアが返している生の%値を確認する。
   - ここが100%でない場合 → 原因はzmk-battery-center側（アプリ側の解析・split対応不足）。
   - ここも100%固定の場合 → ファームウェア/ドライバ側の問題。
3. ファームウェア側が原因の場合、`zmk-feature-non-lipo-battery-management`（https://github.com/sekigon-gonnoc/zmk-feature-non-lipo-battery-management）のソースを読み、
   - 電圧→%変換ロジック
   - `CONFIG_ZMK_NON_LIPO_*` の全Kconfigオプション（MAX_MV相当の有無）
   - ADC分圧のDT bindingパラメータ要否
   を確認し、本リポジトリの設定（`Kconfig.defconfig`, `.dtsi`）に不足があれば追加する。
4. 可能であればRTT/シリアルログでADC生値とドライバの計算結果を確認する。
5. アプリ側が原因と判明した場合、`kot149/zmk-battery-center` のIssueトラッカーで同様の報告（非LiPo機種・split proxy構成）がないか確認する。
6. 修正案が固まったら `.conf`/`.dtsi` を変更し、`git push` でCI（GitHub Actions）ビルド → `.uf2` を実機に書き込んで電池残量表示を確認する。

## 調査結果（外部モジュールソース確認済み）

`zmk-feature-non-lipo-battery-management`（<https://github.com/sekigon-gonnoc/zmk-feature-non-lipo-battery-management>）のソースを直接確認した。

### Kconfig（`Kconfig`）

| オプション | 説明 | デフォルト値 |
| ---------- | ---- | ------------ |
| `CONFIG_ZMK_NON_LIPO_MIN_MV` | 残量0%相当の最小電圧 | 1100 mV |
| `CONFIG_ZMK_NON_LIPO_MAX_MV` | 残量100%相当の最大電圧 | **1300 mV** |
| `CONFIG_ZMK_NON_LIPO_LOW_MV` | この電圧を下回ると自動シャットダウン | 1050 mV |
| `CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT` | アドバタイズ中のスリープ移行時間 | 60000 ms |

本リポジトリでは `MIN_MV=1000` / `LOW_MV=900` のみ上書きしており、**`MAX_MV` はデフォルトの 1300 mV のまま**。

### 変換ロジック（`src/non_lipo_battery_management.c`）

```c
// mv -> pct（線形補間、範囲外はクランプ）
pct = 100 * (mv - CONFIG_ZMK_NON_LIPO_MIN_MV)
          / (CONFIG_ZMK_NON_LIPO_MAX_MV - CONFIG_ZMK_NON_LIPO_MIN_MV);
```

- `mv >= MAX_MV` の場合は無条件に **100% にクランプ**される。
- ADC生値→mV変換は `adc_raw_to_millivolts()`（Zephyr標準）で、ゲインは固定 `ADC_GAIN_1_6`。nRF52 SAADCの内部リファレンス0.6Vとゲイン1/6から、**外部分圧なしで最大約3.6Vまで直接測定可能**な設定。
- コード内に `output-ohms`/`full-ohms` 等の外部分圧スケーリング処理は**存在しない**（DTS bindingにもそのようなプロパティは無い。`io-channels` と任意の `power-gpios` のみ）。
- サンプリング周期はドライバ内では定義されておらず、ZMK上位層からの呼び出し依存（この点は100%固定の直接原因ではなさそう）。

### 電池構成（ユーザー確認・訂正）

build-guide.md の「乾電池2本」は**左右合わせて2本**（片側1本ずつ）の意味で、**各半体は単3電池1本のみ**（直列ではない）。使用電池はエネループ（liteではない標準タイプ、NiMH、公称1.2V）。

→ 前節の「2本直列」仮説は誤り。単セル前提の `MIN_MV`/`MAX_MV` 設計自体は方向性として正しかった。

### PCB回路図での確認（2026-07-02追加調査）

`https://github.com/sekigon-gonnoc/torabo-tsuki-lp/tree/master/pcb/M-size` の KiCad データ（`left.kicad_sch` / `right.kicad_sch`）を取得し、テキストとして直接調査した。

- 左右それぞれに単セルの電池ホルダーシンボルが1個ずつ実装（`BT101`＝左、`BT201`＝右。`lib_id "Library:Battery_Cell"`, footprint `Library:battery_holder`）。
- 両ファイルとも **`Device:R`（抵抗）のコンポーネントが1つも存在しない** → 電池からADCまでの間に**分圧回路は無い**。
- したがって、**ファームウェアがADCで読んでいる電圧 = 電池ホルダー両端の電圧そのもの**。テスターで測る場合は単純に電池ホルダー（乾電池本体）の+/-端子を測ればよい（左右別々に測定）。

### 結論（確定）

`CONFIG_ZMK_NON_LIPO_MAX_MV` が未設定でドライバのデフォルト `1300mV` のまま使われていることが直接原因。エネループ（標準）の電圧特性は：

- 満充電直後：約1.4〜1.45V
- 通常使用域（放電カーブが非常にフラット）：約1.2〜1.3V
- 減り始め（ニー）：約1.1V
- 空（保護のため遮断すべき）：約0.9〜1.0V

放電カーブがフラットなため実使用域の大半（1.2〜1.3V）が `MAX_MV=1300mV` 以上となり、線形変換式 `pct = 100*(mv-MIN_MV)/(MAX_MV-MIN_MV)`（`mv>=MAX_MV`で無条件100%クランプ）により、ほとんどの期間100%表示に張り付いていたと考えられる。

roBa/Zonkey（LiPo）は標準ZMKのLiPo用バッテリードライバ（別の電圧レンジ・変換ロジック）を使うため、この問題は起きない。

### 対応方針（ユーザー判断待ち）

エネループ標準に固定した提案値：

- `CONFIG_ZMK_NON_LIPO_MAX_MV=1450`（満充電直後を100%上限に）
- `CONFIG_ZMK_NON_LIPO_MIN_MV=1050`（現状1000→ニー付近に微調整）
- `CONFIG_ZMK_NON_LIPO_LOW_MV=950`（現状900→過放電保護、現状維持でも可）

ユーザーは「まずテスターで実測してから決めたい」と回答。

### 実測結果（2026-07-02）

満充電直後の電池電圧（電池ホルダー+/-端子で測定、左右）：**1.35V と 1.38V**。

2.（数時間〜1日使用後の落ち着いた電圧）・3.（残量が少ない時点の電圧）は自分での測定が難しいとのことで、Web上の公開データを調査した。

### Web調査：エネループの放電特性（2026-07-02追加）

Panasonic公式データシート（Cell Type BK-3MCC、PDF: <https://www.kronium.cz/uploads/BK-3MCCE.pdf>）の放電特性グラフ（25℃、充電後1h休止、400〜6000mA放電）から読み取れる情報：

- 休止後の開始電圧（400mA負荷時）：約1.4〜1.45V
- 容量の大半（〜80-90%）でほぼフラットに **1.25〜1.3V** を維持
- 容量85〜90%消費あたりから **1.1V付近で急降下** が始まる（ニーポイント）
- メーカー規定の放電終止電圧（E.V.）：**1.0V**（400mA基準の公式スペック値）

キーボードのBLE消費電流（数mAオーダー）は datasheet の最低条件（400mA）よりずっと小さいため、実際の放電カーブはこれよりさらにフラットで高電圧に張り付き、終止直前でようやく急落する形になると考えられる。

ユーザーの実測値（1.35V・1.38V）は、データシートの「休止後開始電圧（約1.4V前後）」とよく一致しており、測定値・データシートどちらも妥当性が確認できた。

参考: [Test of Eneloop AA BK-3MCCE 1900mAh (lygte-info.dk)](https://lygte-info.dk/review/batteries2012/Eneloop%20AA%20BK-3MCCE%201900mAh%20(White)%202019%20UK.html)

### BMP Boost（昇圧回路）の仕様確認（2026-07-02追加）

本機種が使用する `zmk-component-bmp-boost`（sekigon-gonnoc、`config/west.yml` 参照）のベースボード「BMP Boost」の公式README（<https://github.com/sekigon-gonnoc/BLE-Micro-Pro/blob/master/bmp-boost/README.md>）を確認した。

- 「電池一本（**0.7V以上**）で動作可能」なDC-DC昇圧コンバータ搭載のnRF52ボード。
- 昇圧後の `VCC` 出力：**約2.4V**。
- 電源コネクタ `BT` の入力仕様：**0.7〜3.3V**、`AIN4` で電圧測定可能。

→ `io-channels = <&adc 4>`（本リポジトリの`non_lipo_battery`ノード）は、この `BT` 端子＝**昇圧前の生の電池電圧**を直接AIN4で読んでいることが公式ドキュメントからも裏付けられた（先のPCB回路図調査で分圧抵抗が無いことを確認済みとも整合）。

**MIN_MV/LOW_MVへの示唆：**

昇圧回路自体は0.7V（700mV）まで動作継続可能。これは現状の `LOW_MV=900mV` よりかなり低く、**電源が落ちる/昇圧が不安定になるリスクの観点では十分な安全マージンがある**ということが確認できた。つまりこの用途での実質的な下限を決めているのは昇圧回路ではなく、**電池セル自体の特性（データシートのE.V.=1.0V、過放電による劣化）**であり、先に決めた `MIN_MV=1000` / `LOW_MV=900` は妥当（変更不要）という結論を補強する。

### 最終結論・推奨設定値

- `CONFIG_ZMK_NON_LIPO_MAX_MV=1400`（実測値1.35〜1.38Vにわずかな余裕を加えた値。要追加）
- `CONFIG_ZMK_NON_LIPO_MIN_MV=1000`（現状のまま。データシート規定の放電終止電圧E.V.=1.0Vと正確に一致しており変更不要）
- `CONFIG_ZMK_NON_LIPO_LOW_MV=900`（現状のまま。MIN_MVよりさらに低い安全マージンとして妥当）

→ 必要な変更は `torabo_tsuki_lp_left.conf` / `_right.conf` に **`CONFIG_ZMK_NON_LIPO_MAX_MV=1400` を1行追加するだけ**。

2026-07-03、`torabo_tsuki_lp_left.conf` / `_right.conf` に `CONFIG_ZMK_NON_LIPO_MAX_MV=1400` を追加済み（`MIN_MV=1000` / `LOW_MV=900` は変更なし）。

残作業: `git commit` → `git push` でCIビルド → `.uf2` を実機に書き込んで、zmk-battery-center / nRF Connect で電池残量表示が変化することを確認する（未実施）。

## 次のアクション

1. 実機でテスターを使い、(a) 電池2本の実電圧、(b) 可能であれば `non_lipo_battery` の ADC ピン（ADC ch4）の実電圧を測定する。分圧回路の有無もこの時点で判明する。
2. 測定値をもとに `CONFIG_ZMK_NON_LIPO_MIN_MV` / `MAX_MV` / `LOW_MV` を左右の `.conf` ファイルに設定する。
3. `git push` でCIビルド → `.uf2` を書き込み、zmk-battery-center またはnRF Connectで残量表示が変化することを確認する。
4. 併せて、左右どちらの表示も100%固定かを確認しておく（split proxy側の切り分けとして）。

## 実機確認結果（2026-07-11）

`MAX_MV=1400` 適用（`git push`・`.uf2` 書き込み済み）後、1週間ほど使用した状態で確認。

- zmk-battery-center表示：左右とも100%ではなくなったが、1週間で1〜2%しか減っていない → 後述の通り想定通り（バグではない）。
- 電池テスター（BT-168D）実測：左 1.30V（表示82%）、右 1.26V（表示72%）。

現在の設定（`MIN_MV=1000` / `MAX_MV=1400`）の変換式 `pct = 100*(mv-1000)/400` に当てはめると：

| 側 | テスター電圧 | 計算上の% | 実際の表示% | 差 |
| -- | ------------ | --------- | ------------ | -- |
| 左 | 1.30V | 75% | 82% | +7pt |
| 右 | 1.26V | 65% | 72% | +7pt |

左右とも同じ+7ptのズレで、ランダムではなく系統的なオフセットと考えられる。原因候補：

1. 安価なテスター（BT-168D）自体の校正誤差（±0.02〜0.05V程度はあり得る範囲で、7pt分=約28mVはこれだけで説明可能）。
2. バッテリー%通知が瞬時値ではなく、少し前のサンプリング値を反映している可能性（表示のタイムラグ）。

大小関係・オーダーは一致しており、`MAX_MV=1400` の設定自体は妥当と判断。追加のチューニングは現時点では不要と判断。

「1週間で1〜2%しか減らない」件は、エネループ（NiMH）の放電カーブが容量の80〜90%区間で1.25〜1.3Vとほぼフラットなことに起因する想定通りの挙動（電圧ベースの線形変換のため、電圧が動かない区間は%もほぼ動かない）。ニーポイント（1.1V付近）に近づくと急激に%が落ち始めるはず。バグではなく現状の設計上の制約として認識し、様子見でよい。
