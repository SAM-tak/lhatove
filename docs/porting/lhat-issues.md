# lhat 側への報告事項

lhatove の移植中に見つかった、lhat 本体で直すべき事項。解決したら「解決済み」へ移す。
基準: lhat HEAD `e4fef4d`（2026-08-22）。

## 未解決

（なし）

## 提案

### ホスト関数から panic を起こす経路（M1 で必要性確定）

LÖVE API の大半は「プログラマエラー（不正な引数・未ロードのモジュール・GL 状態異常）でしか失敗しない」。
これらを戻り値の `|love.Error.Misuse` で表すと、`love.graphics.rectangle(...)` を呼ぶたびに
L^ 側で `catch^` が必須になり（未処理は静的エラー）、ゲーム API として成立しない。
Lua 版はこの種を `lua_error`（= panic 相当）で扱っていた。

現状ホストにできるのは「宣言と違う値（nil / エラー値）を返す」だけで、検証結果:

- 次に値を使う命令で `TYPE_ERROR` fault になる（traceback は出る）が、**ホストが言いたかったメッセージは消える**
- `p^`（戻り値を使わない）では黙って続行する

欲しいもの（案）: `bool lhat_machine_panic(LhatMachine *machine, LhatValue value)` — ホスト関数の中から呼ぶと
pending な fault を立て、ホスト関数が返った直後に VM が `LHAT_RUN_PANIC` として run を終える
（ネスト fault 伝播と同じ仕掛け）。`value` は文字列かエラー値。traceback はホスト関数を呼んだフレームから。

lhatove 側の暫定: `lh::raise(machine, message)` が stderr に出して nil を返す。

### `std.math` の扱い — スカラー数学関数が無い（M1 で確定）

現状: `std.math` は実験的な `Vector3` host value のみ。`number^` の組込は `floor/ceil/round/eq` の4つ。
**sin/cos/sqrt/abs/min/max/pi が言語のどこにも無い。** Lua ゲームは `math.*` を初日から使う
（角度・距離・クランプ・補間）。lhatove の love.math（乱数・ノイズ・Transform・bezier）は
Lua 版でも `math.*` の代替ではなく、エンジン側に寄せるべきものではない。

提案:

1. **`std.math` をスカラー数学の置き場にする**（Lua の `math` 相当）。`f^number^ -> number^;` 系:
   `abs sqrt pow exp log log2 log10 sin cos tan asin acos atan atan2 sinh cosh tanh fmod`、
   `min max`（`f^number^, number^, ... -> number^;`）、`clamp(x, lo, hi)`、`lerp(a, b, t)`、`sign`、
   定数 `pi`、`huge`（+inf）、`epsilon`。`floor/ceil/round` は `number^` 組込のまま（重複登録はしない）。
   乱数は `std.random` に既にある。整数/実数の分割（number^ は int と real の2表現）は
   「全部 real を返す、`floor` 系だけ int」で Lua 5.3 と同じ扱い
2. **`Vector3` は `std.math` から外す**。host value の実例として価値はあるので `std.vector`（Vector2/3/4 + 演算子）に移すか、
   `stdlib/` 直下ではなく `sample/` へ。lhatove は使わない（LÖVE API は x, y を別引数で渡す設計）
3. 置き場の別案: `number^` の組込メンバ（`x.sqrt` / `x.sin`）。L^ の流儀には合うが、Lua 経験者の期待
   （`math.sqrt(x)`）と `atan2(y, x)` / `min(a, b, c)` のような多引数関数の据わりが悪い → 1 を推奨

### ゲームの公開メンバの型を C から照会したい（M2 で対応を検討）

handlers 構築時、ゲームが公開した `update` が `p^number^;` でない（例: `p^dt:string^`）と
実行時に TYPE_ERROR になる。`lhat_unit_member` でメンバの型文字列（`.signature` 相当）が取れれば
起動時に静的に弾ける。godot ポートが annotation で使っている経路に型文字列があるか未確認。

## 解決済み

- `lhat_type_of_text` の heap-use-after-free（構造型メンバ名がソースバッファを指したまま解放）→ `10e810e fix: a type's member names are the arena's own, not the source's`
- `stdlib/*.h` に `extern "C"` ガード無し → 追加済み。lhatove 側の包み込みは撤去
- `stdlib/math.h` のインストール漏れ → 追加済み
