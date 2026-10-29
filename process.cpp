#include "shell.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>

using namespace std;

// ============================================================================
// ฟังก์ชันช่วย: ประกอบ args กลับเป็นข้อความบรรทัดเดียว (ใช้ส่งให้ add_job)
// ============================================================================
static string join_args(const vector<string>& args) {
    string result;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) result += " ";
        result += args[i];
    }
    return result;
}

// ============================================================================
// 2. handle_child_status: แกะ Exit Status ที่ได้จาก waitpid()
// ----------------------------------------------------------------------------
// ค่า status ที่ waitpid คืนมาเป็น bitfield ต้องใช้ macro ในการแกะ:
//   WIFEXITED   -> โปรเซสจบเองตามปกติ (return จาก main / เรียก exit)
//   WEXITSTATUS -> ดึงรหัสที่โปรเซสส่งกลับ (0 = สำเร็จ)
//   WIFSIGNALED -> โปรเซสถูกฆ่าด้วยสัญญาณ (เช่น SIGSEGV, SIGKILL)
//   WTERMSIG    -> หมายเลขสัญญาณที่ฆ่า
// คืนค่า: exit code แบบเดียวกับ shell จริง (ถ้าโดนสัญญาณฆ่า = 128 + เลขสัญญาณ)
// ============================================================================
int handle_child_status(pid_t pid, int status) {
    if (WIFEXITED(status)) {
        // จบแบบปกติ: คืนรหัสที่โปรเซสลูกส่งมา
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        cerr << "myshell: process " << pid << " terminated by signal "
             << sig << " (" << strsignal(sig) << ")";
#ifdef WCOREDUMP
        if (WCOREDUMP(status)) cerr << " (core dumped)";
#endif
        cerr << endl;
        return 128 + sig;
    }

    if (WIFSTOPPED(status)) {
        // ถูกหยุดชั่วคราว (เช่น Ctrl+Z) -- ไว้ให้คนที่ 4 ต่อยอดเรื่อง job control
        cerr << "myshell: process " << pid << " stopped by signal "
             << WSTOPSIG(status) << endl;
        return 128 + WSTOPSIG(status);
    }

    return -1;  // สถานะที่ไม่รู้จัก
}

// ============================================================================
// 1. launch_standard_process: fork() + execvp() + waitpid()
// ----------------------------------------------------------------------------
// คืนค่า: exit code ของคำสั่ง (โหมด foreground) / 0 (โหมด background) / -1 ถ้า fork ล้มเหลว
// ============================================================================
int launch_standard_process(const Command& cmd) {
    if (cmd.args.empty()) return 0;

    // บล็อก SIGCHLD ชั่วคราวก่อน fork เพื่อป้องกัน Race Condition กับ sigchld_handler
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    // --- ขั้นที่ 1: fork() สร้างโปรเซสลูกที่เป็นสำเนาของ shell ---
    pid_t pid = fork();

    if (pid < 0) {
        // fork ล้มเหลว (เช่น โปรเซสเต็มระบบ / หน่วยความจำไม่พอ)
        perror("myshell: fork");
        sigprocmask(SIG_SETMASK, &prev_mask, nullptr);
        return -1;
    }

    if (pid == 0) {
        // ================= โค้ดส่วนนี้ทำงานใน "โปรเซสลูก" =================
        sigprocmask(SIG_SETMASK, &prev_mask, nullptr);

        // ถ้าเป็นงานเบื้องหลัง แยก Process Group เพื่อไม่ให้โดน Ctrl+C / Ctrl+Z จากหน้าจอหลัก
        if (cmd.is_background) {
            setpgid(0, 0);
        }

        // คืนค่า signal ให้เป็นค่าเริ่มต้น เพราะ shell อาจ ignore SIGINT/SIGTSTP ไว้
        // (ถ้าไม่ทำ คำสั่งที่รันจะกด Ctrl+C หยุดไม่ได้)
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        // จัดการ <, >, >> (ฟังก์ชันของคนที่ 3 ซึ่งใช้ open + dup2 ใน child)
        // ต้องทำก่อน exec เพราะ file descriptor จะถูกสืบทอดข้ามการ exec
        if (handle_redirection(cmd) < 0) {
            _exit(EXIT_FAILURE);
        }

        // แปลง vector<string> เป็น char*[] ที่ลงท้ายด้วย nullptr ตามที่ execvp ต้องการ
        vector<char*> argv;
        for (const string& arg : cmd.args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        // execvp: ค้นหาคำสั่งใน $PATH แล้ว "แทนที่" โปรเซสนี้ด้วยโปรแกรมใหม่
        // ถ้าสำเร็จ จะไม่มีวัน return กลับมาบรรทัดถัดไป
        execvp(argv[0], argv.data());

        // มาถึงตรงนี้ได้ = exec ล้มเหลว (เช่น ไม่พบคำสั่ง)
        cerr << "myshell: " << argv[0] << ": " << strerror(errno) << endl;
        // ใช้ _exit ไม่ใช่ exit เพื่อไม่ให้ flush buffer ซ้ำซ้อนกับของ parent
        // 127 = command not found (ตามธรรมเนียมของ shell)
        _exit(errno == ENOENT ? 127 : 126);
    }

    // ================= โค้ดส่วนนี้ทำงานใน "โปรเซสแม่ (shell)" =================

    if (cmd.is_background) {
        setpgid(pid, pid);
        // งานเบื้องหลัง: ไม่รอ แต่ลงทะเบียนให้คนที่ 4 ติดตาม
        // (การเก็บศพ/กัน Zombie ของงานเบื้องหลังให้ใช้ SIGCHLD handler ใน init_signals ของคนที่ 4)
        add_job(pid, join_args(cmd.args), "Running");
        sigprocmask(SIG_SETMASK, &prev_mask, nullptr);
        return 0;
    }

    // --- ขั้นที่ 2: waitpid() รอลูกจบ หรือถูกหยุดด้วย Ctrl+Z (WUNTRACED) ---
    // ถ้าไม่ wait ลูกที่จบแล้วจะค้างเป็น Zombie (<defunct>) ในตาราง process
    int status = 0;
    pid_t result;
    do {
        result = waitpid(pid, &status, WUNTRACED);
    } while (result < 0 && errno == EINTR);  // ถูกสัญญาณขัดจังหวะ -> รอใหม่

    if (result < 0) {
        perror("myshell: waitpid");
        sigprocmask(SIG_SETMASK, &prev_mask, nullptr);
        return -1;
    }

    // ถ้าถูกหยุดด้วย Ctrl+Z (WIFSTOPPED) ให้เก็บเข้าตาราง Job ในสถานะ Stopped
    if (WIFSTOPPED(status)) {
        add_job(pid, join_args(cmd.args), "Stopped");
    }

    sigprocmask(SIG_SETMASK, &prev_mask, nullptr);

    // --- ขั้นที่ 3: แกะ exit status ---
    return handle_child_status(pid, status);
}

// ============================================================================
// launch_process: จุดเชื่อมที่ main.cpp เรียกใช้ (ตาม shell.hpp)
// ============================================================================
int launch_process(const Command& cmd) {
    return launch_standard_process(cmd);
}
