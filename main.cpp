#include "shell.hpp"

using namespace std;

int main() {
    init_signals();
    print_banner();

    while (true) {
        // 1. รับคำสั่งจากผู้ใช้
        string line = read_line();

        // 2. ตรวจสอบว่ามี Pipeline (|) หรือไม่
        vector<Command> pipeline_cmds = parse_pipeline(line);
        if (pipeline_cmds.empty()) continue;

        if (pipeline_cmds.size() > 1) {
            // ถ้ามี | ส่งชุดคำสั่งรัน Pipeline
            execute_pipeline(pipeline_cmds);
        } else {
            Command& cmd = pipeline_cmds[0];
            // 3. ตรวจสอบคำสั่ง Built-in
            if (!check_builtins(cmd)) {
                // ถ้าไม่ใช่ Built-in ส่งต่อให้รัน External Command (fork + execvp)
                launch_process(cmd);
            }
        }
    }
    return 0;
}
