#include "shell.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

using namespace std;

// ตัวแปร Global สำหรับเก็บรายการ Jobs เบื้องหลัง
static vector<Job> jobs;
static int next_job_id = 1;

// Signal handler สำหรับจัดการ SIGCHLD
void sigchld_handler(int sig) {
    int saved_errno = errno;
    int status;
    pid_t pid;

    // เคลียร์ Zombie process ทั้งหมดที่หยุดการทำงานแล้วด้วย WNOHANG
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (auto& job : jobs) {
            if (job.pid == pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    job.is_active = false;
                    cout << "\n[" << job.job_id << "] Done\t" << job.cmd_line << endl;
                } else if (WIFSTOPPED(status)) {
                    cout << "\n[" << job.job_id << "] Stopped\t" << job.cmd_line << endl;
                } else if (WIFCONTINUED(status)) {
                    cout << "\n[" << job.job_id << "] Continued\t" << job.cmd_line << endl;
                }
                break;
            }
        }
    }
    
    // ลบ Job ที่สถานะไม่ active ออกจาก list
    jobs.erase(remove_if(jobs.begin(), jobs.end(), [](const Job& j) { return !j.is_active; }), jobs.end());
    
    errno = saved_errno;
}

// ฟังก์ชันเริ่มต้นตั้งค่า Signals
void init_signals() {
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; 

    // ดักจับ SIGCHLD เพื่อเคลียร์ Zombie อัตโนมัติ
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction SIGCHLD");
    }

    // เพิกเฉย (Ignore) สัญญาณ Ctrl+C และ Ctrl+Z ในโปรเซสแม่ (Shell)
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
}

// ฟังก์ชันเพิ่ม Job ใหม่เข้าระบบ (ทำงานเมื่อมี & ต่อท้าย)
void add_job(pid_t pid, const string& cmd_line) {
    Job new_job;
    new_job.job_id = next_job_id++;
    new_job.pid = pid;
    new_job.cmd_line = cmd_line;
    new_job.is_active = true;
    jobs.push_back(new_job);

    cout << "[" << new_job.job_id << "] " << new_job.pid << endl;
}

// ฟังก์ชันแสดงรายการ Jobs (เทียบเท่าคำสั่ง jobs)
void list_jobs() {
    for (const auto& job : jobs) {
        if (job.is_active) {
            cout << "[" << job.job_id << "] " << job.pid << " Running\t" << job.cmd_line << endl;
        }
    }
}

// ฟังก์ชันดึง Job กลับมาทำแบบ Foreground (คำสั่ง fg)
int builtin_fg(int job_id) {
    for (auto& job : jobs) {
        if (job.job_id == job_id && job.is_active) {
            pid_t pid = job.pid;
            string cmd = job.cmd_line;
            
            cout << cmd << endl;

            // ส่งสัญญาณ SIGCONT เผื่อโปรเซสติดสถานะ Stopped อยู่
            kill(pid, SIGCONT);

            // รอรับสถานะแบบ Foreground
            int status;
            pid_t res;
            do {
                res = waitpid(pid, &status, WUNTRACED);
            } while (res < 0 && errno == EINTR);

            if (res > 0) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    job.is_active = false;
                }
                // นำสถานะที่ได้ส่งไปแกะข้อมูล (ใช้งานฟังก์ชันจากคนที่ 2)
                return handle_child_status(pid, status);
            }
            return 0;
        }
    }
    cerr << "myshell: fg: job " << job_id << " not found" << endl;
    return -1;
}