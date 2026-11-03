#!/usr/bin/env python3
"""校验一份输出是否是该输入的合法方案。

用法：python check.py <case.in> <answer.out>
退出码 0 表示合法（或输出为 No Solution，此时无法在没有标准解的情况下判定），
1 表示不合法，并打印第一条不合格的原因。

检查的内容就是题目的两条约束加格式：
  1. 每个连接器恰好一行，行列坐标对得上，不能多也不能少；
  2. 每个 1 方向走过去，碰到的第一个非空格必须是连接器（不能出界、不能悬空）；
  3. 水平栅栏与垂直栅栏不能在空格处相交；
  4. 每个连接器四个方向的和等于输入的 k。
"""
import sys


def read_case(path):
    with open(path, encoding="utf-8") as f:
        data = f.read().split()
    n, m = int(data[0]), int(data[1])
    grid = [[int(data[2 + r * m + c]) for c in range(m)] for r in range(n)]
    return n, m, grid


def check(n, m, grid, text):
    lines = [l.strip() for l in text.splitlines() if l.strip()]
    if not lines:
        return "输出为空"
    if lines[0] == "No Solution":
        return None

    connectors = {(r, c) for r in range(n) for c in range(m) if grid[r][c] > 0}
    seen = {}
    for line in lines:
        parts = line.split()
        if len(parts) != 6:
            return "每行必须是 6 个整数：%r" % line
        try:
            row, col = int(parts[0]) - 1, int(parts[1]) - 1
            dirs = [int(x) for x in parts[2:]]
        except ValueError:
            return "无法解析成整数：%r" % line
        if (row, col) not in connectors:
            return "第 %d 行第 %d 列不是连接器" % (row + 1, col + 1)
        if (row, col) in seen:
            return "连接器 (%d,%d) 重复输出" % (row + 1, col + 1)
        if any(d not in (0, 1) for d in dirs):
            return "方向位只能是 0 或 1：%r" % line
        seen[(row, col)] = dirs

    if set(seen) != connectors:
        missing = connectors - set(seen)
        return "漏掉了 %d 个连接器，例如 %s" % (len(missing), sorted(missing)[:3])

    # 方向和坐标的对应：上、下、左、右
    steps = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    opposite = [1, 0, 3, 2]
    hs, vs = set(), set()      # 被水平/垂直栅栏占用的空格
    for (r, c), dirs in seen.items():
        deg = sum(dirs)
        if deg != grid[r][c]:
            return "(%d,%d) 的度数 %d 与输入的 %d 不符" % (r + 1, c + 1, deg, grid[r][c])
        for bit, (dr, dc) in enumerate(steps):
            if not dirs[bit]:
                continue
            rr, cc = r + dr, c + dc
            cells = []
            while True:
                if not (0 <= rr < n and 0 <= cc < m):
                    return "(%d,%d) 的栅栏冲出边界" % (r + 1, c + 1)
                if grid[rr][cc] > 0:
                    break
                cells.append((rr, cc))
                rr += dr
                cc += dc
            # 栅栏的另一端也必须声明这一方向，否则就是一段只连了一头的线
            if not seen[(rr, cc)][opposite[bit]]:
                return "(%d,%d) 到 (%d,%d) 的栅栏只有一端声明" % (r + 1, c + 1, rr + 1, cc + 1)
            for x in cells:
                (hs if dr == 0 else vs).add(x)

    both = hs & vs
    if both:
        x = sorted(both)[0]
        return "栅栏在空格 (%d,%d) 处相交" % (x[0] + 1, x[1] + 1)
    return None


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    n, m, grid = read_case(sys.argv[1])
    with open(sys.argv[2], encoding="utf-8") as f:
        text = f.read()
    err = check(n, m, grid, text)
    if err:
        print("INVALID:", err)
        return 1
    print("OK" if text.split() and text.split()[0] != "No" else "NO-SOLUTION(未验证)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
