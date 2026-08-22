# lhat 側への報告事項

lhatove の移植中に見つかった、lhat 本体で直すべき事項。解決したら「解決済み」へ移す。
基準: lhat HEAD `763c137`（2026-08-23）。

## 未解決

### `lhat_program_install` が無限ループ（自型を返すメンバ + 自型を取るメンバ）

hostdata 型 T に「T を返すメンバ」（`translate : p^self^, number^, number^ -> m.T;`）と
「T を引数に取るメンバ」（`apply : p^self^, m.T -> m.T;`）が共存すると `lhat_program_install` が返らない。
どちらか一方だけなら正常。T の実行時型を `translate` の戻り値のために構築 → T のメンバを辿る → `apply` の
引数 T → また T を構築 … と、構築中の型を覚えていないための循環。

再現: [repro/install_loop.c](repro/install_loop.c)（lhat.lib + lhatport.lib とリンクして実行 → `installing...` で停止）。

lhatove 側の暫定: `love.math.Transform.apply` の引数を `any^` にして実行時検査（lhat 修正後に戻す）。

## 提案

### 可変長アームと他アームの重複判定

`host_arms_overlap`（program.c）は「可変長アームは全てと重複」と単純化しているため、
`newSoundData(path:string^)` と `newSoundData(samples:number^, ...)` のように先頭引数の型で明らかに
区別できる組も登録拒否される。先頭から順に disjoint な位置があれば区別可、とできると
LÖVE 風の API（`print(text, ...)` / `print(text, font, ...)` など）がそのまま書ける。
現状 lhatove は可変長尾の実行時判別（`fontInTail`）や固定アーム化で回避中

## 解決済み

- `lhat_type_of_text` の heap-use-after-free（構造型メンバ名がソースバッファを指したまま解放）→ `10e810e fix: a type's member names are the arena's own, not the source's`
- `stdlib/*.h` に `extern "C"` ガード無し → 追加済み。lhatove 側の包み込みは撤去
- `stdlib/math.h` のインストール漏れ → 追加済み
- ホスト関数から panic を起こす経路 → `bdf8fb9 feat: lhat_machine_panic`。`lh::raise` が `lhat_machine_panic_text` を呼ぶ（`testing/lh/raise`）
- スカラー数学関数が無い → `c7b49bf feat: std.math`（sin/cos/tan/asin/acos/atan/atan2/deg/rad/sqrt/cbrt/exp/log/log2/log10/hypot/fmod/min/max/lerp、**度数法**）、`number^` に abs/sign/clamp と `number^.pi/tau/e/inf/nan`。`Vector3` は `std.math.vector3` へ分離（`2a90e2b`）。lhatove は `std.math` のみ登録
- 公開メンバの型を C から照会 → `763c137 feat: a unit's exports answer their types`（`lhat_unit_export_type` / `lhat_unit_export_conforms`）。起動時のコールバック型検査に使用（`testing/lh/badcallback`）
