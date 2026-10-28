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

using namespace std;

// โครงสร้างข้อมูลคำสั่งที่ใช้ร่วมกัน
struct Command {
    vector<string> args;
    string input_file = "";
    string output_file = "";
    bool append = false;
    bool is_background = false;
};

// โครงสร้างสำหรับเก็บงานเบื้องหลัง
struct Job {
    int job_id;
    pid_t pid;
    string cmd_line;
    bool is_active;
};

string read_line();
Command parse_command(const string& line);
vector<Command> parse_pipeline(const string& line);
bool check_builtins(const Command& cmd);

int launch_process(const Command& cmd);
int launch_standard_process(const Command& cmd);
int handle_child_status(pid_t pid, int status);

int handle_redirection(const Command& cmd);
int execute_pipeline(const vector<Command>& pipeline_cmds);

void init_signals();
void add_job(pid_t pid, const string& cmd_line);
void list_jobs();
int builtin_fg(int job_id);

void builtin_procinfo(int pid);
void builtin_mytree();

#endif