#include<bits/stdc++.h>
#include"solver.h"

/*
这个文件产出一个可执行程序 recover.exe，干三件事：读题面的度数表、交给 Solver 搜、按题目格式输出。
直接把输入重定向进来就行，判题机上不会有参数：

    recover.exe < case.in

另外留了一个 test 子命令跑内建自检（不打参数、不读文件）：

    recover.exe test

退出码只有 0 和 1：0 表示这一趟全对，1 表示读入失败、自检不过或者参数不对。
自检的用例就写在 runTest 里，不另开数据目录——用例少，写在代码里一眼能看全，
也能保证"自检"这件事永远跟着源码走，不会哪天拷包时漏掉一个 .in 文件。
*/

// 校验一份答案，用的是题目自己的规则，不看求解器内部怎么想的：
// 每个连接器的方向位之和等于 k；每个 1 顺着走下去必须先撞到另一个连接器，
// 而且那一端必须反向声明；横线和竖线不许落在同一个空格里。
//
// 这份校验是照题面重写的一遍，没有复用 Solver 里的任何数据结构和判断，
// 故意写得笨一点：笨的那份和求解器犯同一个错的可能性小。
// 对拍脚本 check.py 是第三份实现（Python），三份口径一致才算数。
bool verify(int n,int m,const vector<int>& grid,const vector<array<int,4>>& dirs,const vector<int>& cells){
    vector<int> id((size_t)n*m,-1);
    for(size_t i=0;i<cells.size();i++) id[cells[i]]=(int)i;

    for(size_t i=0;i<cells.size();i++){
        int sum=dirs[i][0]+dirs[i][1]+dirs[i][2]+dirs[i][3];
        if(sum!=grid[cells[i]]) return false;
    }

    // own 记每个空格被哪个方向压住：1 是横线，2 是竖线，3 就是交叉。
    // 用"线扫过的每个空格"来判断交叉，而不是看两条边有没有公共格子，
    // 因为一条线可以跨好几个空格，交叉可能发生在中间那格。
    const int dr[4]={-1,1,0,0},dc[4]={0,0,-1,1};
    const int opp[4]={1,0,3,2};   // 上<->下，左<->右
    vector<int> own((size_t)n*m,0);
    for(size_t i=0;i<cells.size();i++){
        int r=cells[i]/m,c=cells[i]%m;
        for(int d=0;d<4;d++){
            if(!dirs[i][d]) continue;
            int rr=r+dr[d],cc=c+dc[d];
            int bit=(d<2)?2:1;
            while(true){
                if(rr<0||rr>=n||cc<0||cc>=m) return false;   // 走出棋盘还没碰到连接器
                int j=id[rr*m+cc];
                if(j>=0){
                    if(!dirs[j][opp[d]]) return false;       // 只有一端声明了这条线
                    break;
                }
                own[rr*m+cc]|=bit;
                if(own[rr*m+cc]==3) return false;            // 横竖压在同一格
                rr+=dr[d];
                cc+=dc[d];
            }
        }
    }
    return true;
}

// 七组内建用例，覆盖题面没给的那些角落：空棋盘、孤立点、最简单的一对、
// 总度数为奇数、唯一铺法就交叉、中心四向全接、以及压到规模上限的全空棋盘。
// want 只分"该有解"和"该无解"两种；有解的还要过一遍 verify，因为
// 判"有解"容易，判"给出的这个解合法"才是真要求。
int runTest(){
    struct Case{const char* name;int n,m;vector<int> grid;bool want;};
    vector<Case> cs={
        {"edge1-empty",3,3,{0,0,0,0,0,0,0,0,0},true},
        {"edge2-single",2,2,{1,0,0,0},false},
        {"edge3-pair",2,3,{1,1,0,0,0,0},true},
        {"edge4-odd",2,3,{1,1,1,0,0,0},false},
        {"edge5-cross",3,3,{0,1,0,1,0,1,0,1,0},false},
        {"edge6-plus",3,3,{0,1,0,1,4,1,0,1,0},true},
        {"edge7-zero50",50,50,vector<int>(2500,0),true},
    };

    int bad=0;
    for(size_t k=0;k<cs.size();k++){
        const Case& c=cs[k];
        Solver s;
        s.init(c.n,c.m,c.grid);
        bool ok=s.solve();
        bool pass=(ok==c.want);
        if(pass&&ok){
            vector<array<int,4>> dirs=s.directions();
            // 连接器按行优先编号，cells 就是那批格子
            vector<int> cells;
            for(int i=0;i<c.n*c.m;i++) if(c.grid[i]>0) cells.push_back(i);
            pass=verify(c.n,c.m,c.grid,dirs,cells);
        }
        printf("%-14s %-10s %s\n",c.name,c.want?"有解":"无解",pass?"ok":"FAIL");
        if(!pass) bad++;
    }
    printf("%d/%d ok\n",(int)cs.size()-bad,(int)cs.size());
    return bad==0?0:1;
}

// 读入。题面的输入小得很（最多 50×50=2500 个数），scanf 足够；
// 读失败直接返回 false，让上层报错退出，不能把空输入当成"无解"——那会静默给错答案。
bool readInput(int& n,int& m,vector<int>& grid){
    if(scanf("%d %d",&n,&m)!=2) return false;
    if(n<=0||m<=0) return false;
    grid.assign((size_t)n*m,0);
    for(int i=0;i<n*m;i++) if(scanf("%d",&grid[i])!=1) return false;
    return true;
}

int main(int argc,char** argv){
    if(argc>1&&strcmp(argv[1],"test")==0) return runTest();

    bool stats=false,prop=true;
    int heur=0;
    // 三个开关只影响"怎么搜"和"打不打统计"，不影响输出格式：
    // --stats 把三个数打到 stderr，--heur 换分支策略，--noprop 关传播。
    // 判题时一个都不会带，所以答案路径上不允许有任何行为依赖这几个值。
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--stats")==0) stats=true;
        else if(strcmp(argv[i],"--heur")==0&&i+1<argc) heur=atoi(argv[++i]);
        else if(strcmp(argv[i],"--noprop")==0) prop=false;
    }

    int n=0,m=0;
    vector<int> grid;
    if(!readInput(n,m,grid)){
        fprintf(stderr,"usage: recover [test] [--stats] [--heur N] [--noprop] < input\n");
        return 1;
    }

    Solver solver;
    solver.init(n,m,grid);
    solver.setHeuristic(heur);
    solver.setPropagation(prop);

    // 计时只包住 solve()。读入和输出不算进去，否则报告里的表会被两万多次 printf 污染。
    // 用 clock() 拿的是进程 CPU 时间：同一台机器上连跑几十组，比墙钟稳，
    // 也不会被后台的其他进程拉长——报告里的秒数都出自这一个口径。
    const clock_t t0=clock();
    const bool ok=solver.solve();
    const double ms=(double)(clock()-t0)*1000.0/CLOCKS_PER_SEC;

    if(!ok){
        printf("No Solution\n");
    }else{
        // 编号本来就是行优先，按编号顺序输出就是题目要的顺序；坐标加 1 是因为内部用 0 基。
        // cells 这里重新扫一遍 grid 得到，而不是问 Solver 要：连接器就是 k>0 的格子，
        // 这条定义写死在题面里，不依赖求解器内部怎么编号，万一哪天编号方式变了也不会错位。
        vector<array<int,4>> dirs=solver.directions();
        vector<int> cells;
        for(int i=0;i<n*m;i++) if(grid[i]>0) cells.push_back(i);
        for(size_t i=0;i<cells.size();i++){
            printf("%d %d %d %d %d %d\n",cells[i]/m+1,cells[i]%m+1,
                   dirs[i][0],dirs[i][1],dirs[i][2],dirs[i][3]);
        }
    }

    // 这几个数只给实验用，全走 stderr，所以带 --stats 跑出来的 stdout 仍然是干净答案，
    // 可以直接喂给校验脚本，不用另写一个只打答案的版本。
    if(stats){
        fprintf(stderr,"nodes=%lld propagate=%lld time_ms=%.3f answer=%s\n",
                solver.nodes(),solver.propagations(),ms,ok?"yes":"no");
    }
    return 0;
}
