#include "shell.hpp"
#include <iostream>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cstring>
#include <cerrno>

using namespace std;

// ============================================================================
// 1. ฟังก์ชันจัดการทิศทาง I/O (<, >, >>)
// ถูกเรียกใช้โดยคนที่ 2 (process.cpp) และใช้ใน execute_pipeline ของไฟล์นี้ด้วย
// ============================================================================
int handle_redirection(const Command& cmd) {
    // จัดการ Input Redirection (<)
    if (!cmd.input_file.empty()) {
        int fd_in = open(cmd.input_file.c_str(), O_RDONLY);
        if (fd_in < 0) {
            perror("myshell: input redirection");
            return -1; // แจ้งคนที่ 2 ว่าล้มเหลว
        }
        if (dup2(fd_in, STDIN_FILENO) < 0) {
            perror("myshell: dup2 input");
            close(fd_in);
            return -1;
        }
        close(fd_in);
    }

    // จัดการ Output Redirection (>, >>)
    if (!cmd.output_file.empty()) {
        int flags = O_WRONLY | O_CREAT;
        flags |= cmd.append ? O_APPEND : O_TRUNC; // เช็ค flag ว่าเป็น >> หรือไม่
        
        // กำหนดสิทธิ์ไฟล์เป็น 0644 (rw-r--r--)
        int fd_out = open(cmd.output_file.c_str(), flags, 0644);
        if (fd_out < 0) {
            perror("myshell: output redirection");
            return -1;
        }
        if (dup2(fd_out, STDOUT_FILENO) < 0) {
            perror("myshell: dup2 output");
            close(fd_out);
            return -1;
        }
        close(fd_out);
    }
    
    return 0; // สำเร็จ
}

// ============================================================================
// 2. ฟังก์ชันหลักสำหรับรันคำสั่งหลายตัวที่ต่อท่อ (Pipeline)
// ============================================================================
int execute_pipeline(const vector<Command>& pipeline_cmds) {
    if (pipeline_cmds.empty()) return 0;

    // ถ้ามีคำสั่งเดียว (ไม่มี |) โยนให้ฟังก์ชันของคนที่ 2 จัดการได้เลย
    if (pipeline_cmds.size() == 1) {
        return launch_process(pipeline_cmds[0]);
    }

    int num_cmds = pipeline_cmds.size();
    int prev_pipe_read = -1; // เก็บปลายท่ออ่านของรอบก่อนหน้า
    vector<pid_t> child_pids;
    bool is_bg = pipeline_cmds.back().is_background;

    // บล็อก SIGCHLD ชั่วคราวเพื่อป้องกัน Race Condition กับ sigchld_handler
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    for (int i = 0; i < num_cmds; ++i) {
        int fd[2];
        
        // สร้างท่อ (pipe) เฉพาะคำสั่งที่ไม่ใช่ตัวสุดท้าย
        if (i < num_cmds - 1) {
            if (pipe(fd) < 0) {
                perror("myshell: pipe");
                sigprocmask(SIG_SETMASK, &prev_mask, nullptr);
                return -1;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("myshell: fork in pipeline");
            sigprocmask(SIG_SETMASK, &prev_mask, nullptr);
            return -1;
        }

        if (pid == 0) { 
            // ==========================================
            // โปรเซสลูก (Child Process)
            // ==========================================
            sigprocmask(SIG_SETMASK, &prev_mask, nullptr);

            if (is_bg) {
                setpgid(0, 0);
            }

            // คืนค่า Signal ให้กลับเป็นปกติ (เหมือนที่คนที่ 2 ทำ)
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);

            // 1. รับข้อมูลจากท่อก่อนหน้า
            if (prev_pipe_read != -1) {
                dup2(prev_pipe_read, STDIN_FILENO);
                close(prev_pipe_read);
            }

            // 2. ส่งข้อมูลลงท่อถัดไป (ถ้าไม่ใช่คำสั่งสุดท้าย)
            if (i < num_cmds - 1) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }

            // 3. จัดการ Redirection ของคำสั่งย่อย (เผื่อมี ls | grep txt > out.txt)
            if (handle_redirection(pipeline_cmds[i]) < 0) {
                _exit(EXIT_FAILURE);
            }

            if (pipeline_cmds[i].args.empty()) {
                _exit(EXIT_SUCCESS);
            }

            // รองรับคำสั่ง Built-in ใน Pipeline (เช่น help | grep cd)
            if (check_builtins(pipeline_cmds[i])) {
                _exit(EXIT_SUCCESS);
            }

            // 4. เตรียมข้อมูลส่งให้ execvp
            vector<char*> argv;
            for (const string& arg : pipeline_cmds[i].args) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            // 5. สั่งรันคำสั่ง
            execvp(argv[0], argv.data());
            
            // ถ้าหลุดมาถึงนี่แสดงว่ารันไม่ผ่าน
            cerr << "myshell: " << argv[0] << ": " << strerror(errno) << endl;
            _exit(errno == ENOENT ? 127 : 126);
        } else {
            // ==========================================
            // โปรเซสแม่ (Parent Process)
            // ==========================================
            child_pids.push_back(pid);

            // ปิดปลายท่ออ่านของลูปที่แล้ว
            if (prev_pipe_read != -1) {
                close(prev_pipe_read);
            }

            // ปิดฝั่งเขียนของรอบนี้ (แม่ไม่ได้เขียน) และเก็บฝั่งอ่านไว้ให้ลูกรอบถัดไป
            if (i < num_cmds - 1) {
                close(fd[1]);
                prev_pipe_read = fd[0];
            }
        }
    }

    // ==========================================
    // ขั้นตอนที่ 3: รอให้ลูกในท่อทำงานเสร็จ
    // ==========================================
    int last_status = 0;

    if (is_bg) {
        // งานเบื้องหลัง: สร้าง Job
        string full_cmd = "";
        for (size_t i = 0; i < pipeline_cmds.size(); ++i) {
            for (const auto& arg : pipeline_cmds[i].args) full_cmd += arg + " ";
            if (i < pipeline_cmds.size() - 1) full_cmd += "| ";
        }
        add_job(child_pids.back(), full_cmd, "Running"); 
        sigprocmask(SIG_SETMASK, &prev_mask, nullptr);
        return 0;
    } else {
        // วนรอโปรเซสลูกทั้งหมด
        for (size_t i = 0; i < child_pids.size(); ++i) {
            int status = 0;
            pid_t result;
            
            // ใช้เงื่อนไขกันสัญญาณขัดจังหวะ (แบบเดียวกับคนที่ 2)
            do {
                result = waitpid(child_pids[i], &status, WUNTRACED);
            } while (result < 0 && errno == EINTR); 

            // แกะค่า Status เฉพาะคำสั่งสุดท้ายของ Pipeline ส่งกลับคืน
            if (i == child_pids.size() - 1 && result > 0) {
                if (WIFSTOPPED(status)) {
                    string full_cmd = "";
                    for (size_t j = 0; j < pipeline_cmds.size(); ++j) {
                        for (const auto& arg : pipeline_cmds[j].args) full_cmd += arg + " ";
                        if (j < pipeline_cmds.size() - 1) full_cmd += "| ";
                    }
                    add_job(child_pids[i], full_cmd, "Stopped");
                }
                last_status = handle_child_status(child_pids[i], status);
            }
        }
    }

    sigprocmask(SIG_SETMASK, &prev_mask, nullptr);
    return last_status;
}
