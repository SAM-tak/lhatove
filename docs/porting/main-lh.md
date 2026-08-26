# main.lh の書き方

M2 時点で動く形。実例は `testing/lh/{hello,autoquit,customrun,panic,raise,badcallback,realgame}`。
実装と食い違ったらこのファイルを直す。

## 基本形

main.lh は `module^` を宣言し、コールバックを `public^let^` メンバとして公開する。
エンジンは起動時に公開メンバを集めて handlers テーブルを作り（無いものは no-op）、
既定の `run`（埋め込み Boot.lh）がフレーム毎にそれらを呼ぶ。
Lua 版の `love.update = function(dt) ... end` 代入スタイルは存在しない
（L^ に metatable は無く、`L^` 直下のテーブルは sealed のため）。

```lhat
module^ main

import^ love.graphics
import^ love.event

var^ x = 0

public^let^ load = p^ {
    # one-time setup
}

public^let^ update = p^dt:number^ {
    x := x + 60 * dt
}

public^let^ draw = p^ {
    love.graphics.rectangle("fill", x, 100, 50, 50)
}

public^let^ keypressed = p^key:string^, scancode:string^, isrepeat:bool^ {
    if^ key = "escape" { love.event.quit() }
}

public^let^ quit = p^ -> bool^ {
    return^ false^   # true^ で終了を拒否
}
```

すべてのコールバックは任意。副作用を持つので `p^`（`f^` は純粋関数）。
M1 時点のコールバック: `load update draw quit keypressed keyreleased textinput mousemoved
mousepressed mousereleased wheelmoved resize focus mousefocus visible`
（一覧は `src/lh/Boot.cpp` の `callbacks[]`）。

## run のオーバーライド

`public^let^ run = p^ { … }` を公開すると既定の run の代わりに使われる。
yieldable な `p^` で、毎フレーム `yield^` する。終了は `return^ <number^>`（終了コード）。
コールバックは `love.boot.handlers()` で得る（既定 run と同じ経路）。
`love.event.dispatch(h)` がイベントを各コールバックへ配り、quit が受理されたら終了コードを返す（それ以外は `nil^`）。

```lhat
public^let^ run = p^ {
    let^ h = love.boot.handlers()
    h.load()
    love.timer.step()
    repeat^ {
        love.event.pump()
        let^ code = love.event.dispatch(h)
        if^ code isa^ number^ { return^ code }
        h.update(love.timer.step())
        love.graphics.origin()
        love.graphics.clear()
        h.draw()
        love.graphics.present()
        yield^
    }
}
```

## conf.lton

**設定はデータ。** LTON（L^ Table Object Notation）はテーブルリテラルの**中身**をそのまま書く綴りで、
`return^ {` と `}` を書かない。love.conf のスキーマを 1:1 写像。
読まれる項目: `identity` `appendidentity` `version` `console`、`window.{title width height fullscreen
fullscreentype vsync msaa stencil depth resizable minwidth minheight borderless centered displayindex
usedpiscale refreshrate x y}`、`modules.{timer event keyboard mouse image font window graphics}`（既定 `true^`）。

```lton
# conf.lton
identity = "mygame",
window = {
    title = "My Game",
    width = 1280,
    height = 720,
    resizable = true^,
},
```

綴りは L^ のもの（コメント・文字列エスケープ・数の形は同じ字句解析器が読む）。末尾の `,` は許される。

**効果のあることは書けない。** 本文は `f^` の本体として読まれ、`f^` は `f^` しか呼べない（02 の 15.1）
ので `p^` 呼び出しは誤り。加えて `love.*` も `print` も**スコープに入らない**ので、
設定ファイルからエンジンには一切触れない。算術・比較・連結・入れ子のテーブルは通る。

同じ綴りはゲームのデータファイルにも使える。

```lhat
let^ stage = try^ std.lton.load("stages/3.lton")
```

`std.lton.load(path)` / `std.lton.parse(text)` はどちらも `f^ -> t^{}|std.lton.LtonError|std.error.OutOfMemory`。
パスは `require^` と同じく PhysFS 経由なので、ディレクトリ・`.love`・fused のどれでも同じように読める。
スクリプトを読む `std.load` と違い、読んだ結果が何かを実行することはない。

## ゲームの置き場

`love path/to/dir`（`main.lh` を含むディレクトリ）、`love game.love`（zip）、`love file.lh`、
または exe に zip を連結した fused 形式（`copy /b love.exe+game.love mygame.exe`）。
ゲーム内の別ユニットは `require^ "lib/util.lh"`（要求側からの相対パス、リテラルのみ）。
実行時に決めるパスは `love.filesystem.load(path)`（閉包を返す）か `std.load`。

## エラー表示

型検査エラーは起動前に診断として（lovec はコンソール、love.exe はメッセージボックス）。
実行時の `panic^`（ホストが検出したプログラマエラーを含む）は traceback 付きの青画面。Escape で閉じる。

## Lua 版との違い（ゲーム作者向け）

- 型チェックが実行前に走る。typo・引数違いは起動時に診断として報告される
- 文は `;` で区切らない（改行で終わる）。コメントは `#`
- 真偽値は `true^` / `false^`。真偽値以外を条件に使えない（`0` や `""` は false ではない）
- 関数の末尾の式は戻り値にならない。`return^` を書く
- 多値戻りは固定幅タプル。`let^ w, h = love.graphics.getDimensions()` のように分解束縛
- エラーは値。`pcall` は無く `try^` / `catch^` / `isa^` を使う。love.* が返しうるエラーを未処理のまま放置すると静的エラー
- 実行時の `panic^` は traceback 付きでゲームを止める（Lua の error 相当）
- LÖVE オブジェクトの `=` は同一実体なら真（別ラッパでも可）。`is^` はラッパ同一性
- `string.format` は無い。`$"x = {x}"` の補間を使う
- Lua パターン（`string.match` 等）は無い。`s.find/replace/split` の組込か `std.regex`（実正規表現のサブセット。backref/lookaround 無し）
- `table.sort` 等は組込メンバ `t.sort^()`（ハット必須）。比較関数は 3値（負/0/正）
- `math.*` 相当は `std.math`（`import^ std.math` → `std.math.sin/cos/sqrt/atan2/lerp/min/max/...`）。**角度は度数法**。`love.graphics.rotate` などラジアンを取る API へは `std.math.rad(deg)` で変換。`abs/sign/clamp/floor/ceil/round` は `number^` のメンバ（`x.clamp(lo, hi)`）、定数は `number^.pi` / `tau` / `e` / `inf` / `nan`
- 公開したコールバックの型が違うと（例 `update = p^dt:string^`）起動前に診断で止まる
- 実行時のコード読み込みは `love.filesystem.load(path)`（ゲームのファイルシステム経由）か `std.load.file/text`
- `love.physics`: 型は `World` / `Body` / `Shape` / `Joint` / `Contact` の 5 つ。`CircleShape` や `RevoluteJoint` は無く、種別は `shape.getType()` / `joint.getType()` で見分け、種別のメンバ（`setRadius`、`setMotorSpeed`…）は合う種別にだけ呼べる（違う種別へ呼ぶと panic）。`world.setCallbacks(begin, end, presolve, postsolve)` は省いた分がクリアされる。`postsolve` は `(a, b, contact, n1, t1, n2, t2)` の 7 引数固定。`queryShapesInArea` / `rayCast` / `setContactFilter` のコールバックは `p^`（`f^` は外の変数へ代入できない）。座標の列は `getPoints()` → `t^{...:number^}` で受け、`body.getWorldPoints(shape.getPoints()...)...` のように展開して渡す
- タプルを返す呼び出しは `$"..."` の補間スロットへ直接書けない（静的エラー）。`let^ x, y = body.getPosition()` で受けてから補間する
- `love.thread`: スレッド本体は `.lh` ファイル（`newThread("worker.lh")`、型検査は newThread 時）か改行入りのコード文字列。本体は `let^ args = ...` で引数表を受け、`args[1]` を `isa^` で絞る。Channel の `pop/demand/peek` は `any^` を返すので `isa^` で絞る（`if^ v isa^ t^{ x : number^ } { ... }`）。table と閉包は複製されて渡る（元と共有しない）。LOVE オブジェクトを含む table は渡せない。`performAtomic(p^c:love.thread.Channel, extra:any^ { ... }, 100)` のように extra は最大 3 つ
- `love.graphics`: Canvas は `newCanvas(w, h)` が返す Texture（`isCanvas()`）。`setCanvas(c)` / `setCanvas()`。ピクセルを読むには `love.graphics.readbackTexture(c)` → ImageData。`draw(texture, quad, x, y, ...)` は第 2 引数に Quad。`Shader.send(name, ...)` は数値列・`{...}` 表・Texture・Transform を受ける。Mesh の頂点は `{x, y, u, v, r, g, b, a}`（後ろ 6 つ省略可）
- ブロックは `do^{ ... }`。裸の `{ ... }` はテーブル literal なので文にならない。同じ名前を二度作りたい時（`known` を 2 回など）は `do^` で囲んでスコープを分ける
