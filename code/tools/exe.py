# 找可执行文件：脚本既能在完整仓库里跑（exe 在 build/），也能在只有 code/ 的
# 提交包里跑（按 Readme 用 g++ 编出 code/recover.exe）。顺序就是优先级。
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

CANDIDATES = [
    os.path.join(HERE, "..", "recover.exe"),          # 只有 code/：在 code/ 里 g++ 编出来
    os.path.join(HERE, "..", "..", "build", "recover.exe"),  # 完整仓库：CMake 产物
    os.path.join(HERE, "..", "..", "recover.exe"),
    os.path.join(HERE, "recover.exe"),
]


def find_exe(argv=None, flag="--exe"):
    """返回 exe 路径；显式给了 --exe 就用它，没找到就报错并退出。"""
    argv = sys.argv[1:] if argv is None else argv
    if flag in argv:
        return argv[argv.index(flag) + 1]
    env = os.environ.get("RECOVER_EXE")
    if env:
        return env
    for path in CANDIDATES:
        if os.path.exists(path):
            return path
    sys.exit("找不到 recover.exe：先在 code/ 里编（见 code/Readme.txt），或用 --exe 指定路径")
