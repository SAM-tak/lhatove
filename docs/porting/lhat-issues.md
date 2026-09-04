# lhat 側への報告事項

lhatove の移植中に見つかった、lhat 本体で直すべき事項。解決したら「解決済み」へ移す。
基準: lhat HEAD `f43f8b1`（2026-09-05）。

## 未解決

- **`a8d91a1 perf: reaching L^.modules stops making a string to throw away` が physics を壊す。**
  `testing/lh/physics` が**約半分の実行で落ちる**（exit=1）。症状は 2 通りで、どちらも
  「値が壊れている」形:
  - `line 139/151: panic^: Expected a Body` — `body.getShapes()` のようなメンバ呼び出しが
    **引数 0 個**で届く（`checkAt` の `index < count` が偽。`checkObject` には入っていない）。
    直前の `typeof^(body).signature` は `love.physics.Body` と答えるので、値自体は Body
  - `line 150: an instruction was given the wrong type`

  **二分探索済み**（lhatove を各 lhat コミットに対してビルドし、physics を 8 回ずつ）:

  - `f81d426`（`a8d91a1` の 4 つ前）: 8/8 通過
  - `69e598d`: 8/8 通過
  - `28360b7`（std.task）: 8/8 通過
  - **`a8d91a1`: 7/8 失敗**
  - `f43f8b1`（現 HEAD）: 同様に失敗

  lhatove 側は無関係と確認済み — `lh::WrapperCache` の掃除を外しても再現し、キャッシュが
  released なラッパを返している事実も無い（計測して 0 件）。ただし**キャッシュを丸ごと切ると
  6/6 通る**ので、値が作られる回数と収集の間合いに依存する。

  読んで気づいた 2 点（当たりかは lhat 側で）:
  - `LhatMachine` に `LhatTable *modules` が増えたが、コミットは `src/gc.c` を触っていない。
    すぐ上の `self_key` は「`gc.c` の `mark_roots` が印を付ける」とコメントしている
  - `lhat_table_get_bytes`（`src/object.c:1789`）は**ハッシュ部の線形走査**で、置き換えた
    `lhat_table_get` → `table_get_in` が見る**配列部も継承も見ない**。`reach_table` が
    取り違えて「無い」と答えると、その場に**新しい空テーブルを作って既存を上書きする**

## 提案

（なし）

## 解決済み

- 呼び出し文の直後の `try^{ }` が命令モードの呼び出しに読まれる → `f43f8b1 fix: a call statement no longer swallows the word that opens the next one`。`test.begin("std.channel")` を `try^{ }` の外へ戻した（`testing/lh/suite/tests/thread.lh`）
- ワーカーの失敗文が panic の中身を落とす → `stdlib/thread.c` の `failure_text` が `fault_text` と `fault_line` を綴るようになった。`threaderror` が `panic^: "boom from a thread" (line 75)` と言う（以前は `panic^` の 1 語）

- DAP のヘッダに `extern "C"` ガードが無い／パス照合がディスク上の実パス前提 → `781b2cb fix: the adapter speaks the host's paths, and the headers speak C++`。ガードは `dap/*.h` に加え `transport/transport.h`・`port/socket.h`・`port/thread.h` にも入った（lhatove 側の `extern "C" { #include }` 包みは撤去）。照合は `DapPathMap`（`to_unit` / `to_editor` の双方向、NULL で従来どおり）になり、`_fullpath`/`realpath` を通らなくなった。lhatove は PhysFS のマウント（`getRealDirectory`）で写像を書き、VS Code が送る絶対パスのままブレークポイントが結ばれ、スタックの `source.path` も同じ綴りで返る。`.love` の中の単位はディスクに無いので `to_editor` が false を答え、単位名のまま報告される

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
