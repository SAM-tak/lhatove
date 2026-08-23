# lhat 側への報告事項

lhatove の移植中に見つかった、lhat 本体で直すべき事項。解決したら「解決済み」へ移す。
基準: lhat HEAD `3a4376c`（2026-08-23）。

## 未解決

- **`lhat_program_install` が、検査済み型を名指す箇所ごとにランタイム型を作り直す（共有しない）**。相互参照する hostdata 型（love.physics の World/Body/Shape/Joint/Contact — `Body.getShapes -> Shape`、`Shape.getBody -> Body`、`Joint.getBodies -> (Body, Body)`…）で生成数が爆発し、lhatove は physics 登録だけで起動直後 **live 265 万オブジェクト**（physics を外すと 1,491）。全オブジェクトが L^ から到達可能なので毎回の collection が全走査になり、`World.update` 内の nested `lhat_machine_call` が 1 回 370ms（`LhatRunResult.live` 2,651,118）。再現 [repro/install_blowup.c](repro/install_blowup.c): 5 型 × 8 メンバ相互参照で live 6,124,940、4×4 で 20,290、3×4 で 4,852、相互参照を `number^` に置き換えると 140。`3a4376c` は終了するようにはなったが、生成量は依然指数的。期待: 検査済み型 1 つにつきランタイム型 1 つ（memo/hash-cons）。lhatove 側の暫定回避は無し（physics のシグネチャは相互参照が本質）。影響は physics に限らない: `testing/lh/m3` の `ImageData.mapPixel`（64×64 = 4096 回の nested call）も修正まで 20 分超かかる（`LHATOVE_SKIP_REGISTRATIONS=love.physics.*` なら 1 秒）
- `lhat_machine_call` 自体は 3 引数でも 7 引数でも、コルーチン内・深い入れ子・registry 経由の closure でも正常（[repro/call_arity.c](repro/call_arity.c) 20 万回通過）— 上記の遅さをハングと誤認したもの
- **`$"..."` 補間にタプルを返す呼び出しを書くと、検査は通るが実行時に fault する**: `$"gravity={world.getGravity()}"`（`f^self^ -> (number^, number^)`）が check を通過し、実行時に「this call and what it called disagree on how many values come back」で止まる。13.8改ではタプルは引数位置に置けない（`pack^` か `...`）ので、補間スロットも同じ静的エラーにしてほしい。回避: `let^ gx, gy = world.getGravity()` で受けてから補間（`testing/lh/physics/main.lh`）

## 提案

（なし）

## 解決済み

- `lhat_program_install` の無限ループ（自型を返すメンバ + 自型を取るメンバ、実態は 3^32 歩）→ `3a4376c fix: a host type that answers and takes itself installs`（program.c の独自型下ろしを `lhat_machine_rt_from_checked` に一本化、`lhat_machine_make_type` は廃止）。再現 [repro/install_loop.c](repro/install_loop.c) は通過。`Transform.apply` を型付きに戻した
- ホスト登録署名の `Self^` → 同コミットで `lhat_register_member` / `_hostvalue_member` の署名中の `Self^` が登録先の型に解決される。lhatove の Transform メンバは `p^self^, Self^ -> Self^;` 綴り。モジュール関数 / global では従来どおり誤り
- 可変長アームと他アームの重複判定 → 同コミットで「書かれた位置で型が交わらない、または片方に置き場の無い個数がある」なら別アーム。`print` は `p^string^;` + `p^string^, number^, ...;` + `p^string^, love.graphics.Font, ...;` の3アーム。`f(string^, ...)` と `f(string^, Font, ...)` の組は引き続き拒否（2引数の呼び出しが両方に収まるため）— 尾の前に型の交わらない位置を置く
- `lhat_type_of_text` の heap-use-after-free（構造型メンバ名がソースバッファを指したまま解放）→ `10e810e fix: a type's member names are the arena's own, not the source's`
- `stdlib/*.h` に `extern "C"` ガード無し → 追加済み。lhatove 側の包み込みは撤去
- `stdlib/math.h` のインストール漏れ → 追加済み
- ホスト関数から panic を起こす経路 → `bdf8fb9 feat: lhat_machine_panic`。`lh::raise` が `lhat_machine_panic_text` を呼ぶ（`testing/lh/raise`）
- スカラー数学関数が無い → `c7b49bf feat: std.math`（sin/cos/tan/asin/acos/atan/atan2/deg/rad/sqrt/cbrt/exp/log/log2/log10/hypot/fmod/min/max/lerp、**度数法**）、`number^` に abs/sign/clamp と `number^.pi/tau/e/inf/nan`。`Vector3` は `std.math.vector3` へ分離（`2a90e2b`）。lhatove は `std.math` のみ登録
- 公開メンバの型を C から照会 → `763c137 feat: a unit's exports answer their types`（`lhat_unit_export_type` / `lhat_unit_export_conforms`）。起動時のコールバック型検査に使用（`testing/lh/badcallback`）
