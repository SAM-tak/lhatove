# AGENT.md

このファイルは、lhatove リポジトリで AI アシスタントが守るべき実務ルールを定義します。

## プロジェクト概要

lhatove（lhat + löve）は [love2d/love](https://github.com/love2d/love) 12.0 のフォーク。
スクリプト言語を Lua/LuaJIT から自作言語 [L^ (lhat)](https://github.com/SAM-tak/lhat) に置き換える。

- エンジン本体: C++17 / CMake
- L^ ランタイム: C11 静的ライブラリ（`../lhat` を `add_subdirectory` で取込）
- 当面 Windows（megasource + MSVC）ビルドのみ対象

まだ実用に供されていないので、**後方互換性を保つ必要はない**。upstream love2d との互換も目標ではない。

移植の全体計画・設計判断は @docs/porting/lua-to-lhat.md、進捗は @docs/porting/status.md を参照。

## ビルド

megasource（love2d 公式の Windows 依存関係一括ビルドリポジトリ）を親として CMake を実行する。
`scripts/build.ps1` が megasource の取得・`megasource/libs/love` ジャンクション作成・CMake 実行まで行う。

```powershell
.\scripts\build.ps1              # Release ビルド
.\scripts\build.ps1 -Config Debug
```

L^ ランタイムの場所は CMake オプション `LHATOVE_LHAT_DIR`（デフォルト `../lhat`）。

## 移植規約

### 命名

- コアグルー層: `src/lh/`、namespace `love::lh`（LÖVE 旧来の `luax_*` ヘルパ層の役割を引き継ぐ。L^ 言語自体とは無関係の内部呼称）
- モジュール別バインディング: `src/modules/<mod>/lh_<Thing>.cpp`（`wrap_<Thing>.cpp` の隣に並置）
- ホスト関数: `lh_<name>`（static）、モジュールレジストラ: `lhopen_love_<module>(lh::Context&)`
- include: L^ のヘッダは `<lhat.h>` / `<lhat/xxx.h>`、グルー層は `"lh/lh.h"` 等

### 登録

- lhat の登録は check 前に完結が必須。レジストラは **2相**（TYPES 相で `lhat_register_type` / `lhat_register_hostdata_type`、MEMBERS 相でメンバ・関数登録）。シグネチャは既登録型しか参照できないため
- 登録 context はプロセス寿命のファイルスコープ static でよい（雛形: `../lhat/stdlib/io.c`）。lhat は「登録呼び出し＝宣言」として hostdata タグ・host value タグ・エラー種を**プロセス単位で intern** するので（`676b8d1`）、program をいくつ作っても identity は 1 つ。context の中身も program ごとに作り直す理由が無い。ただし program 固有のポインタ（`LhatProgram *` 等）を入れるなら、restart で再登録が必ず走って更新されることが前提。`lh::Errors` / `lh::TypeRegistry` も同じ理由で `Runtime` の値メンバをやめプロセス寿命にした（`lh.cpp` の `sharedErrors` / `sharedRegistry`）
- 登録が program に預けた state を返す口が `lhat_program_on_dispose`。lhatove では使わない — static context には返すものが無く、program 寿命の heap 資源は `ParkingLot` だけで `Runtime` のデストラクタが片付ける
- プロセス共有 registry は `lhat_registry_dispose()` で返す。**LhatProgram が 1 つも無い時のみ**呼べるので、呼ぶのは restart ループを抜けた後（`love_lh_shutdown()` ← `src/love.cpp`）
- C 側で保持する L^ 値は GC ルートにならない。永続値は `lhat_machine_register` で `L^.modules.love.*` に係留する
- lhatstdlib は選別登録: `error` / `debug` / `regex` / `load` / `math` / `lton`。conf は **conf.lton**（LTON = テーブルリテラルの中身）で、check も compile もされず `lhatstdlib_lton_load` が program の loader 経由で読む — 本文は `f^` として読まれるので `p^` を呼べず、`love.*` はスコープにも入らない。ゲームも `std.lton.load` で同じ綴りのデータファイルを読める。`std.io`（love.filesystem が担当）と `std.math.vector3` は登録しない。`std.thread` / `std.async` は登録しない（スレッドは love.thread。M5 で決定）
- エラー宣言はモジュールごと（04 の 2.4）。失敗しうるモジュールが TYPES 相で `ctx.errorKind(m, variants, n, out)` を呼び、`love.<module>.Error` を宣言する。variant は**何が起きたか**で命名する（`CouldNotLoad` / `ShaderFailed` / `Rejected`。「どの層が気づいたか」を表す `IO` / `Misuse` を全モジュールで共有しない）。1 variant なら葉を書かず宣言名だけでシグネチャに載る（`-> love.audio.Source|love.audio.Error`）。受け側は宣言名でも葉でも `fits^` で絞れる
- プログラマエラー（不正な enum 等）は `lh::raise` = `lhat_machine_panic_text`。失敗しうる API（IO 等）だけがエラー値をシグネチャに書く
- ホスト関数は `void`（`16caa92`）。答えは machine が渡す room に書く — `answers[0] = v; *answerCount = 1;`、タプルなら `answers[0..n]` と `*answerCount = n`。`*answerCount` は 0 で届くので `p^` と `dispose` は何もせず返る。`LHAT_MAX_TUPLE` より広い戻り値は登録が拒否されるので、room があふれることはない
- `lh::guard` / `lh::catchexcept` は void 本体を取る（答えは本体が room に書き終えている）。`catchexcept` だけ room を受け取る — 例外が起きたら書かれたものをエラー値 1 個に差し替えるため。`lh::raise` は panic なので、呼んで `return;` するだけ
- メインループは埋め込み `Boot.lh` の `run`（yieldable `p^`）。C++ は `lhat_machine_resume` を毎フレーム呼ぶだけ。optional なコールバックの解決は C++ 側の handlers 構築で行う（L^ では「あれば呼ぶ」を静的に書けない）
- 前提 lhat は HEAD `9a5d49a` 以降（`lhat_machine_panic`・`lhat_unit_export_conforms`・std.math・署名中 `Self^`・可変長アームの位置判定・登録型のランタイム型が葉 1 個・親と子の同時 import・登録型どうしは交わらない・登録の identity はプロセス単位で intern・`lhat_registry_dispose`・`lhat_program_on_dispose`・`lhat_program_invalidate`・std.lton と `lhatstdlib_lton_load`・ホスト型の親宣言・ホスト境界が count で答える・ホスト型の親宣言・DAP と `lhat_reload`）
- 自型を返す/取るメンバは `Self^` で書く（`p^self^, Self^ -> Self^;`）。オーバーロードは「書かれた位置で型が交わらない or 個数で分かれる」こと。`f(string^, ...)` と `f(string^, Font, ...)` は拒否される — 尾の前に交わらない位置を置く（`print` の3アーム参照）
- デバッガは lhat の DAP アダプタ（09 章）。`lovec --dap=PORT game/` でポートを開いて待ち、繋がってから起動列を進める。`LHATOVE_WITH_DAP`（既定 ON）で `lhatdap` をリンク。**fused では無効**（配布物がポートを開かない。`Boot.cpp` が `--dap` を捨てる）
- **love.thread のワーカーも対象**。バインディングは何もしない — `lhat_debug_watch_machines` が `lhat_machine_new` を拾うので、`Runtime::spawnMachine` が作った machine がそのまま DAP のスレッドになる（確認: `worker.lh` にブレークポイントが効き、スレッド 2/3/4 が現れる）
- ブレークポイントのパスは `DapPathMap`（09 の 5.2）で写す。`Boot.cpp` の `toUnit` / `toEditor` が PhysFS のマウント（`getRealDirectory`）を使い、エディタの絶対パス ↔ 単位の綴りを両方向に翻訳する。だから VS Code が送る絶対パスのままブレークポイントが結ばれ、スタックの `source.path` も同じ綴りで返る。`.love` や fused の中の単位はディスクに無いので `to_editor` が false を答え、単位名のまま報告される（埋め込みの `Boot.lh` も同じ）
- 起動がおかしい時: 環境変数 `LHATOVE_TRACE=1`（起動列トレース）、`LHATOVE_SKIP_REGISTRATIONS=love.x.f,love.y.T.m,love.z.*`（登録を外して二分探索。`*` で前方一致）、`LHATOVE_GC_STATS=<n>`（n フレーム毎に collected/live）
- 止まる・遅い時: `LHATOVE_WATCHDOG=<秒>` でフレームが止まった主スレッドのスタックを base+offset で stderr へ出力 → `scripts/symbolize.c`（`cl symbolize.c dbghelp.lib`）で `.pdb` から名前解決。symbols は `cmake --build build --config RelWithDebInfo --target lovec`（`SDL3.dll` / `OpenAL32.dll` を `build/SDL3/RelWithDebInfo` 等から `build/love/RelWithDebInfo` へコピー）。stderr を PowerShell のパイプに流すと書込で止まって見えるので、ファイルへリダイレクトする
- C 側が L^ の値を保持する時は `lh::Parked`（`lh::ParkingLot` の整数スロットに係留、`StrongRef<love::Object>` で持てる）。コールバックは `lhat_machine_call` をホスト関数の中から呼ぶ（fault は外側の run に伝播するので戻り値を捨てるだけでよい）
- `pushObject` は machine ごとに 1 オブジェクト 1 ラッパを返す（`lh::WrapperCache`）。同一性は lhat の等値規則が既に持っている（hostdata の `=` は tag + 実体ポインタ）ので、キャッシュの目的は churn を避けることだけ — `world.getBodies()` を毎フレーム呼ぶ類が対象。`is^` まで一致するのは副産物。**ホストが C++ 側に持つ `LhatValue` は GC ルートではない**（コレクタが辿るのは `L^` と実行フレーム）ので、Lua が `__mode = "v"` を要したのに対しこちらは素の map でよい。エントリを外すのは `dispose` で、手で呼ばれた時も sweep から呼ばれた時も同じ経路を通る
- ホスト型は親を宣言できる（`lhat_register_hostdata_subtype`、8.8改）。`lh::Context::objectType` の 5 引数版がそれで、TYPES 相で親付きに登録し MEMBERS 相は何もしない — `dispose` / `type` / `typeOf` は親から継承される。`love.graphics.Drawable` の 6 派生（Texture / Mesh / SpriteBatch / ParticleSystem / TextBatch / Video）がこの形で、`draw` のシグネチャは親 1 語。親が先に登録されている必要があるので、レジストラの中で順序を作る（`lhopen_love_graphics` が Drawable を登録してから `lhGraphicsMesh` 等を呼ぶ）
- 親の宣言は**ポインタについての約束**。lhatove では自明に成り立つ — hostdata が持つのは常に `love::Object *`（`pushObject` がそれを取る）で、ポインタは 1 種類しか無い
- physics も同じ形。Shape の下に CircleShape / PolygonShape / EdgeShape / ChainShape、Joint の下に 11 種。`pushShape` / `pushJoint` が `getType()` で実際の `love::Type` を選ぶので、L^ に届く値は種別そのもの。種別依存メンバはその型にだけ登録する — box2d は「クラスを共有せずメンバを共有する」（stiffness は distance/mouse/weld/wheel）ので、C++ の木が 1 度で言えないものは種別ごとに登録する（`jointMember` ヘルパ）

### 旧 Lua コード

`wrap_*.cpp` / `runtime.cpp` / `Reference.cpp` / `LuaThread` / `boot.lua` / `callbacks.lua` / `nogame.lua` と luasocket / enet / lua53 / luahttps は M6 で削除済み（`git log -- src/modules/*/wrap_*.cpp` で読める）。対訳元が要る時は upstream の love2d/love 12.0 か、このリポジトリの履歴を見る。

`testing/*.lua`（upstream のテストスイート）は L^ への移植元として残置。

### コメント・コミット

新しく書くソースコードのコメント（`.cpp` / `.h` / スクリプト / CMake）は英語。日本語で書かない。
既存の日本語・英語コメントはそのまま残す。

コミットメッセージは conventional commits（`feat:` / `fix:` / `refactor:` …）。

## その他のリソース

### lhat

L^ 言語本体。埋め込み API は `include/lhat.h`。最重要参照:

- `include/lhat/program.h` / `vm.h` — 登録・実行契約
- `tests/install_smoke/host.c` — 最小ホスト
- `godot/src/` — Godot GDExtension ポート（最良の参照実装）
- `stdlib/io.c` — C モジュール登録の雛形

@../lhat/

### megasource

love2d 公式 Windows 依存ビルド。`scripts/build.ps1` が `../megasource` に clone する。

> `git clone https://github.com/love2d/megasource.git`

## 動作確認（M6 時点）

lhat 側で直すべき事項は @docs/porting/lhat-issues.md に記録する。

```powershell
.\build\love\Release\lovec.exe                      # 引数なし: nogame 画面（物理で振れる n o g a m e。ゲームを窓へドロップで起動、Esc で終了）
.\build\love\Release\lovec.exe --dump-host-api      # lhat-host.json（LSP 用ホスト API）を書き出す
.\build\love\Release\lovec.exe --dap=41300 testing\lh\autoquit   # DAP: ポート 41300 で待つ。VS Code の絶対パスでブレークポイントが結ばれる（.love の中なら単位名 "main.lh"）
.\build\love\Release\lovec.exe testing\lh\realgame   # conf.lton・require^・画像・フォント・セーブ dir、exit=4（.love / fused exe でも同じ）
.\build\love\Release\lovec.exe testing\lh\m3         # audio/sound/data/math/system/touch/sensor/joystick/ImageData ピクセル、exit=6
.\build\love\Release\lovec.exe testing\lh\physics    # box2d: 接触コールバック4種・filter・query・rayCast・joint・userData、exit=7（約 5 秒）
.\build\love\Release\lovec.exe testing\lh\thread     # love.thread: file/code スレッド・Channel（table/closure の carry）・performAtomic・threaderror、exit=8
.\build\love\Release\lovec.exe testing\lh\shader     # Canvas 読み戻し・Shader uniform・Quad/Mesh/SpriteBatch/ParticleSystem/TextBatch・状態系・Video、exit=9
.\build\love\Release\lovec.exe testing\lh\restart    # love.event.restart + restartValue、2 周目で exit=10
.\build\love\Release\lovec.exe testing\lh\suite      # L^ 版テストスイート（バインド済み全モジュール、451 チェック）、pass=0 / fail=1
.\build\love\Release\lovec.exe testing\lh\hello      # 矩形が動く。Esc で終了
.\build\love\Release\lovec.exe testing\lh\autoquit   # 90 フレームで自動終了、exit=3
.\build\love\Release\lovec.exe testing\lh\customrun  # run オーバーライド、exit=5
.\build\love\Release\lovec.exe testing\lh\panic      # update 内 panic^ → traceback
.\build\love\Release\lovec.exe testing\lh\raise      # 不正な draw mode → ホスト発 panic
.\build\love\Release\lovec.exe testing\lh\badcallback # update の型違い → 起動前に診断
$env:LHATOVE_GC_STATS=120; .\build\love\Release\lovec.exe testing\lh\customrun # 120 フレーム毎に GC 統計（collected / live）
```
