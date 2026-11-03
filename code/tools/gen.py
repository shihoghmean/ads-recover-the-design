#!/usr/bin/env python3
"""测试数据生成器。

两种模式：
  layout  先在网格上随机铺一批互不相交的栅栏，再按铺出来的端点算每个连接器的
          度数。这样造出来的输入**一定有解**，用来测"有解"这一侧，也顺便测
          "程序能不能找到解"而不是只会输出 No Solution。
  random  每个格子独立地按概率给一个 1..4 的度数。这种输入绝大多数无解，
          正好拿来测 "No Solution" 这一侧。

用法：
  python gen.py layout 20 20 0.35 1 > case.in
  python gen.py random 8 8 0.5 2 > case.in
"""
import random
import sys

EMPTY, HORIZ, VERT = 0, 1, 2


def gen_layout(n, m, density, rng):
    """随机铺栅栏。返回度数表；密度是"被栅栏或连接器占用的格子比例"的目标值。"""
    owner = [[EMPTY] * m for _ in range(n)]
    k = [[0] * m for _ in range(n)]

    def free_cell(r, c):
        # 连接器所在的格子 owner 仍是 EMPTY，但栅栏不能从它身上穿过去（只能连到它），
        # 所以要连 k 一起查。
        return 0 <= r < n and 0 <= c < m and owner[r][c] == EMPTY and k[r][c] == 0

    def can_extend(r, c):
        return 0 <= r < n and 0 <= c < m and (owner[r][c] == EMPTY or k[r][c] > 0)

    target = int(n * m * density)
    filled = 0
    attempts = 0
    linked = set()   # 已经直接相连的相邻连接器对，避免在两点之间铺第二条线
    while filled < target and attempts < n * m * 40:
        attempts += 1
        horiz = rng.random() < 0.5
        r = rng.randrange(n)
        c = rng.randrange(m)
        limit = (m - c) if horiz else (n - r)
        if limit < 2:
            continue
        length = rng.randint(2, min(limit, 12))
        cells = [(r, c + i) for i in range(length)] if horiz else [(r + i, c) for i in range(length)]

        # 两端可以是空格，也可以是度数还没满的连接器；中间必须是空格。
        ends = [cells[0], cells[-1]]
        ok = True
        for cell in ends:
            rr, cc = cell
            if not can_extend(rr, cc):
                ok = False
            elif k[rr][cc] >= 4:
                ok = False
        for cell in cells[1:-1]:
            if not free_cell(*cell):
                ok = False
        if ok and length == 2 and k[ends[0][0]][ends[0][1]] > 0 and k[ends[1][0]][ends[1][1]] > 0:
            pair = tuple(sorted((ends[0][0] * m + ends[0][1], ends[1][0] * m + ends[1][1])))
            if pair in linked:
                ok = False
        if not ok:
            continue
        if length == 2:
            pair = tuple(sorted((ends[0][0] * m + ends[0][1], ends[1][0] * m + ends[1][1])))
            linked.add(pair)

        # 落笔：两端度数 +1（空格就变成新连接器），中间标成栅栏
        for cell in ends:
            rr, cc = cell
            if k[rr][cc] == 0:
                k[rr][cc] = 1
                filled += 1
            else:
                k[rr][cc] += 1
        for cell in cells[1:-1]:
            rr, cc = cell
            owner[rr][cc] = HORIZ if horiz else VERT
            filled += 1

    return k


def gen_random(n, m, p, rng):
    return [[(rng.randint(1, 4) if rng.random() < p else 0) for _ in range(m)] for _ in range(n)]


def main():
    if len(sys.argv) < 6:
        print(__doc__)
        return 2
    mode = sys.argv[1]
    n, m = int(sys.argv[2]), int(sys.argv[3])
    value = float(sys.argv[4])
    seed = int(sys.argv[5])
    rng = random.Random(seed)

    if mode == "layout":
        k = gen_layout(n, m, value, rng)
    elif mode == "random":
        k = gen_random(n, m, value, rng)
    else:
        print(__doc__)
        return 2

    out = ["%d %d" % (n, m)]
    out += [" ".join(map(str, row)) for row in k]
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
