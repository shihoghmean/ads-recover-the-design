#!/usr/bin/env python3
"""第 4 章的性能实验：把 recover --stats 的 stderr 收集成表。

五组实验：
  scale      固定密度、放大网格，看时间和结点数怎么涨
  heuristic  同一批数据换四种分支点策略（0..3），比结点数
  worst      随机撒一批 50x50，挑结点数最大的（看最坏情况）
  hard       高密度 50x50，这类输入会真的打穿搜索，用来量最坏情况的尾巴
  unsolvable 高度数的随机输入，基本都是无解，看传播能不能直接否掉

用法：python bench.py [实验名...]（不给就跑全部）；加 -u 让输出不被缓冲。
"""
import os
import random
import statistics
import subprocess
import sys

import exe as exefinder
import gen

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = exefinder.find_exe(flag="--exe")
OUT = os.path.join(HERE, "..", "..", "data", "bench")


def to_text(n, m, k):
    return "\n".join(["%d %d" % (n, m)] + [" ".join(map(str, row)) for row in k]) + "\n"


def run(case, heur=0, repeat=3, timeout=10.0, noprop=False):
    """跑 repeat 次取最快的一次（排除系统抖动），返回 dict；超时返回 {'timeout': True}。"""
    best = None
    args = [EXE, "--heur", str(heur), "--stats"] + (["--noprop"] if noprop else [])
    for _ in range(repeat):
        try:
            p = subprocess.run(args, input=case,
                               capture_output=True, text=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            # 超时说明这条数据真的打穿了搜索，再跑几次也是白等，直接记下来
            return {"timeout": True}
        line = [l for l in p.stderr.splitlines() if l.startswith("nodes=")]
        if not line:
            return None
        kv = dict(x.split("=") for x in line[0].split())
        cur = {"nodes": int(kv["nodes"]), "prop": int(kv["propagate"]),
               "ms": float(kv["time_ms"]), "yes": kv["answer"] == "yes", "timeout": False}
        if best is None or cur["ms"] < best["ms"]:
            best = cur
    return best


def summarize(got):
    alive = [g for g in got if g and not g.get("timeout")]
    ok = [g for g in alive if g["yes"]]
    no = [g for g in alive if not g["yes"]]
    ms = [g["ms"] for g in alive]
    nodes = [g["nodes"] for g in alive]
    return {
        "n": len(got), "yes": len(ok), "no": len(no), "timeout": len(got) - len(alive),
        "ms_med": statistics.median(ms) if ms else float("nan"),
        "ms_max": max(ms) if ms else float("nan"),
        "nodes_med": statistics.median(nodes) if nodes else float("nan"),
        "nodes_max": max(nodes) if nodes else float("nan"),
    }


def cases_scale():
    rows = []
    for size in (10, 20, 30, 40, 50):
        for density in (0.15, 0.25, 0.35):
            got = []
            for s in range(12):
                k = gen.gen_layout(size, size, density, random.Random(size * 1000 + s * 17 + int(density * 100)))
                got.append(run(to_text(size, size, k)))
            rows.append((size, density, got))
    return rows


def cases_heuristic():
    data = []
    for s in range(8):
        k = gen.gen_layout(30, 30, 0.35, random.Random(9000 + s))
        data.append(("30x30 d0.35 #%d" % s, to_text(30, 30, k)))
    for s in range(4):
        k = gen.gen_layout(50, 50, 0.40, random.Random(9500 + s))
        data.append(("50x50 d0.40 #%d" % s, to_text(50, 50, k)))
    return data


def cases_worst():
    rows = []
    rng = random.Random(11)
    for s in range(40):
        density = rng.choice([0.2, 0.3, 0.4, 0.5])
        k = gen.gen_layout(50, 50, density, rng)
        rows.append((density, s, run(to_text(50, 50, k), timeout=10.0)))
    return rows


def cases_hard():
    rows = []
    for density in (0.55, 0.6):
        got = []
        for s in range(8):
            k = gen.gen_layout(50, 50, density, random.Random(31000 + s))
            got.append(run(to_text(50, 50, k), timeout=10.0))
        rows.append((density, got))
    return rows


def cases_unsolvable():
    rows = []
    rng = random.Random(13)
    for p in (0.5, 0.7, 0.9):
        got = []
        for s in range(15):
            k = gen.gen_random(50, 50, p, rng)
            got.append(run(to_text(50, 50, k), timeout=10.0))
        rows.append((p, got))
    return rows


def cases_prop():
    """传播开/关的对照：同一组数据、同一策略，只差 propagate() 有没有跑。"""
    data = []
    for s in range(4):
        k = gen.gen_layout(30, 30, 0.35, random.Random(9000 + s))
        data.append(("30x30 d0.35 #%d" % s, to_text(30, 30, k)))
    for s in range(2):
        k = gen.gen_layout(50, 50, 0.40, random.Random(9500 + s))
        data.append(("50x50 d0.40 #%d" % s, to_text(50, 50, k)))
    return data


def main():
    os.makedirs(OUT, exist_ok=True)
    which = [a for a in sys.argv[1:] if not a.startswith("-")] or \
            ["scale", "heuristic", "prop", "worst", "hard", "unsolvable"]

    if "scale" in which:
        print("== scale: layout 输入，12 个种子取中位数 ==")
        print("| size | density | 有解/总数 | 中位时间 ms | 最大 ms | 中位结点 | 最大结点 | 超时 |")
        print("|---|---|---|---|---|---|---|---|")
        for size, density, got in cases_scale():
            s = summarize(got)
            print("| %d | %.2f | %d/%d | %.2f | %.2f | %.0f | %.0f | %d |"
                  % (size, density, s["yes"], s["n"], s["ms_med"], s["ms_max"],
                     s["nodes_med"], s["nodes_max"], s["timeout"]))

    if "heuristic" in which:
        print("\n== heuristic: 四种分支点策略 ==")
        print("| case | h0 结点 | h1 结点 | h2 结点 | h3 结点 | h0 ms | h1 ms | h2 ms | h3 ms |")
        print("|---|---|---|---|---|---|---|---|---|")
        tot = [0, 0, 0, 0]
        for name, case in cases_heuristic():
            res = [run(case, heur=h) for h in range(4)]
            nodes, ms = [], []
            for i, r in enumerate(res):
                nodes.append("超时" if (r is None or r.get("timeout")) else str(r["nodes"]))
                ms.append("超时" if (r is None or r.get("timeout")) else "%.2f" % r["ms"])
                if r and not r.get("timeout"):
                    tot[i] += r["nodes"]
            print("| %s | %s | %s | %s | %s | %s | %s | %s | %s |"
                  % ((name,) + tuple(nodes) + tuple(ms)))
        print("| **合计结点** | %d | %d | %d | %d | | | | |" % tuple(tot))

    if "worst" in which:
        print("\n== worst: 40 组随机 50x50，按结点数排序取前 8 ==")
        rows = cases_worst()
        rows.sort(key=lambda t: -(t[2]["nodes"] if t[2] and not t[2].get("timeout") else 10 ** 18))
        for density, s, r in rows[:8]:
            if r and not r.get("timeout"):
                print("d=%.2f #%d nodes=%d prop=%d ms=%.1f answer=%s"
                      % (density, s, r["nodes"], r["prop"], r["ms"], "yes" if r["yes"] else "no"))
            else:
                print("d=%.2f #%d 超时" % (density, s))
        s = summarize([r for _, _, r in rows])
        print("全部 40 组：有解 %d / 无解 %d / 超时 %d，中位 %.2f ms，最大 %.2f ms，最大结点 %d"
              % (s["yes"], s["no"], s["timeout"], s["ms_med"], s["ms_max"], s["nodes_max"]))

    if "hard" in which:
        print("\n== hard: 高密度 50x50，每组 8 个种子、单次上限 10 s ==")
        print("| density | 跑完/总数 | 中位 ms | 最大 ms | 最大结点 | 超时 |")
        print("|---|---|---|---|---|---|")
        for density, got in cases_hard():
            s = summarize(got)
            print("| %.2f | %d/%d | %.1f | %.1f | %.0f | %d |"
                  % (density, s["n"] - s["timeout"], s["n"], s["ms_med"], s["ms_max"],
                     s["nodes_max"], s["timeout"]))

    if "prop" in which:
        print("\n== prop: 传播开 / 关，同策略同数据（超时上限 10 s）==")
        print("| case | 开 结点 | 开 ms | 关 结点 | 关 ms |")
        print("|---|---|---|---|---|")
        for name, case in cases_prop():
            on = run(case)
            off = run(case, noprop=True)
            def cell(r):
                if r is None or r.get("timeout"):
                    return ("超时", "超时")
                return (str(r["nodes"]), "%.2f" % r["ms"])
            a, b = cell(on), cell(off)
            print("| %s | %s | %s | %s | %s |" % (name, a[0], a[1], b[0], b[1]))

    if "unsolvable" in which:
        print("\n== unsolvable: 50x50 随机度数（大概率无解）==")
        print("| 非零概率 | 有解/总数 | 中位 ms | 最大 ms | 中位结点 | 最大结点 | 超时 |")
        print("|---|---|---|---|---|---|---|")
        for p, got in cases_unsolvable():
            s = summarize(got)
            print("| %.1f | %d/%d | %.2f | %.2f | %.0f | %.0f | %d |"
                  % (p, s["yes"], s["n"], s["ms_med"], s["ms_max"],
                     s["nodes_med"], s["nodes_max"], s["timeout"]))


if __name__ == "__main__":
    main()
