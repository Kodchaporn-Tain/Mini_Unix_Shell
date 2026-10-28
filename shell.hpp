#ifndef SHELL_HPP
#define SHELL_HPP

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

// โครงสร้างข้อมูลคำสั่งที่ใช้ร่วมกัน
struct Command {
    std::vector<std::string> args;
    std::string input_file = "";
    std::string output_file = "";
    bool append = false;
    bool is_background = false;
};

// โครงสร้างสำหรับเก็บงานเบื้องหลัง
struct Job {
    int job_id;
    pid_t pid;
    std::string cmd_line;
    bool is_active;
};

std::string read_line();
Command parse_command(const std::string& line);
bool check_builtins(const Command& cmd);

int launch_process(const Command& cmd);

int handle_redirection(const Command& cmd);
int execute_pipeline(const std::vector<Command>& pipeline_cmds);

void init_signals();
void add_job(pid_t pid, const std::string& cmd_line);
void list_jobs();
int builtin_fg(int job_id);

void print_procinfo(pid_t pid);
void print_process_tree();

#endif