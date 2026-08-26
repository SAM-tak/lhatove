# lhat 側への報告事項

lhatove の移植中に見つかった、lhat 本体で直すべき事項。解決したら「解決済み」へ移す。
基準: lhat HEAD `daf6353`（2026-08-26）。

## 未解決

（なし）

## 提案

（なし）

## 解決済み

- 親モジュールとその子モジュールを同じスコープに import できない（`import^ love` と `import^ love.graphics`）／ 同じ位置に異なる hostdata 型を置いたアームが登録で「重なる」と拒否される → `995d0e8 fix: a namespace and one under it import together; registered types are disjoint`。lhatove 側: `newThread` を `p^File;` / `p^FileData;` の 2 アームへ戻した（実行時もタグで解決 — File・FileData 双方からスレッドが起動するのを確認）、`testing/lh/suite/tests/love.lh` は `import^ love` と `import^ love.system` を同居させた綴りに戻した。`love.event.restartValue` は移さない（`love` 直下を薄く保つ方針は変わらず妥当）
- `lhat_program_install` のランタイム型爆発（相互参照する hostdata 型をメンバ付きテーブルとして再帰展開）→ `fea90e4 fix: a registered type lowers to one nominal node, not its members`（hostdata_tag を持つ型は `LHAT_TYPE_RT_HOSTDATA` の葉 1 個。5 型×8 メンバ相互参照が test_program に pin、live < 1000）。lhatove: 起動直後 live 2,651,187 → 2,477、`testing/lh/physics` 完走 ≈10 分 → 5 秒、`m3` の mapPixel も即時。副産物: hostdata 引数のオーバーロード解決がタグ比較で効くようになった。再現 [repro/install_blowup.c](repro/install_blowup.c)
- `$"..."` 補間スロットのタプル → 同コミットで検査器が `TUPLE_MISPLACED` を静的に報告（02 の 13.8改「置けない」一覧に補間の穴を追記）。lhatove 側は `let^ gx, gy = world.getGravity()` で受ける綴りのまま
- `lhat_machine_call` の「ハング」は上記の遅さの誤認。[repro/call_arity.c](repro/call_arity.c) は 3/7 引数・コルーチン内・registry 経由で 20 万回通過する確認として残置
- `lhat_program_install` の無限ループ（自型を返すメンバ + 自型を取るメンバ、実態は 3^32 歩）→ `3a4376c fix: a host type that answers and takes itself installs`（program.c の独自型下ろしを `lhat_machine_rt_from_checked` に一本化、`lhat_machine_make_type` は廃止）。再現 [repro/install_loop.c](repro/install_loop.c) は通過。`Transform.apply` を型付きに戻した
- ホスト登録署名の `Self^` → 同コミットで `lhat_register_member` / `_hostvalue_member` の署名中の `Self^` が登録先の型に解決される。lhatove の Transform メンバは `p^self^, Self^ -> Self^;` 綴り。モジュール関数 / global では従来どおり誤り
- 可変長アームと他アームの重複判定 → 同コミットで「書かれた位置で型が交わらない、または片方に置き場の無い個数がある」なら別アーム。`print` は `p^string^;` + `p^string^, number^, ...;` + `p^string^, love.graphics.Font, ...;` の3アーム。`f(string^, ...)` と `f(string^, Font, ...)` の組は引き続き拒否（2引数の呼び出しが両方に収まるため）— 尾の前に型の交わらない位置を置く
- `lhat_type_of_text` の heap-use-after-free（構造型メンバ名がソースバッファを指したまま解放）→ `10e810e fix: a type's member names are the arena's own, not the source's`
- `stdlib/*.h` に `extern "C"` ガード無し → 追加済み。lhatove 側の包み込みは撤去
- `stdlib/math.h` のインストール漏れ → 追加済み
- ホスト関数から panic を起こす経路 → `bdf8fb9 feat: lhat_machine_panic`。`lh::raise` が `lhat_machine_panic_text` を呼ぶ（`testing/lh/raise`）
- スカラー数学関数が無い → `c7b49bf feat: std.math`（sin/cos/tan/asin/acos/atan/atan2/deg/rad/sqrt/cbrt/exp/log/log2/log10/hypot/fmod/min/max/lerp、**度数法**）、`number^` に abs/sign/clamp と `number^.pi/tau/e/inf/nan`。`Vector3` は `std.math.vector3` へ分離（`2a90e2b`）。lhatove は `std.math` のみ登録
- 公開メンバの型を C から照会 → `763c137 feat: a unit's exports answer their types`（`lhat_unit_export_type` / `lhat_unit_export_conforms`）。起動時のコールバック型検査に使用（`testing/lh/badcallback`）
