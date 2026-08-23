# lhat 側への報告事項

lhatove の移植中に見つかった、lhat 本体で直すべき事項。解決したら「解決済み」へ移す。
基準: lhat HEAD `9adc06d`（2026-08-24）。

## 未解決

- **親モジュールとその子モジュールを同じスコープに import できない**: `import^ love` と `import^ love.graphics` を同じユニットに書くと「this name is already defined in this scope: love」。LÖVE の API は `love.getVersion()` と `love.graphics.*` を同じファイルで使うのが普通なので、ゲーム側で必ず当たる。lhatove の回避: `love` テーブル直下のホスト関数は `getVersion` だけにし、他は必ず子モジュールへ置く（`love.event.restartValue` など）。テストも `testing/lh/suite/tests/love.lh` だけ `import^ love` にして分離。期待: 子モジュールを import した後でも親の直下メンバーが読めること

## 提案

- 同じ位置に **異なる hostdata 型** を置いたアーム（`newThread(love.filesystem.File)` と `newThread(love.filesystem.FileData)`）が登録で「重なる」と拒否される。**`9adc06d` でも再現**（2 アームに戻すと `love.thread.newThread : p^love.filesystem.FileData -> love.thread.Thread;` が MEMBERS 相で拒否）。原因は検査器側: `type.c` の `disjoint_in` が host **value** 型（8.9）はタグ比較で分けるのに、host **data** 型（8.8）は `LHAT_TYPE_TABLE` のまま構造で見るため、「共有する名前が一つも無ければ重なる」規則に落ちて `File` と `FileData`（`type` / `typeOf` / `getSize` を共有）が交わると判定される。`LhatType` は `v.table.hostdata_tag` を持っているので、`LHAT_TYPE_HOSTVALUE` と同じ 2 行（タグが違えば交わらない）で足りるはず。`fea90e4` が実行時の解決をタグにしたのと同じ理屈。lhatove は `p^File|FileData -> Thread;` の合併 1 アームで回避（実害なし）
  - 補足: ホスト登録側に `overload^` に当たる印は無い（`lhat_register_func` を同名で複数回呼ぶのがオーバーロード）。`overload^` は `def^` 内のメンバ用で、しかも 14.12 の「重なりの禁」はそのまま効くので、印を足しても上記は通らない

## 解決済み

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
