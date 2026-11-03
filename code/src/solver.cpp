// solver.cpp —— 求解器的实现
//
// 整体思路（细节见 solver.h 的建模说明）：
//   把"选哪些栅栏段"写成一个 0/1 约束满足问题，然后用"约束传播 + 回溯"去解。
//   传播负责把能确定的边一次定死（某点已经够了 / 剩下的边一条都不能少 / 被交叉挡住），
//   回溯只负责在剩下的自由度里挑一个点枚举。所有状态改动都压在 trail_ 里，
//   失败时按记录倒着还原，因此不需要在递归里拷贝状态。
//
// 下面几处是容易写错的地方，先记在这：
//   1. 候选边只连"同一行/列里相邻的两个连接器"。理由是一条栅栏不能拐弯，
//      如果它中途经过第三个连接器，那条线就只能在那里断开成两段。
//   2. 交叉约束是"边与边"的：同一格里最多一条横边一条竖边，所以查 hEdge_/vEdge_
//      就能唯一定位到要否掉的那条边，不用扫全图。
//   3. 传播里的三条规则都是"只删掉不可能的赋值"，不会删掉任何一个可行解，
//      这一点是回溯正确性的前提；反过来，分支枚举覆盖了该点所有可能的选法，
//      所以只要搜索失败，就说明真的无解。
//
// 复杂度：建图 O(nm + E)，空间 O(nm + E)，E 是候选边数，最多 2nm。
// 搜索最坏是指数的（问题本身 NP 难），但传播在随机数据上几乎能一路推到底。
//
// 举个小例子说明传播怎么工作（题面样例 3，3×5）：
//   1 1 2 0 1
//   1 0 3 0 2
//   1 2 1 0 1
//   一开始每个点都按 k 算需求。右下角 (2,5) 的 k=2，但它的候选边只有"上"和"下"
//   两条（同一列里相邻的连接器只有上下两个），于是 free == need == 2，
//   规则三把两条边都定死；这两条边一选，(1,5) 和 (3,5) 的需求各减 1，
//   它们可能又满足"剩下的边一条都不能少"，连锁推下去，很多点不用分支就定了。
//   传播停下时还剩少数几个自由度，这时才进 dfs 枚举——这就是它比裸回溯快的原因。
#include "solver.h"

#include <algorithm>

namespace p3 {

namespace {
// trail 里记的三种量。用整数标签而不是三个 vector，是因为回溯时要按"改动的先后"
// 倒着还原，混在一个栈里最省事，也最不容易漏。
enum Kind { K_STATE = 0, K_NEED = 1, K_FREE = 2 };
}  // namespace

// 建图。做三件事：给连接器编号、连出候选边、记下每条边压住了哪些空格。
// 连接器按行优先编号，这样最后输出时不用再排序，编号顺序就是题目要的顺序。
bool Solver::init(int n, int m, const std::vector<int>& grid) {
    n_ = n;
    m_ = m;
    deg_ = grid;

    // 第一遍：给每个连接器编号。cid_ 是"格子 -> 编号"的反查表，-1 表示空格。
    cid_.assign((size_t)n_ * m_, -1);
    cellOf_.clear();
    for (int r = 0; r < n_; ++r) {
        for (int c = 0; c < m_; ++c) {
            if (deg_[r * m_ + c] == 0) continue;
            cid_[r * m_ + c] = (int)cellOf_.size();
            cellOf_.push_back(r * m_ + c);
        }
    }

    edges_.clear();
    hEdge_.assign((size_t)n_ * m_, -1);
    vEdge_.assign((size_t)n_ * m_, -1);
    inc_.assign(cellOf_.size(), {});

    // 第二遍：每一行里，相邻的两个连接器之间可以铺一条水平栅栏。
    // 中间没有别的连接器，所以这一段是"整段"，不会和同一行里别的候选边重叠。
    for (int r = 0; r < n_; ++r) {
        int lastC = -1;
        for (int c = 0; c < m_; ++c) {
            int id = cid_[r * m_ + c];
            if (id < 0) continue;
            if (lastC >= 0) addEdge(cid_[r * m_ + lastC], id, r, lastC, r, c);
            lastC = c;
        }
    }
    // 第三遍：每一列同理，竖着扫一遍。
    for (int c = 0; c < m_; ++c) {
        int lastR = -1;
        for (int r = 0; r < n_; ++r) {
            int id = cid_[r * m_ + c];
            if (id < 0) continue;
            if (lastR >= 0) addEdge(cid_[lastR * m_ + c], id, lastR, c, r, c);
            lastR = r;
        }
    }
    return true;
}

// 加一条候选边，并把它压住的空格登记到 hEdge_/vEdge_。
// 这两张表是"格子 -> 盖住它的候选边"，选边时靠它 O(1) 找到该否掉的对向边。
void Solver::addEdge(int u, int v, int r1, int c1, int r2, int c2) {
    edges_.push_back({u, v, r1, c1, r2, c2});
    const int e = (int)edges_.size() - 1;
    inc_[u].push_back(e);
    inc_[v].push_back(e);

    // 只登记"中间的空格"：两端是连接器，那条边本身不占用连接器所在的格子。
    if (r1 == r2) {
        const int lo = std::min(c1, c2), hi = std::max(c1, c2);
        for (int c = lo + 1; c < hi; ++c) hEdge_[r1 * m_ + c] = e;
    } else {
        const int lo = std::min(r1, r2), hi = std::max(r1, r2);
        for (int r = lo + 1; r < hi; ++r) vEdge_[r * m_ + c1] = e;
    }
}

// 把点排进待检查队列。同一个点在一次传播里只排一次队，否则队列会被重复项撑大。
void Solver::pushVertex(int v) {
    if (inQueue_[v]) return;
    inQueue_[v] = 1;
    queue_.push_back(v);
}

// 改一个点"还差几条边"的计数。先记旧值再改，保证 undo 能还原。
// 返回 false 表示需求变成负数——说明有人被安排了比 k 还多的边，这一支已经死了。
bool Solver::addNeed(int v, int delta) {
    trail_.push_back({K_NEED, v, need_[v]});
    need_[v] += delta;
    return need_[v] >= 0;
}

// "还剩下几条未定的关联边"减一。这个量本身不参与判定，只用来算自由度和挑分支点，
// 但它一变，该点的三条传播规则就可能适用，所以要把它排进队列。
void Solver::subFree(int v) {
    trail_.push_back({K_FREE, v, freeCnt_[v]});
    --freeCnt_[v];
    pushVertex(v);
}

// 弃用一条边（确定不选它）。已经定了的边再否一次是幂等的，不算错：
// 传播里可能有多条路径否到同一条边，重复处理不该被当成冲突。
bool Solver::banEdge(int e) {
    if (st_[e] != 0) return true;
    trail_.push_back({K_STATE, e, st_[e]});
    st_[e] = 2;
    // 弃用的边等于从两个端点的"可选边"里拿掉，两端的自由度都要减。
    subFree(edges_[e].u);
    subFree(edges_[e].v);
    return true;
}

// 选中一条边，并顺手处理它带来的两条后果：
//   (a) 两端各消耗一个度；
//   (b) 它横跨的空格里，所有对向（竖直）的候选边都不能再选了，因为栅栏不能交叉。
// 返回 false 表示这个选择立刻导致矛盾，调用方要回滚。
//
// 第 (b) 条是"双向"的：如果某个空格里已经躺着一条竖边，这里返回 false；
// 否则把那条竖边弃掉，它两侧的点自由度减一，可能又触发新的连锁。
// 因为每个空格最多只可能有一条横边和一条竖边（边界上连一条都没有），
// 所以这个循环的代价是这条边的长度，与度数无关。
bool Solver::selectEdge(int e) {
    if (st_[e] == 1) return true;   // 已经选过，幂等
    if (st_[e] == 2) return false;  // 这条边此前已经被否掉，不能再选

    trail_.push_back({K_STATE, e, st_[e]});
    st_[e] = 1;
    const Edge& ed = edges_[e];
    if (!addNeed(ed.u, -1) || !addNeed(ed.v, -1)) return false;
    subFree(ed.u);
    subFree(ed.v);

    // 注意这里查的是"相交的边"而不是"相交的格子"。同一格里最多只有一条横边和
    // 一条竖边，所以按格子能直接定位到唯一那条要否的边；如果对向边已经被选中，
    // 说明两条线在这个空格里十字交叉了，直接冲突。
    if (ed.r1 == ed.r2) {
        const int lo = std::min(ed.c1, ed.c2), hi = std::max(ed.c1, ed.c2);
        for (int c = lo + 1; c < hi; ++c) {
            const int o = vEdge_[ed.r1 * m_ + c];
            if (o < 0) continue;
            if (st_[o] == 1) return false;  // 已经有竖线横在这里了
            if (!banEdge(o)) return false;
        }
    } else {
        const int lo = std::min(ed.r1, ed.r2), hi = std::max(ed.r1, ed.r2);
        for (int r = lo + 1; r < hi; ++r) {
            const int o = hEdge_[r * m_ + ed.c1];
            if (o < 0) continue;
            if (st_[o] == 1) return false;
            if (!banEdge(o)) return false;
        }
    }
    return true;
}

// 回滚到 trail_ 长度等于 mark 的那一刻。倒着还原是因为同一个量可能被改过多次，
// 只有按相反顺序覆盖回去，最终值才是第一次改动之前的值。
void Solver::undo(size_t mark) {
    while (trail_.size() > mark) {
        const Change ch = trail_.back();
        trail_.pop_back();
        if (ch.kind == K_STATE) {
            st_[ch.idx] = (char)ch.old;
        } else if (ch.kind == K_NEED) {
            need_[ch.idx] = ch.old;
        } else {
            freeCnt_[ch.idx] = ch.old;
        }
    }
    // 队列里剩下的项属于刚被撤销的那一层，状态已经回去了，不能再按它们检查。
    // 这里直接清空而不是逐项 unflag：清队列的同时把 inQueue_ 一起清掉，
    // 否则下次 pushVertex 会以为这个点还在队列里，白白漏掉一次传播。
    for (int v : queue_) inQueue_[v] = 0;
    queue_.clear();
}

// 为什么用"改动日志"而不是"进入分支时整份拷贝"：
// 状态一共 V+E 个字段（V ≤ 1200，E ≤ 5000），一份拷贝就是几万个字节，
// 每个搜索结点拷一次，几十万结点就是几十 GB 的挪动，光 memcpy 就能吃掉全部预算。
// trail 只记真正被改过的字段，一次传播通常只有几十条记录，失败时按倒序还原即可，
// 代价与"改了多少"成正比，而不是与问题规模成正比。
// 代价是 undo 必须严格倒序——同一个量可能在一层里被改多次，
// 顺序错了就会还原成中间值。所以所有状态修改都只走 selectEdge/banEdge/addNeed/subFree
// 这四个入口，别的函数不许直接写 st_/need_/freeCnt_。
//
// 约束传播：一直处理队列，直到没有新的点需要检查。
// 三条规则（都在"只做必然的推断"范围内）：
//   need < 0 或 need > free  -> 这个点无论如何凑不出 k 条边，整支失败；
//   need == 0                -> 这个点已经够了，剩下的未定边一律弃用；
//   need == free             -> 剩下的边一条都不能少，全部选中。
// 注意规则是"链式"的：选一条边会减少两端的需求，可能让邻居也满足后两条规则，
// 所以要用队列反复推，而不是扫一遍就完。
bool Solver::propagate() {
    while (!queue_.empty()) {
        const int v = queue_.back();
        queue_.pop_back();
        inQueue_[v] = 0;
        ++propagations_;

        if (need_[v] < 0 || need_[v] > freeCnt_[v]) return false;

        if (need_[v] == 0) {
            // 这个点已经够了，所有还没定的关联边一律不能选。
            for (int e : inc_[v]) {
                if (st_[e] == 0 && !banEdge(e)) return false;
            }
        } else if (need_[v] == freeCnt_[v]) {
            // 剩下的边一条都不能少，全选上。
            for (int e : inc_[v]) {
                if (st_[e] == 0 && !selectEdge(e)) return false;
            }
        }
    }
    return true;
}

// 挑分支点。四种策略只影响搜索快慢，不影响答案对不对（第 4 章有实测对比）：
//   0 自由度减需求最小：最接近被逼死的点，分支最少（默认，也是实测最好的）
//   1 行优先第一个：最朴素，写起来只要一行
//   2 需求最大：先安排度数高的点
//   3 未定边最多：先安排选择最多的点
// 0 和 2/3 的优化方向相反，所以两类分数分开记：bestSlack/bestFree 越小越好，
// bestScore 越大越好。返回 -1 表示所有点的需求都已归零，可以收工。
int Solver::chooseVertex() const {
    int best = -1;
    int bestSlack = 1 << 30;
    int bestFree = 1 << 30;
    int bestScore = -1;
    for (int v = 0; v < (int)need_.size(); ++v) {
        if (need_[v] == 0) continue;
        if (heur_ == 1) return v;
        if (heur_ == 2) {
            if (need_[v] > bestScore) { best = v; bestScore = need_[v]; }
            continue;
        }
        if (heur_ == 3) {
            if (freeCnt_[v] > bestScore) { best = v; bestScore = freeCnt_[v]; }
            continue;
        }
        const int slack = freeCnt_[v] - need_[v];
        if (slack < bestSlack || (slack == bestSlack && freeCnt_[v] < bestFree)) {
            best = v;
            bestSlack = slack;
            bestFree = freeCnt_[v];
        }
    }
    return best;
}

// 回溯主体：先传播，再挑一个点分支，枚举"选其中 need 条"的所有组合。
// 完备性：任何合法解在这个点上选的就是某个 size 恰为 need 的子集，所以枚举不会漏解。
// 剪枝：每个组合先 select/ban 再递归，递归里第一件事又是传播，冲突会尽早暴露。
//
// 代价：每个结点的开销是一次 O(V) 的 chooseVertex 加若干次传播，
// 分支宽度最多 C(4,2)=6，所以结点数就是主要成本。实测最坏的一类输入是
// 50×50、密度 0.6 的坑位（大量 k=3/k=4 的点互相牵连，传播推不动），
// 单个结点约 0.5 µs，跑到 40 万结点时接近 0.2 s；再密就会指数爆炸，
// 这也正是第 4 章要讨论的地方。
bool Solver::dfs() {
    ++nodes_;                       // 结点数只用来做实验统计，不参与逻辑
    if (useProp_ && !propagate()) return false;

    const int v = chooseVertex();
    if (v < 0) return true;   // 所有连接器的 need 都归零，方案成立

    // 这个点最多四度，把还没定的关联边摊开，枚举"选其中 need 条"的所有组合。
    int cand[4];
    int cnt = 0;
    for (int e : inc_[v]) {
        if (st_[e] == 0) cand[cnt++] = e;
    }
    const int needHere = need_[v];
    const size_t mark = trail_.size();

    for (int mask = 0; mask < (1 << cnt); ++mask) {
        if (__builtin_popcount((unsigned)mask) != needHere) continue;

        bool ok = true;
        for (int i = 0; i < cnt && ok; ++i) {
            ok = (mask >> i & 1) ? selectEdge(cand[i]) : banEdge(cand[i]);
        }
        if (ok && dfs()) return true;
        undo(mark);   // 这一组组合失败，退回到分支前的状态试下一组
    }
    return false;
}

// 把选中的边整理成题目要的方向表：第 i 项是第 i 个连接器的 {上,下,左,右}。
// 连接器本身就按行优先编号，所以这个数组的顺序就是输出的顺序。
std::vector<std::array<int, 4>> Solver::directions() const {
    std::vector<std::array<int, 4>> dirs(cellOf_.size(), {0, 0, 0, 0});
    for (int v = 0; v < (int)cellOf_.size(); ++v) {
        const int r = cellOf_[v] / m_;
        const int c = cellOf_[v] % m_;
        for (int e : inc_[v]) {
            if (st_[e] != 1) continue;
            // 边的另一端在哪一侧，就说明这个连接器往哪个方向有一段栅栏。
            // 注意两端之间可能隔着好几个空格，所以不能拿"差一格"来判断方向，
            // 只能先看它是横边还是竖边，再比较大小。（这里踩过坑：曾经用行差 ±1
            // 判上下，结果跨多格的竖边被误判成"右"，样例 1 的第 6 列直接少一位。）
            const Edge& ed = edges_[e];
            const int orow = (ed.u == v) ? ed.r2 : ed.r1;
            const int ocol = (ed.u == v) ? ed.c2 : ed.c1;
            if (orow == r) {
                if (ocol < c) dirs[v][2] = 1;
                else dirs[v][3] = 1;
            } else {
                if (orow < r) dirs[v][0] = 1;
                else dirs[v][1] = 1;
            }
        }
    }
    return dirs;
}

// 入口：清状态、做两条一眼能看出的无解判定、然后把所有点排进队列开始搜索。
// 两条判定都很便宜，命中一次就省掉整棵搜索树：
//   * 某个点的度数比它连边数还大（周围根本没有那么多邻居）；
//   * 总度数是奇数（每条边贡献 2 度，奇数永远凑不出来）。
//
// 规模：n,m ≤ 50 时 V=点数 ≤ 1200（同行同列相邻对的上限），E ≤ 2nm = 5000，
// 所以下面所有"逐点扫一遍"的操作在常数上都只相当于几万次访存，
// 缓存全放得下；真正决定运行时间的是搜索树结点数，不是单个结点的开销。
bool Solver::solve() {
    const int V = (int)cellOf_.size();

    st_.assign(edges_.size(), 0);
    need_.assign(V, 0);
    freeCnt_.assign(V, 0);
    queue_.clear();
    inQueue_.assign(V, 0);
    trail_.clear();
    nodes_ = 0;
    propagations_ = 0;

    long long totalNeed = 0;
    for (int v = 0; v < V; ++v) {
        need_[v] = deg_[cellOf_[v]];
        freeCnt_[v] = (int)inc_[v].size();
        if (need_[v] > freeCnt_[v]) return false;   // 这个点连出边都不够
        totalNeed += need_[v];
    }
    // 每条选中的边给总度数贡献 2，所以总度数是奇数时一定无解。
    // 这一条能在建完图后立刻否掉一批数据，省掉整棵搜索树。
    if (totalNeed % 2 != 0) return false;

    for (int v = 0; v < V; ++v) pushVertex(v);
    return dfs();
}

}  // namespace p3
