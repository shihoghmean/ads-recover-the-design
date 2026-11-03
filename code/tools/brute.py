#!/usr/bin/env python3
"""暴力求解器：给小规模输入找一个合法方案，用来给主程序做对拍。

思路就是把"选哪些候选边"的所有组合试一遍：每条边只有选/不选两种状态，
选的时候检查两端度数没超、并且没有和已选的反方向栅栏交叉。规模小（边数十几条）
时它慢不到哪去，但它的答案一定是"存在性"的正确答案，正好当标准答案。
"""
import sys


def build(n, m, grid):
    """返回 (连接器坐标表, 每个连接器的度数, 候选边表)。

    候选边只连同一行/列里相邻的两个连接器；边的两个端点用连接器编号表示。
    """
    pts = [(r, c) for r in range(n) for c in range(m) if grid[r][c] > 0]
    idx = {p: i for i, p in enumerate(pts)}
    k = [grid[r][c] for r, c in pts]
    edges = []
    for r in range(n):
        row = [c for c in range(m) if grid[r][c] > 0]
        for a, b in zip(row, row[1:]):
            edges.append((idx[(r, a)], idx[(r, b)]))
    for c in range(m):
        col = [r for r in range(n) if grid[r][c] > 0]
        for a, b in zip(col, col[1:]):
            edges.append((idx[(a, c)], idx[(b, c)]))
    return pts, k, edges


def solve(n, m, grid):
    pts, k, edges = build(n, m, grid)
    E = len(edges)
    adj = [[] for _ in pts]
    for e, (u, v) in enumerate(edges):
        adj[u].append(e)
        adj[v].append(e)

    deg = [0] * len(pts)
    chosen = [False] * E
    used_h, used_v = set(), set()   # 已被水平/垂直栅栏占用的空格

    # suffix[v][e] = 端点 v 在"第 e 条及以后"的边里还挂着几条边。
    # 用来剪枝：如果剩下的边全选上都不够补度数，这一支就不用往下走了。
    V = len(pts)
    suffix = [[0] * (E + 1) for _ in range(V)]
    for e in range(E - 1, -1, -1):
        for v in range(V):
            suffix[v][e] = suffix[v][e + 1]
        suffix[edges[e][0]][e] += 1
        suffix[edges[e][1]][e] += 1

    def feasible(e):
        for v in range(V):
            if deg[v] > k[v] or deg[v] + suffix[v][e] < k[v]:
                return False
        return True

    def cells_of(e):
        (r1, c1), (r2, c2) = pts[edges[e][0]], pts[edges[e][1]]
        if r1 == r2:
            return [(r1, c) for c in range(min(c1, c2) + 1, max(c1, c2))], True
        return [(r, c1) for r in range(min(r1, r2) + 1, max(r1, r2))], False

    def dfs(e):
        if e == E:
            return all(deg[i] == k[i] for i in range(len(pts)))
        if not feasible(e):
            return False
        # 不选
        if dfs(e + 1):
            return True
        # 选：先查度数和交叉
        u, v = edges[e]
        if deg[u] >= k[u] or deg[v] >= k[v]:
            return False
        cells, horiz = cells_of(e)
        busy = used_v if horiz else used_h
        if any(x in busy for x in cells):
            return False
        own = used_h if horiz else used_v
        deg[u] += 1
        deg[v] += 1
        chosen[e] = True
        for x in cells:
            own.add(x)
        if dfs(e + 1):
            return True
        for x in cells:
            own.discard(x)
        chosen[e] = False
        deg[u] -= 1
        deg[v] -= 1
        return False

    if not dfs(0):
        return None

    dirs = {p: [0, 0, 0, 0] for p in pts}
    for e, (u, v) in enumerate(edges):
        if not chosen[e]:
            continue
        (r1, c1), (r2, c2) = pts[u], pts[v]
        if r1 == r2:
            if c1 < c2:
                dirs[(r1, c1)][3] = 1
                dirs[(r2, c2)][2] = 1
            else:
                dirs[(r1, c1)][2] = 1
                dirs[(r2, c2)][3] = 1
        else:
            if r1 < r2:
                dirs[(r1, c1)][1] = 1
                dirs[(r2, c2)][0] = 1
            else:
                dirs[(r1, c1)][0] = 1
                dirs[(r2, c2)][1] = 1
    return dirs


def main():
    data = sys.stdin.read().split()
    if not data:
        return
    n, m = int(data[0]), int(data[1])
    grid = [[int(data[2 + r * m + c]) for c in range(m)] for r in range(n)]
    ans = solve(n, m, grid)
    if ans is None:
        print("No Solution")
        return
    for (r, c) in sorted(ans):
        d = ans[(r, c)]
        print(r + 1, c + 1, d[0], d[1], d[2], d[3])


if __name__ == "__main__":
    main()
