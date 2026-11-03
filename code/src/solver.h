// solver.h —— "Recover the Design" 的求解器接口
//
// 建模：把每个连接器看成一个点，同一行（或同一列）里**相邻**的两个连接器之间
// 可以铺一条栅栏，看成一条候选边。为什么只连相邻的：栅栏不能拐弯，一条栅栏的
// 两端必须落在连接器上；如果它中途经过第三个连接器，那条栅栏就只能在那里断开成
// 两条，于是任何合法方案都可以拆成"相邻连接器之间的一段一段"。
//
// 在这个模型下，问题变成：从候选边里选一个子集，使得
//   (1) 每个连接器 incident 的选中边数恰好等于它的 k；
//   (2) 选中的水平边和垂直边不能在空格处十字相交。
// 两条约束都是局部的，但叠在一起就是 NP 难的（含最大独立集作为特例），
// 所以这里用"约束传播 + 回溯"，并在回溯时优先挑最紧的点分支。
//
// 规模：连接器数 V ≤ nm，候选边数 E ≤ 2nm（每行/列相邻点各一条），
// 每点最多 4 条关联边。空间 O(nm + E)，建图 O(nm + E)。
//
// 几条贯穿全类的不变量，改代码时务必保持：
//   * st_[e] == 1 的边，两端 need_ 已经减过；st_[e] == 2 的边两端 freeCnt_ 已经减过，
//     所以"未定边数"和"还差几条"永远互相自洽，传播规则才有意义；
//   * 任何一次状态改动都必须在 trail_ 上留一条旧值，否则 undo 之后状态就脏了；
//   * queue_ 里只放"need_ 或 freeCnt_ 刚变过"的点；undo 会同时清空队列和 inQueue_；
//   * need_ ∈ [0, freeCnt_] 是搜索继续的前提，一旦越界 propagate() 立刻返回 false。
#pragma once

#include <array>
#include <vector>

namespace p3 {

// 一条候选边：两个连接器之间的一段水平或垂直栅栏。
// 存两端的格子坐标而不是只存连接器编号，是因为最后要按方向输出 1/0，
// 而且"这条边横跨了哪些格子"要靠坐标现算，省一份内存。
struct Edge {
    int u, v;              // 两端连接器的编号（0 基，按行优先编号）
    int r1, c1, r2, c2;    // 两端所在的格子（0 基）
};

class Solver {
public:
    // 传入 n 行 m 列的度数表（0 表示该格没有连接器），建图。
    // 这个接口永远返回 true：候选边怎么建都建得出来，真正的"无解"判断留给 solve()，
    // 因为"某个点边数不够"这类结论在 solve() 一开始就能统一处理，不必分两处写。
    bool init(int n, int m, const std::vector<int>& grid);

    // 搜索一个合法方案；返回 false 表示无解。
    // 可以重复调用（状态在函数开头会重置），但一次对象只对应一组输入。
    bool solve();

    // 以下接口只在该次 solve() 返回 true 后有效，且都返回内部引用，不拷贝。
    const std::vector<Edge>& edges() const { return edges_; }        // 全部候选边
    const std::vector<char>& chosen() const { return st_; }          // 每条边的状态
    const std::vector<int>& connectorCell() const { return cellOf_; }  // 连接器所在的格子

    // 把选中的边整理成题目要的方向表：第 i 项是第 i 个连接器的 {上,下,左,右}。
    // 连接器本身就按行优先编号，所以这个数组的顺序就是输出的顺序。
    std::vector<std::array<int, 4>> directions() const;

    // 供报告里的实验用：搜索树结点数、传播中处理过的点次数。
    // 两个计数都只增不减，solve() 开头清零，不影响任何判定逻辑。
    long long nodes() const { return nodes_; }
    long long propagations() const { return propagations_; }

    // 分支点的选取策略，只影响搜索快慢，不影响答案对不对：
    //   0 = 自由度减需求最小（默认，最接近被逼死的点）
    //   1 = 行优先的第一个点（最朴素）
    //   2 = 需求最大的点
    //   3 = 未定边最多的点
    // 第 4 章拿这四种在同一批数据上比结点数：小规模几乎一样，高密度大实例上
    // 2 的总结点数少两个数量级，但个别实例会反过来爆掉；3 最不稳。单一一句话
    // 选不出赢家，所以默认仍用最稳的 0。
    void setHeuristic(int h) { heur_ = h; }

    // 关掉传播（只保留逐边检查），用来量"传播到底值多少"。
    // 关掉后算法退化成一个带前向检查的回溯，仍然正确，只是结点数会暴涨。
    void setPropagation(bool on) { useProp_ = on; }

private:
    // ---- 建图 ----
    // 加一条候选边，并把它横跨的空格登记到 hEdge_/vEdge_。
    void addEdge(int u, int v, int r1, int c1, int r2, int c2);

    // ---- 状态修改（全部写 trail，可回滚）----
    void pushVertex(int v);                  // 把点排进待传播队列（去重）
    bool selectEdge(int e);                  // 选中一条边，连带否掉交叉的对向边
    bool banEdge(int e);                     // 弃用一条边（幂等）
    bool addNeed(int v, int delta);          // 改"还差几条"，负数即失败
    void subFree(int v);                     // 未定边数减一，并触发该点重新传播
    void undo(size_t mark);                  // 回滚到 trail 的某个长度

    // ---- 传播与搜索 ----
    bool propagate();                        // 反复套三条传播规则直到不动点
    bool dfs();                              // 回溯主体
    int chooseVertex() const;                // 挑下一个分支点，-1 表示全部满足

    // ---- 建图结果（solve() 之后只读）----
    int n_ = 0, m_ = 0;
    std::vector<int> deg_;       // 每个格子的 k，0 表示没有连接器
    std::vector<int> cid_;       // 格子 -> 连接器编号，-1 表示空
    std::vector<int> cellOf_;    // 连接器 -> 格子编号
    std::vector<Edge> edges_;    // 候选边表
    std::vector<std::vector<int>> inc_;   // 连接器 -> 关联的候选边
    std::vector<int> hEdge_, vEdge_;      // 空格 -> 盖住它的水平/垂直候选边

    // ---- 搜索状态（会被 undo 反复改写）----
    std::vector<char> st_;       // 每条边：0 未定 / 1 选中 / 2 弃用
    std::vector<int> need_;      // 每个连接器还差几条边
    std::vector<int> freeCnt_;   // 每个连接器还有几条未定的关联边
    std::vector<int> queue_;     // 待检查的连接器
    std::vector<char> inQueue_;  // 是否已在队列里，用来给 pushVertex 去重

    // 回溯用：只记"哪个量被改成了什么"，undo 时倒着还原。
    struct Change { int kind; int idx; int old; };
    std::vector<Change> trail_;

    // ---- 实验统计 ----
    long long nodes_ = 0;          // dfs() 的调用次数，即搜索树结点数
    long long propagations_ = 0;   // 传播里检查过的点次数
    int heur_ = 0;                 // 分支点策略，见 setHeuristic
    bool useProp_ = true;          // 是否开传播，见 setPropagation
};

}  // namespace p3
