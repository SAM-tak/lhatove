# Lua → L^ 移植 設計判断記録

lhatove の Lua/LuaJIT を L^ (lhat) へ置き換えるにあたっての確定事項と対応表。
進捗は [status.md](status.md)、ゲーム作者向けの書き方は [main-lh.md](main-lh.md)。
前提 lhat: HEAD `ad39df0` 以降（ホスト駆動コルーチン・traceback・hostdata 等値・carry・std.load 入り）。

## 確定した方針

1. **lhat 取込**: 兄弟ディレクトリ参照。CMake `LHATOVE_LHAT_DIR`（デフォルト `../lhat`）+ `add_subdirectory`。`LHAT_BUILD_TESTS/CLI/LSP=OFF`、`LHAT_BUILD_STDLIB=ON`、`LHAT_WITH_COMMENTS/RESOLUTIONS=OFF`
2. **Lua 撤去**: クリーンカット。wrap 層はビルドから外し参照用残置 → 移植完了後に削除
3. **バインディング様式**: 型ごと hostdata。多態はシグネチャ union、コンストラクタ多重定義は再登録=オーバーロード
4. **スクリプト形**: main.lh は `module^` + `public^let^` コールバック
5. **メインループ**: L^ 側 run コルーチン。埋め込み Boot.lh が既定 `run`（yieldable `p^`、毎フレーム `yield^`）を提供、ゲームは `public^let^ run` でオーバーライド可。C++ は毎フレーム `lhat_machine_resume`
6. **lhatstdlib**: 選別登録 — `error` / `debug` / `regex` / `load`。`std.io` は非登録（love.filesystem が担当）。`std.thread` / `std.async` は M5、`std.math` は love.math 設計時に判断

## 対応表

| Lua 時代 | L^ 移植後 |
| --- | --- |
| `luax_*`（src/common/runtime.h、約60関数） | `love::lh` 名前空間（src/lh/lh.h） |
| `Proxy` userdata + 弱値テーブル同一性 + `__gc` | 型ごと hostdata + `dispose` メンバ（`tag->release` → `Object::release()`）。同一性は fresh-wrapper-per-push + lhat の等値規則（`=` は tag+実体ポインタ、テーブルキーも畳まれる） |
| `Reference`（luaL_ref） | `lh::Parked`（`L^.modules.love.registry` テーブル + 整数フリーリスト、RAII） |
| `love::Variant`（スレッド間輸送） | Channel は `lhat_carry/uncarry`（テーブル・閉包可）。love オブジェクトは C ポインタ再鋳造で拡張。Variant は SDL イベント/restart 用に縮退 or 廃止（M5 で判断） |
| `luaopen_love_<mod>` + `luaL_Reg` | `lhopen_love_<mod>(lh::Context&)` 2相登録（TYPES → MEMBERS） |
| boot.lua の coroutine 駆動 `love.run` | Boot.lh の `run`（yieldable p^）を `lhat_machine_call` でコルーチン化、C++ が毎フレーム `lhat_machine_resume`。終了は `return^`（nil/number = 終了コード、"restart" = 再起動） |
| `love.handlers` + `love.run` 内のイベント分岐 | C++ が handlers テーブルを構築（ゲーム公開メンバ or Boot.lh の no-op 既定）、`love.event.dispatch(handlers)` ホスト関数がネスト `lhat_machine_call` で型付き配送 |
| conf.lua | conf.lh（テーブルを返す素のユニット。love.conf スキーマを 1:1 写像） |
| `lua_newstate` per love.thread | 1 Program 共有 + OS スレッド毎 machine（`std.thread` 方式）。thread ユニットの check/compile はメインスレッド |
| pcall / error / errorhandler | エラーは値（`love.Error{Misuse,IO,NotSupported}`）。C++ 例外は `lh::catchexcept` でエラー値化。fault/panic は `lhat_machine_traceback` → ネイティブエラー画面 |
| package.loaders + PhysFS | `LhatProgramLoader` 実装（src/lh/PhysfsLoader.cpp）。PhysFS は 1 名前空間なので 1 program = 1 loader 制約と適合 |
| `love.filesystem.load` / loadstring | `lhat_program_load_text` + `lhat_machine_adopt_script`。スクリプトからは `std.load.file/text` も可 |
| 多値戻り (`getPosition() -> x, y`) | 型付きタプル `-> (number^, number^)`（`lhat_make_tuple`） |
| `string.*` / `table.*` | 組込メンバ（`s.find/replace/split/…`、`t.sort^/push^/join^/…`）。`string.format` → `$""` 補間。Lua パターン → `std.regex`（実正規表現サブセット） |
| enum 文字列（"fill"/"line"） | 当面 `string^` のまま既存 StringMap で実行時検証 |
| LuaJIT FFI 高速化 shim（wrap_*.lua） | 廃止。ImageData = hostdata + ピクセル host value 型（フィールド直読み書き）+ C 側 `mapPixel` 一括メンバ |
| luasocket / enet / luahttps / lua53 backport | 恒久廃止（将来 std.* 代替を検討） |
| nogame.lua (+ auto.lua hex ヘッダ) | 埋め込み nogame.lh（loader の hold パターンで供給） |

## 重要な制約（lhat 側の性質）

- 全登録は `lhat_program_check` 前に完結必須 → 全モジュール API を無条件登録し、conf はインスタンス生成のみ制御
- GC ルートは `L^` と実行フレームのみ。C 保持値は非ルート → `lhat_machine_register` で係留。到達済みテーブルへは `lhat_machine_table_set`（write barrier）
- 「全引数変換 → 呼ぶ → 答え変換」規律（変換と実行を交錯させない）
- Boot.lh は「あれば呼ぶ」を静的に書けない（`t.foo` は型に無ければ静的エラー、`t[k]` は合併|nil^ で絞れない）→ optional コールバックは C++ 境界（handlers 構築）で解決
- 弱値キャッシュ無し（fresh ラッパ churn = U2）。バイト配列型無し
- エラー値は traceback を自己記録しない（fault/panic のみ）。末尾呼び出しは痕跡なし、fault 時 `finally^` 不実行
- `std.load` は同時 1 machine（スレッドからの load 禁止）。carry 産閉包は proto を借用 → program 破棄をまたぐ restart ペイロードに閉包不可
- 純インタプリタ（JIT 無し）
- machine 破棄は program 破棄・モジュール shutdown より先（GPU リソースのデストラクタは Graphics 生存中に実行必須）
- dispose コールバック内での lhat API 再入は禁止

## 未解決事項（M0/M1 で検証）

- **U1**（M0 で確認）: シグネチャに宣言したエラー種（`f^ -> number^|love.probe.Error.Failed;`）をホストが `lhat_machine_make_error` で返し、L^ 側 `catch^` で受けられる。未宣言のエラー値を返す経路は試していない → 各バインディングは返しうるエラー種をシグネチャに必ず書く
- **U2**: 60fps での fresh ラッパ GC 負荷
- **U3**: `require^` のパス正規化と PhysFS 区切り・`..` の整合
- **U4**（M0 で確認）: `f^ -> t^{ update : p^number^ -> nil^;, draw : p^ -> nil^; };` は登録・検査・呼び出しとも動く（`lovec --probe`）。ただし lhat 側に構造型メンバ名の use-after-free があり結果が非決定的 → [lhat-issues.md](lhat-issues.md) #1 の修正が M1 の前提

## M0 で確定した実装事項

- lhatstdlib ヘッダは `extern "C" {}` で包んで include（ガード無し）
- `love.Error{Misuse,IO,NotSupported}` を TYPES 相の最初に登録（`love` モジュール表を最初に作る）
- `lovec` は `LOVE_LH_CONSOLE_EXE` 定義付きでビルドし、`love_lh_boot(argc, argv, console)` にフラグを渡す。console=true では診断・エラーは stderr のみ、false（love.exe）ではメッセージボックスも出す
- 末尾呼び出しは traceback に残らない（`outer = p^ { inner() }` は表示されない）— 仕様
