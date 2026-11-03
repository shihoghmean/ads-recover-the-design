// main.cpp —— 读入、调用求解器、按题目要求的格式输出
//
// 输入：第一行 n m；接下来 n 行、每行 m 个整数，0 表示空格，1..4 表示该格的连接器
//       还要接几条栅栏。
// 输出：无解打一行 "No Solution"；有解则每个连接器一行：
//       row col up down left right
//       row/col 从 1 开始，四个方向位是 0/1，按行优先顺序输出。
//
// 拿题面样例 3 举例（3 行 5 列），输入是
//       3 5
//       1 1 2 0 1
//       1 0 3 0 2
//       1 2 1 0 1
// 输出是 11 行 6 列的整数，每行对应一个连接器，例如 "1 1 0 0 0 1" 表示
// 第 1 行第 1 列那个连接器往右有一段栅栏（上下左都没有）。多解时给哪个都算对。
//
// 命令行开关（只影响实验，不影响判题输出）：
//   --stats        往 stderr 打一行 nodes= propagate= time_ms= answer=，
//                  stdout 的答案保持原样，所以带 --stats 也能直接拿去对拍。
//   --heur N       换分支点策略（0..3，见 solver.h）；默认 0 是实测最稳的那种。
//   --noprop       关掉传播，只留逐边检查；用来量传播值多少，判题时不会用到。
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "solver.h"

namespace {

// 题目的输入小得很（最多 50×50=2500 个数），用 scanf 读完全够。
// 这里不做任何合法性检查之外的补救：读失败就报错退出，免得把空输入当成"无解"。
bool readInput(int& n, int& m, std::vector<int>& grid) {
    if (std::scanf("%d %d", &n, &m) != 2) return false;
    if (n <= 0 || m <= 0) return false;
    grid.assign((size_t)n * m, 0);
    for (int i = 0; i < n * m; ++i) {
        if (std::scanf("%d", &grid[i]) != 1) return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    bool stats = false;
    bool prop = true;
    int heur = 0;
    // 只认这几个开关，其余参数一律忽略：判题机上只会重定向 stdin，不会有参数。
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--stats") == 0) {
            stats = true;
        } else if (std::strcmp(argv[i], "--heur") == 0 && i + 1 < argc) {
            heur = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--noprop") == 0) {
            prop = false;
        }
    }

    int n = 0, m = 0;
    std::vector<int> grid;
    if (!readInput(n, m, grid)) {
        std::fprintf(stderr, "usage: recover [--stats] [--heur N] [--noprop] < input\n");
        return 1;
    }

    p3::Solver solver;
    solver.init(n, m, grid);
    solver.setHeuristic(heur);
    solver.setPropagation(prop);

    // 计时只包住 solve()：读入和输出的时间不算进去，
    // 否则报告里的表会被 printf 的 2 万多次调用污染。
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = solver.solve();
    const auto t1 = std::chrono::steady_clock::now();

    if (!ok) {
        std::printf("No Solution\n");
    } else {
        // 连接器编号本来就是行优先的，直接按编号顺序输出就是题目要的顺序；
        // 坐标加 1 是因为题面用 1 基，内部全用 0 基。
        const auto dirs = solver.directions();
        const auto& cells = solver.connectorCell();
        for (size_t i = 0; i < cells.size(); ++i) {
            const int r = cells[i] / m + 1;
            const int c = cells[i] % m + 1;
            std::printf("%d %d %d %d %d %d\n", r, c, dirs[i][0], dirs[i][1], dirs[i][2], dirs[i][3]);
        }
    }

    if (stats) {
        // 三个数都是给实验用的：nodes 是 dfs 调用次数，propagate 是传播里检查过的点数，
        // time_ms 用 steady_clock 算墙钟（不受系统时间调整影响）。
        // 这些都走 stderr，所以带 --stats 跑出来的 stdout 仍然是干净答案，能直接喂给校验脚本。
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::fprintf(stderr, "nodes=%lld propagate=%lld time_ms=%.3f answer=%s\n",
                     solver.nodes(), solver.propagations(), ms, ok ? "yes" : "no");
    }
    return 0;
}
