# Lua → L^ 移植 設計判断記録

lhatove の Lua/LuaJIT を L^ (lhat) へ置き換えるにあたっての確定事項と対応表。
進捗は [status.md](status.md)、ゲーム作者向けの書き方は [main-lh.md](main-lh.md)。
前提 lhat: HEAD `a6c81b5` 以降（列挙は AGENT.md の「登録」節）。

## 確定した方針

1. **lhat 取込**: 兄弟ディレクトリ参照。CMake `LHATOVE_LHAT_DIR`（デフォルト `../lhat`）+ `add_subdirectory`。`LHAT_BUILD_TESTS/CLI/LSP=OFF`、`LHAT_BUILD_STDLIB=ON`、`LHAT_WITH_COMMENTS/RESOLUTIONS=OFF`
2. **Lua 撤去**: クリーンカット。wrap 層はビルドから外し参照用残置 → 移植完了後に削除
3. **バインディング様式**: 型ごと hostdata。多態はシグネチャ union、コンストラクタ多重定義は再登録=オーバーロード
4. **スクリプト形**: main.lh は `module^` + `public^let^` コールバック
5. **メインループ**: L^ 側 run コルーチン。埋め込み Boot.lh が既定 `run`（yieldable `p^`、毎フレーム `yield^`）を提供、ゲームは `public^let^ run` でオーバーライド可。C++ は毎フレーム `lhat_machine_resume`
6. **lhatstdlib**: 選別登録 — `error` / `debug` / `regex` / `load` / `math`（スカラー、度数法）。`std.io`・`std.math.vector3` は非登録（love.filesystem が担当 / LÖVE API に用途なし）。`std.thread` / `std.async` は M5 で判断

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

- **登録**（型・メンバ・関数・エラー種・アノテーション・global）は `lhat_program_check` 前に完結必須。検査器がシグネチャの意味を知る必要があるため、check 後の登録は false を返す → 全モジュール API を無条件登録し、conf はインスタンス生成のみ制御。**ユニット**の追加は別物で後からできる（`check` → `compile` は実行中 machine の下でも反復可。`std.load` / `love.filesystem.load` はこれ）
- 登録の identity（hostdata タグ・host value タグ・エラー種）は **program ではなくプロセスのもの**。「登録呼び出し＝宣言」として lhat が intern するので、program をいくつ作っても同じタグが返り、食い違う宣言（別サイズの host value 型、別の variant 並び）は拒否される。restart で program を作り直しても型 identity は変わらない。返すのは `lhat_registry_dispose()`（LhatProgram が 1 つも無い時のみ）
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
- **U2**（M1 で計測）: オブジェクト無しの段階で live ≈750、120 フレームで数百回収。hostdata が入る M2 以降に再計測
- **U3**: `require^` のパス正規化と PhysFS 区切り・`..` の整合
- **U4**（M0 で確認）: `f^ -> t^{ update : p^number^ -> nil^;, draw : p^ -> nil^; };` は登録・検査・呼び出しとも動く（`lovec --probe`）。lhat 側の構造型メンバ名 use-after-free は `10e810e` で修正済み

## M0 で確定した実装事項

- lhatstdlib ヘッダは lhat 側で `extern "C"` ガード済み（そのまま include）
- `love.Error{Misuse,IO,NotSupported}` を TYPES 相の最初に登録（`love` モジュール表を最初に作る）
- `lovec` は `LOVE_LH_CONSOLE_EXE` 定義付きでビルドし、`love_lh_boot(argc, argv, console)` にフラグを渡す。console=true では診断・エラーは stderr のみ、false（love.exe）ではメッセージボックスも出す
- 末尾呼び出しは traceback に残らない（`outer = p^ { inner() }` は表示されない）— 仕様

## M1 で確定した実装事項

- 起動列: 登録 → check(Boot.lh, main.lh) → compile → main.lh 実行（公開テーブル）→ モジュール生成（timer/event/keyboard/mouse/font/window/graphics、800x600）→ handlers 構築 → run 選択 → `lhat_machine_call(run)` でコルーチン → 毎フレーム `lhat_machine_resume`
- handlers: `src/lh/Boot.cpp` の `callbacks[]` からシグネチャ文字列を生成し `love.boot.handlers()` に登録。欠けたコールバックは variadic no-op のホスト関数（quit は `false^` を返す）
- `love.event.dispatch(h) -> number^|nil^`: C++ がイベントを poll し `handlers.<name>(typed args)` をネスト `lhat_machine_call`。quit は `handlers.quit()` が `true^` なら拒否、さもなくば終了コードを返す
- `lh::raise`: プログラマエラー用。`lhat_machine_panic_text` で run を `panic^` として終える（traceback は呼び出し箇所から）。戻り値の nil は捨てられる
- `lh::guard`: `love::Exception` → `raise`。`lh::catchexcept`: `love::Exception` → 宣言済みエラー値（fallible API 用）
- 可変長の末尾引数（描画の transform 9 数値など）は `...`、少数の省略可能引数は同名再登録のオーバーロード（`clear` 0/3/4 引数）
- enum 文字列（draw mode、align）は `string^` + `getConstant` で実行時検証、不正なら `raise`
- `src/lh/lh.h` に TypeRegistry（`love::Type* ↔ LhatHostDataTag*`、isa によるダウンキャスト）、`pushObject`/`checkObject`、`pushVariant`、`park` を実装済み（オブジェクト型の初使用は M2 の image/font）
- 起動時に `lhat_unit_export_conforms` でゲームの公開コールバックの型を `callbacks[]` の署名と照合。違えば診断を出して起動しない（`testing/lh/badcallback`）
- `std.math` を登録（度数法。`love.graphics.rotate` 等ラジアン API へは `std.math.rad(a)`）。`std.math.vector3` は非登録
- 未対応: conf.lh、Window の settings テーブル、restart、nogame、`love.graphics` のオブジェクト類

## M2 で確定した実装事項

- `Loader`（src/lh/PhysfsLoader.cpp）は held ユニット → `love.filesystem` の順に読む。boot が最初に `physfs::Filesystem` を作り `init(argv[0])` → fused 判定（`setSource(exepath)`）→ ゲームの `setSource` → identity（boot.lua と同じ導出）→ `setIdentity(identity, true)` → check
- **U3 解決**: `require^ "lib/vec.lh"` はそのまま PhysFS に届く（ディレクトリ・zip・fused で確認）
- conf.lh は check 後・compile 後に実行（テーブル）、モジュール生成と window 設定を制御。`modules.*` の既定は全 `true^`。conf.lh が無ければ既定値
- 起動列: filesystem → 登録 → check(Boot.lh, conf.lh?, main.lh) → callback 型検査 → compile → conf 実行 → モジュール生成 + window → main 実行 → handlers → run
- nogame: 引数なし or 無効パスは埋め込み `nogame_lh`（簡素版。アニメ版は M6）
- エラー画面: `src/lh/ErrorScreen.cpp`。実行時 fault/panic は `reportRuntime` → 青画面（Escape/閉じるで終了）。check 失敗は `report`（コンソール + love.exe はメッセージボックス）— window 生成前のため
- オブジェクト: File / FileData / ImageData / Drawable / Texture / Font を `ctx.objectType` で登録（dispose/type/typeOf 自動）。`draw` は当面 `love.graphics.Texture` 引数（Drawable union は他の Drawable 実装時）
- 失敗しうる API（read/write/newImage/newFont(path)/newFile/newFileData(path)）は `|love.Error.IO` を宣言し `catchexcept`。それ以外のプログラマエラーは `raise` = panic
- **可変長オーバーロードの制約**: `print(text, ...)` と `print(text, Font, ...)` は重複とみなされ登録拒否 → 1 本にして可変長尾の先頭が Font かを実行時判別（`fontInTail`）。同様の「任意位置のオブジェクト引数」は同方式
- `Runtime::failedRegistrar()` でどのレジストラが拒否されたか分かる
- 未対応: window の `getMode` settings 返却、`love.filesystem.mount/lines/enumerate`、restart、`--game` 等 arg.lua のオプション群、URI 引数

## M3 で確定した実装事項

- モジュール生成順は boot.lua どおり: filesystem → timer → event → keyboard → joystick → mouse → touch → sound → system → sensor → audio(OpenAL、失敗時 null) → image → data → font → window → graphics → math。conf.lh の `modules.*` で個別に切れる
- コールバック追加: joystick/gamepad 系（`love.joystick.Joystick` hostdata が引数）、touch 系（id は整数）、`sensorupdated`。`Variant::LUSERDATA` は整数へ変換（touch id）
- lhat `3a4376c` で解消: 自型メンバは `Self^` 綴り（`Transform` の全メンバ）、可変長アームは位置で区別（`print` = `string^` / `string^, number^, ...` / `string^, Font, ...` の3アーム）。当初の回避（`apply` の `any^`、固定アーム化）は撤去 or 任意
- ImageData のピクセル: `getPixel -> (r,g,b,a)` タプル、`setPixel(x,y,r,g,b[,a])`、`mapPixel(f^x,y,r,g,b,a -> (r,g,b,a))`（ピクセル毎にネスト呼び出し。host value 色型は見送り — タプルで足りる）
- `love.data` は文字列 in/out（ByteData 等のコンテナは必要になった時点で）
- デバッグ補助: `LHATOVE_TRACE=1`（起動列をトレース）、`LHATOVE_SKIP_REGISTRATIONS=a,b`（登録を外して二分探索）、`Runtime::failedRegistrar()` は拒否された登録名と署名を含む

## M4 で確定した実装事項

- box2d コアの脱 Lua: `World`/`Body`/`Shape`/`Joint`/`Contact` から `lua_State` と `Reference` を除去。多値返しは out 参照 or `std::vector<float>`、列挙は `std::vector<T*>`（`getContacts` は retain 済みを返す）、userData は `StrongRef<love::Object>`。World のコールバックは言語非依存の listener 型（`ContactListener::onContact(a, b, contact, impulses)` / `ContactFilterListener::shouldCollide` / `ShapeVisitor::onShape` / `RayCastVisitor::onHit`）、`rayCastAny/Closest` は `RayHit` 構造体
- `lh::ParkingLot` / `lh::Parked`（lh.h）: `Reference` の後継。`L^.modules.love.registry` テーブルの整数スロットに値を係留し、`Parked` は `love::Object` として `StrongRef` で保持できる。解放は `releaseLater` → 次の `park()`/`sweep()`（safe point）で nil 書込（dispose は collector 内で走りうるため）。`Runtime` が lot を所有し、`compile()` 後に `attach`、破棄時に `detach`
- L^ 側の型は 5 つ: `love.physics.World/Body/Shape/Joint/Contact`。Shape の種別（circle/polygon/edge/chain）と Joint の種別（distance/revolute/…）は **1 つの hostdata 型に全メンバを平坦化**し、種別外のメンバは `lh::raise`（`getType()` で判別）。理由: L^ の hostdata 型に部分型が無く、`Body.getShapes -> t^{...:Shape}` のような戻りを合併で書く負担を避ける
- コールバック: `World.setCallbacks(begin[, end[, presolve[, postsolve]]])`（引数個数のアーム。省いたものはクリア、`setCallbacks()` で全解除）、`setContactFilter(p^Shape, Shape -> bool^;)`、`queryShapesInArea(..., p^Shape -> bool^;)`、`rayCast(..., p^Shape, x, y, nx, ny, fraction -> number^;)`。callback の型は `f^` ではなく `p^`（外の変数を書く用途のため）。postsolve は `(a, b, contact, n1, t1, n2, t2)` 固定 7 引数（足りない点は 0）
- `lhat_machine_call` は `World.update` の中（b2 の step 内）から呼ぶ。fault は外側の run に伝播するので host 側は戻り値を捨てるだけ
- 可変長の座標は `...` で受け、結果の列は `t^{...:number^}`（`getPoints`、`getWorldPoints`、`getPositions`、`getCategory`）。`love.graphics.polygon` は `p^string^, ...;` に緩和（`body.getWorldPoints(shape.getPoints()...)...` の展開は固定引数を満たさないため）
- Shape の `rayCast` は `(hit:bool^, nx, ny, fraction)` タプル（タプル|nil^ の合併を避けた）。`World.rayCastAny/Closest` は `t^{ shape, x, y, nx, ny, fraction }|nil^`
- 非対応（12.0 で deprecated）: body 無しの `newCircleShape(x, y, r)` 系、`newFixture`、`Fixture:getShape`、`ChainShape:getChildEdge`、`MouseJoint` の setFrequency/setDampingRatio（コアに実装が無い）
- デバッグ補助を追加: `LHATOVE_WATCHDOG=<秒>`（フレームが止まると主スレッドのスタックを base+offset で stderr へ書き、`scripts/symbolize.c` で .pdb から名前解決。RelWithDebInfo 推奨）、`LHATOVE_GC_STATS=<n>`（n フレーム毎）、`LHATOVE_SKIP_REGISTRATIONS=love.physics.*`（前方一致）
- lhat `fea90e4` で解消: install が hostdata 型をメンバ付きテーブルとして再帰展開していた型爆発（physics の相互参照 5 型で live 265 万 → 2,477、nested call 370ms → 即時）。副産物として hostdata 引数のオーバーロード解決がタグ比較で効く。補間スロットのタプルも静的エラーになった

## M5 で確定した実装事項

- love.thread: `LhThread`（`LuaThread` の後継、`Threadable`）が OS スレッド上に専用 `LhatMachine` を作り（`lh::Runtime::spawnMachine` = `lhat_machine_new` + `lhat_program_install` + 専用 `ParkingLot`）、Program を共有して走る（std.thread と同形）。スレッド本体はファイル（`lhat_program_check` で unit 化 → 診断は newThread 時に panic）か文字列コード（`lhat_program_load_text`、Thread が proto を所有）。引数は script の `...` で受ける
- Program への書込（check / compile / load / install）は `lh::programMutex()` で直列化。実行中の machine は proto を読むだけ
- Channel の値は `lh::variantOf`: nil/bool/number/string はそのまま、hostdata は `Variant::LOVEOBJECT`、table/closure は `lhat_carry` の複製を `lh::Carried`（love::Object）で包んで `Variant` に載せ、取り出す側の machine で `lhat_uncarry`。LOVE オブジェクトを含む table は運べない（carry が拒否 → panic）
- `ParkingLot::lotOf(machine)`: 値を係留する binding は machine から lot を引く（physics のコールバック等）。`physicsBinding.lot` は廃止
- `threaderror` コールバック追加（`p^love.thread.Thread, string^`）。thread の fault は `lh::describeRun` でメッセージ化
- `performAtomic(fn, ...)` は extra 引数の個数でアーム分け（0〜3、`any^`）。可変長型の手続き型に固定引数の閉包は適合しないため
- 同位置に異なる hostdata 型を置くアームは登録で拒否される（File vs FileData）→ `p^File|FileData` の合併 1 アーム（[lhat-issues.md](lhat-issues.md) 提案）
- `std.thread` / `std.async` は登録しない
- graphics: Canvas は独立型にせず `newCanvas` が render target 付き `Texture` を返す（12.0 と同じ）。`setCanvas(...)` は 1 本の可変長アーム（Texture 列 + 省略可の depth/stencil bool）— `p^Texture, ...` と `p^t^{...:Texture}, ...` は登録で重なると判定されるため。`getScissor` は未設定時 0 の 4 組、`rayCast` 系と同じ「nil^ の代わりに値」方針
- `draw` は `p^D;` / `p^D, number^, ...;` / `p^Texture, Quad, ...;` の 3 アーム（D = Texture|Mesh|SpriteBatch|ParticleSystem|TextBatch|Video の合併）。`add(x, y, ...)` と `add(quad, x, y, ...)` も同様に先頭位置で分ける
- Shader.send は uniform の型に従う: float（数値列 or 要素ごとの number 表）、int/uint、bool、matrix（列優先の平坦表 / 行の表 / mat4 は Transform）、sampler（Texture）
- Mesh は標準頂点形式（x, y, u, v, r, g, b, a）のみ。頂点は 8 数 or 表で指定
- Video: `love.graphics.newVideo(path[, {audio = false^, dpiscale}])`。wrap_Graphics.lua と同じく同ファイルから audio Source を作って同期、無ければ DeltaSync。VideoStream 型は公開しない
- 未対応: Buffer / compute shader / drawInstanced / テクスチャ配列・立方体・3D / カスタム頂点形式 / captureScreenshot / Shader の `Buffer` uniform / ParticleSystem の setQuads 以外の細目（clone はあり）

## M6 で確定した実装事項

- restart: `love.event.restart([payload])` が `quit` メッセージを `"restart"` 付きで積み、`love.event.dispatch` が `"restart"` 文字列を返す → run が返し、`boot` は `LOVE_LH_RESTART`（Boot.h の負値）を返す。`love.cpp` の `do { love_lh_boot(...) } while (code == LOVE_LH_RESTART)` が再 boot。payload は `lh::variantOf` で Variant 化してプロセス側 static に置き、次の boot の `love.event.restartValue()` が `pushVariant` で戻す（table/closure は carry の複製、LOVE オブジェクトは拒否）
- `love.event.restartValue` は `love.event` に置く。`love` テーブル直下のホスト関数は `import^ love` を要し、それが `import^ love.graphics` 等のサブモジュール import と衝突する（「this table belongs to the machine」）ため
- ドロップからの再起動: `love.boot.restartInto(path)`（nogame.lua の `_noGameRestartInfo` 相当）が `quit "restart"` にゲームパスを添え、次の boot が `args.game` として使う。`filedropped` / `directorydropped` コールバックを追加
- Boot.lh の run は `dispatch` の答えが nil^ でなければ返す（数値 = 終了コード、`"restart"` = 再起動）
- nogame.lh: 埋め込み画像を持たない代わりに love.physics で組む — 静的アンカーから伸びる6連リンクの各リンクに文字（n o g a m e）、末端に浮力（`setGravityScale(-1.2)`）の風船、背景に流れる雲。クリックで風船を弾き、Esc で終了、ゲームをドロップで起動
- `testing/` の移植: `testing/lh/suite/` に upstream の TestSuite/TestMethod を L^ で置き直す。`lib/test.lh` が assertion（`isTrue` / `number` / `near` / `range` / `text` / `notNil`）を持ち、`tests/<module>.lh` が `public^let^ run` を公開、`main.lh` が全部呼んで pass なら exit 0・fail なら exit 1。Lua の `assertEquals(any, any)` は L^ に無い（`any^` は渡すか絞るかだけ）ので、比較は型ごとに分ける
- `require^` は **要求元のユニットからの相対パス**。`tests/x.lh` から `lib/test.lh` を読むには `require^ "../lib/test.lh"`（`..` は PhysFS 越しでも通る — U3 の残り）
- `testing/lh/suite` は M6 で全モジュールに広げた（451 チェック）。移植中に見つけて直したもの: `love.graphics.validateShader` が壊れたコードで投げる例外を捕まえていなかった（プロセスが fastfail）→ `(false, message)` を答える、`getScissor` が scissor を切った後も最後の矩形を答えていた → 0 を答える、`Shape.setCategory` / `setMask` に引数なしアームが無かった（LÖVE の「全部と衝突」）
- L^ の制約が 1 つ見つかった: 親モジュールと子モジュールを同じスコープに import できない（`love` と `love.graphics`）。もう 1 件は綴りの誤りだった — **ブロックは `do^{ ... }`**（02 の 8.7）。裸の `{ ... }` はテーブル literal なので文にならない
- lhat `995d0e8` で解消: 親と子の同時 import（`import^ love` と `import^ love.graphics`）、登録型どうしの重なり判定（`newThread` は `p^File;` / `p^FileData;` の 2 アームへ戻した）。`testing/lh/suite` は 451 チェック
