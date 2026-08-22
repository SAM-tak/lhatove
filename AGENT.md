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
- 登録 context は per-registration の構造体を渡す。ファイルスコープ static 変数は使わない（雛形: `../lhat/stdlib/io.c`）
- C 側で保持する L^ 値は GC ルートにならない。永続値は `lhat_machine_register` で `L^.modules.love.*` に係留する
- lhatstdlib は選別登録: `error` / `debug` / `regex` / `load` / `math`。`std.io`（love.filesystem が担当）と `std.math.vector3` は登録しない。`std.thread` / `std.async` は M5 で判断
- プログラマエラー（不正な enum 等）は `lh::raise` = `lhat_machine_panic_text`。失敗しうる API（IO 等）だけがエラー値をシグネチャに書く
- メインループは埋め込み `Boot.lh` の `run`（yieldable `p^`）。C++ は `lhat_machine_resume` を毎フレーム呼ぶだけ。optional なコールバックの解決は C++ 側の handlers 構築で行う（L^ では「あれば呼ぶ」を静的に書けない）
- 前提 lhat は HEAD `ad39df0` 以降（`lhat_machine_set_modules` / `LhatModule` 廃止後の API）

### 旧 Lua コード

`wrap_*.cpp` / `runtime.cpp` / `Reference.cpp` / `*.lua` はビルドから除外済みの参照用残置。移植の対訳元として読む。編集・復活はしない。移植完了後に削除する。

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

## 動作確認（M0 時点）

```powershell
.\build\love\Release\lovec.exe                 # 埋め込み hello（print / タプル / U1）
.\build\love\Release\lovec.exe --probe         # U4 probe（構造型戻り値）
.\build\love\Release\lovec.exe path\to\gamedir # gamedir\main.lh を実行（stdio 読込、PhysFS は M2）
```

lhat 側で直すべき事項は @docs/porting/lhat-issues.md に記録する。

## 動作確認（M1 時点）

```powershell
.\build\love\Release\lovec.exe testing\lh\hello      # 矩形が動く。Esc で終了
.\build\love\Release\lovec.exe testing\lh\autoquit   # 90 フレームで自動終了、exit=3
.\build\love\Release\lovec.exe testing\lh\customrun  # run オーバーライド、exit=5
.\build\love\Release\lovec.exe testing\lh\panic      # update 内 panic^ → traceback
.\build\love\Release\lovec.exe testing\lh\raise      # 不正な draw mode → ホスト発 panic
.\build\love\Release\lovec.exe testing\lh\badcallback # update の型違い → 起動前に診断
$env:LHATOVE_GC_STATS=1; .\build\love\Release\lovec.exe testing\lh\customrun   # 120 フレーム毎に GC 統計
```
