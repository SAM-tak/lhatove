# main.lh の書き方

M1 時点で動く形。実例は `testing/lh/{hello,autoquit,customrun,panic}/main.lh`。
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

## conf.lh（M2 予定・未実装）

テーブルを返す素のユニット（`module^` なし）。love.conf のスキーマを 1:1 写像。

```lhat
return^ {
    window = {
        title = "My Game",
        width = 1280,
        height = 720,
    },
}
```

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
- `math.sin` 等のスカラー数学関数は現状 L^ に無い（lhat 側へ提案中、[lhat-issues.md](lhat-issues.md)）
- 実行時のコード読み込みは `std.load.file(path)` / `std.load.text(src, name)`、または `love.filesystem.load`（M2）
