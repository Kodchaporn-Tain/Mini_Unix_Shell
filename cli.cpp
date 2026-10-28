#include "shell.hpp"
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <iomanip>

using namespace std;

// เก็บประวัติคำสั่งที่ผู้ใช้เคยพิมพ์ทั้งหมดในเซสชันปัจจุบัน
static vector<string> history_list;

// ฟังก์ชันช่วยตัดช่องว่างหัว-ท้ายข้อความ (Trim)
static string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// ============================================================================
// 1. ฟังก์ชันรับคำสั่ง (read_line)
// ============================================================================
string read_line() {
    string line;

    while (true) {
        // แสดงข้อความ Prompt บนหน้าจอ
        cout << "myshell$ " << flush;

        // อ่านข้อความทั้งบรรทัดแบบยืดหยุ่นด้วย getline (ป้องกัน Buffer Overflow)
        if (!getline(cin, line)) {
            // ดักจับกรณีผู้ใช้กด Ctrl+D (End-of-File: EOF)
            if (cin.eof()) {
                cout << "\nexit" << endl;
                exit(EXIT_SUCCESS);
            }
            // กรณีถูกขัดจังหวะด้วยสัญญาณ (เช่น Ctrl+C) ให้ล้างสถานะแล้ววนรับใหม่
            cin.clear();
            cout << endl;
            continue;
        }

        // ตัดช่องว่างหัว-ท้ายเพื่อตรวจสอบกรณีผู้ใช้กด Enter เปล่า
        string trimmed_line = trim(line);
        if (trimmed_line.empty()) {
            // ถ้ากด Enter เปล่า ให้วนรอรับคำสั่งใหม่
            continue;
        }

        // บันทึกคำสั่งลงในประวัติ (history)
        history_list.push_back(trimmed_line);
        return trimmed_line;
    }
}

// ============================================================================
// 2. ฟังก์ชันตัดคำและคัดแยกตัวดำเนินการ (parse_command & parse_pipeline)
// ============================================================================

// แยกคำสั่งและตัวดำเนินการพิเศษ (<, >, >>, &) ออกจากบรรทัดคำสั่งเดี่ยว
Command parse_command(const string& line) {
    Command cmd;
    vector<string> tokens;
    string current;
    bool in_quotes = false;
    char quote_char = '\0';

    // ขั้นที่ 1: Tokenize แยกคำและตัวดำเนินการพิเศษ (<, >, >>, &)
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (in_quotes) {
            if (c == quote_char) {
                in_quotes = false;
            } else {
                current += c;
            }
        } else if (c == '"' || c == '\'') {
            in_quotes = true;
            quote_char = c;
        } else if (isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else if (c == '<' || c == '&') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(string(1, c));
        } else if (c == '>') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            if (i + 1 < line.size() && line[i + 1] == '>') {
                tokens.push_back(">>");
                ++i; // ข้าม '>' ตัวที่สอง
            } else {
                tokens.push_back(">");
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }

    // ขั้นที่ 2: คัดแยก Arguments และตัวดำเนินการพิเศษลงในโครงสร้าง Command
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "<") {
            // ดักจับ < บันทึกชื่อไฟล์นำเข้าสำหรับส่งให้คนที่ 3
            if (i + 1 < tokens.size()) {
                cmd.input_file = tokens[++i];
            } else {
                cerr << "myshell: syntax error near unexpected token `<'" << endl;
            }
        } else if (tokens[i] == ">") {
            // ดักจับ > บันทึกชื่อไฟล์ส่งออก (โหมดเขียนทับ) ส่งให้คนที่ 3
            if (i + 1 < tokens.size()) {
                cmd.output_file = tokens[++i];
                cmd.append = false;
            } else {
                cerr << "myshell: syntax error near unexpected token `>'" << endl;
            }
        } else if (tokens[i] == ">>") {
            // ดักจับ >> บันทึกชื่อไฟล์ส่งออก (โหมดเขียนต่อท้าย) ส่งให้คนที่ 3
            if (i + 1 < tokens.size()) {
                cmd.output_file = tokens[++i];
                cmd.append = true;
            } else {
                cerr << "myshell: syntax error near unexpected token `>>'" << endl;
            }
        } else if (tokens[i] == "&") {
            // ดักจับ & บันทึกแฟล็กให้รันงานแบบเบื้องหลัง ส่งให้คนที่ 4
            cmd.is_background = true;
        } else {
            // คำสั่งหลักและ Arguments ย่อย
            cmd.args.push_back(tokens[i]);
        }
    }

    return cmd;
}

// ดักจับ | เพื่อแยกกลุ่มชุดคำสั่งสำหรับ Pipeline ส่งให้คนที่ 3 (execute_pipeline)
vector<Command> parse_pipeline(const string& line) {
    vector<Command> pipeline_cmds;
    string segment;
    bool in_quotes = false;
    char quote_char = '\0';

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == quote_char) {
                in_quotes = false;
            }
            segment += c;
        } else if (c == '"' || c == '\'') {
            in_quotes = true;
            quote_char = c;
            segment += c;
        } else if (c == '|') {
            string trimmed = trim(segment);
            if (!trimmed.empty()) {
                pipeline_cmds.push_back(parse_command(trimmed));
            }
            segment.clear();
        } else {
            segment += c;
        }
    }

    string trimmed = trim(segment);
    if (!trimmed.empty()) {
        pipeline_cmds.push_back(parse_command(trimmed));
    }

    return pipeline_cmds;
}

// ============================================================================
// 3. ฟังก์ชันคำสั่งในตัว (check_builtins)
// ============================================================================
bool check_builtins(const Command& cmd) {
    if (cmd.args.empty()) {
        return true;
    }

    const string& command = cmd.args[0];

    // 1) cd: เปลี่ยนโฟลเดอร์การทำงานด้วย System Call chdir()
    if (command == "cd") {
        const char* target_dir = nullptr;

        if (cmd.args.size() < 2 || cmd.args[1] == "~") {
            target_dir = getenv("HOME");
            if (!target_dir) {
                cerr << "myshell: cd: HOME not set" << endl;
                return true;
            }
        } else {
            target_dir = cmd.args[1].c_str();
        }

        if (chdir(target_dir) != 0) {
            perror("myshell: cd");
        }
        return true;
    }

    // 2) pwd: ดึงและแสดง Absolute Path ปัจจุบันผ่านฟังก์ชัน getcwd()
    if (command == "pwd") {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            cout << cwd << endl;
        } else {
            perror("myshell: pwd");
        }
        return true;
    }

    // 3) history: แสดงรายการคำสั่งที่เคยพิมพ์ย้อนหลัง
    if (command == "history") {
        for (size_t i = 0; i < history_list.size(); ++i) {
            cout << setw(4) << (i + 1) << "  " << history_list[i] << endl;
        }
        return true;
    }

    // 4) help: แสดงรายชื่อคำสั่งและคู่มือการใช้งานคร่าวๆ
    if (command == "help") {
        cout << "====================================================\n"
             << "              Mini Unix Shell - Help                \n"
             << "====================================================\n"
             << "Built-in Commands:\n"
             << "  cd [dir]       : Change current working directory\n"
             << "  pwd            : Print current working directory\n"
             << "  history        : Show command history\n"
             << "  help           : Display this help message\n"
             << "  exit           : Exit the shell\n\n"
             << "Supported Operators:\n"
             << "  cmd < file     : Redirect standard input from file\n"
             << "  cmd > file     : Redirect standard output to file (overwrite)\n"
             << "  cmd >> file    : Redirect standard output to file (append)\n"
             << "  cmd1 | cmd2    : Pipe output of cmd1 to input of cmd2\n"
             << "  cmd &          : Run command in the background\n"
             << "====================================================" << endl;
        return true;
    }

    // 5) exit: คืนค่าและสั่งยุติการทำงานของเชลล์
    if (command == "exit") {
        int exit_code = EXIT_SUCCESS;
        if (cmd.args.size() >= 2) {
            try {
                exit_code = stoi(cmd.args[1]);
            } catch (...) {
                cerr << "myshell: exit: " << cmd.args[1] << ": numeric argument required" << endl;
                exit_code = 2;
            }
        }
        cout << "Exiting shell..." << endl;
        exit(exit_code);
    }

    // ไม่ใช่คำสั่ง Built-in ส่งค่า false กลับไปเพื่อให้ระบบส่งต่อให้คนที่ 2 (fork + execvp)
    return false;
}
