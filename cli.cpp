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

// ฟังก์ชันช่วยแทนที่ $$ ด้วย PID ของเชลล์ปัจจุบัน (เช่น procinfo $$)
static string expand_variables(const string& token) {
    string result = token;
    string pid_str = to_string(getpid());
    size_t pos = 0;
    while ((pos = result.find("$$", pos)) != string::npos) {
        result.replace(pos, 2, pid_str);
        pos += pid_str.length();
    }
    return result;
}

// โครงสร้างช่วยสลับและคืนค่า Standard I/O สำหรับคำสั่ง Built-in ที่ใช้ Redirection
struct RedirGuard {
    int saved_stdin = -1;
    int saved_stdout = -1;
    bool ok = true;

    RedirGuard(const Command& cmd) {
        if (!cmd.input_file.empty() || !cmd.output_file.empty()) {
            saved_stdin = dup(STDIN_FILENO);
            saved_stdout = dup(STDOUT_FILENO);
            if (handle_redirection(cmd) < 0) {
                ok = false;
            }
        }
    }

    ~RedirGuard() {
        if (saved_stdin != -1) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (saved_stdout != -1) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
    }
};

// ============================================================================
// 1. ฟังก์ชันรับคำสั่ง (read_line)
// ============================================================================
string read_line() {
    string line;

    while (true) {
        cout << "myshell$ " << flush;
        if (!getline(cin, line)) {
            if (cin.eof()) {
                cout << "\nexit" << endl;
                exit(EXIT_SUCCESS);
            }
            cin.clear();
            cout << endl;
            continue;
        }
        string trimmed_line = trim(line);
        if (trimmed_line.empty()) continue;

        history_list.push_back(trimmed_line);
        return trimmed_line;
    }
}

// ============================================================================
// 2. ฟังก์ชันตัดคำและคัดแยกตัวดำเนินการ (parse_command & parse_pipeline)
// ============================================================================
Command parse_command(const string& line) {
    Command cmd;
    struct Token {
        string text;
        bool is_operator;
    };
    vector<Token> tokens;
    string current;
    bool in_quotes = false;
    bool had_quotes = false;
    char quote_char = '\0';

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == quote_char) in_quotes = false;
            else current += c;
        } else if (c == '"' || c == '\'') {
            in_quotes = true;
            had_quotes = true;
            quote_char = c;
        } else if (isspace(static_cast<unsigned char>(c))) {
            if (!current.empty() || had_quotes) {
                tokens.push_back({expand_variables(current), false});
                current.clear();
                had_quotes = false;
            }
        } else if (c == '<' || c == '&') {
            if (!current.empty() || had_quotes) {
                tokens.push_back({expand_variables(current), false});
                current.clear();
                had_quotes = false;
            }
            tokens.push_back({string(1, c), true});
        } else if (c == '>') {
            if (!current.empty() || had_quotes) {
                tokens.push_back({expand_variables(current), false});
                current.clear();
                had_quotes = false;
            }
            if (i + 1 < line.size() && line[i + 1] == '>') {
                tokens.push_back({">>", true});
                ++i;
            } else {
                tokens.push_back({">", true});
            }
        } else {
            current += c;
        }
    }
    if (!current.empty() || had_quotes) {
        tokens.push_back({expand_variables(current), false});
    }

    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].is_operator && tokens[i].text == "<") {
            if (i + 1 < tokens.size() && !tokens[i + 1].is_operator) cmd.input_file = tokens[++i].text;
            else cerr << "myshell: syntax error near unexpected token `<'" << endl;
        } else if (tokens[i].is_operator && tokens[i].text == ">") {
            if (i + 1 < tokens.size() && !tokens[i + 1].is_operator) { cmd.output_file = tokens[++i].text; cmd.append = false; }
            else cerr << "myshell: syntax error near unexpected token `>'" << endl;
        } else if (tokens[i].is_operator && tokens[i].text == ">>") {
            if (i + 1 < tokens.size() && !tokens[i + 1].is_operator) { cmd.output_file = tokens[++i].text; cmd.append = true; }
            else cerr << "myshell: syntax error near unexpected token `>>'" << endl;
        } else if (tokens[i].is_operator && tokens[i].text == "&") {
            cmd.is_background = true;
        } else {
            cmd.args.push_back(tokens[i].text);
        }
    }
    return cmd;
}

vector<Command> parse_pipeline(const string& line) {
    vector<Command> pipeline_cmds;
    string segment;
    bool in_quotes = false;
    char quote_char = '\0';

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == quote_char) in_quotes = false;
            segment += c;
        } else if (c == '"' || c == '\'') {
            in_quotes = true;
            quote_char = c;
            segment += c;
        } else if (c == '|') {
            string trimmed = trim(segment);
            if (!trimmed.empty()) pipeline_cmds.push_back(parse_command(trimmed));
            segment.clear();
        } else {
            segment += c;
        }
    }
    string trimmed = trim(segment);
    if (!trimmed.empty()) pipeline_cmds.push_back(parse_command(trimmed));

    return pipeline_cmds;
}

// ============================================================================
// 3. ฟังก์ชันคำสั่งในตัว (check_builtins)
// ============================================================================
bool check_builtins(const Command& cmd) {
    if (cmd.args.empty()) return true;
    const string& command = cmd.args[0];

    if (command == "cd") {
        RedirGuard guard(cmd);
        if (!guard.ok) return true;
        const char* target_dir = nullptr;
        if (cmd.args.size() < 2 || cmd.args[1] == "~") {
            target_dir = getenv("HOME");
            if (!target_dir) { cerr << "myshell: cd: HOME not set" << endl; return true; }
        } else target_dir = cmd.args[1].c_str();
        if (chdir(target_dir) != 0) perror("myshell: cd");
        return true;
    }

    if (command == "pwd") {
        RedirGuard guard(cmd);
        if (!guard.ok) return true;
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) cout << cwd << endl;
        else perror("myshell: pwd");
        return true;
    }

    if (command == "history") {
        RedirGuard guard(cmd);
        if (!guard.ok) return true;
        for (size_t i = 0; i < history_list.size(); ++i) cout << setw(4) << (i + 1) << "  " << history_list[i] << endl;
        return true;
    }

    if (command == "jobs") {
        RedirGuard guard(cmd);
        if (!guard.ok) return true;
        list_jobs();
        return true;
    }

    if (command == "fg") {
        RedirGuard guard(cmd);
        if (!guard.ok) return true;
        if (cmd.args.size() < 2) {
            cerr << "myshell: fg: job_id required" << endl;
        } else {
            try {
                int job_id = stoi(cmd.args[1]);
                builtin_fg(job_id);
            } catch (...) {
                cerr << "myshell: fg: invalid job id" << endl;
            }
        }
        return true;
    }

    // =======================================================
    // เพิ่มคำสั่ง procinfo ของคนที่ 5
    // =======================================================
    if (command == "procinfo") {
        RedirGuard guard(cmd);
        if (!guard.ok) return true;
        int pid = -1;
        if (cmd.args.size() >= 2) {
            try { 
                pid = stoi(cmd.args[1]); 
            } catch (...) { 
                cerr << "myshell: procinfo: invalid pid" << endl; 
                return true; 
            }
        }
        builtin_procinfo(pid);
        return true;
    }

    // =======================================================
    // เพิ่มคำสั่ง mytree ของคนที่ 5
    // =======================================================
    if (command == "mytree") {
        RedirGuard guard(cmd);
        if (!guard.ok) return true;
        builtin_mytree();
        return true;
    }

    if (command == "help") {
        RedirGuard guard(cmd);
        if (!guard.ok) return true;
        cout << "====================================================\n"
             << "              Mini Unix Shell - Help                \n"
             << "====================================================\n"
             << "Built-in Commands:\n"
             << "  cd [dir]       : Change current working directory\n"
             << "  pwd            : Print current working directory\n"
             << "  history        : Show command history\n"
             << "  jobs           : List background jobs\n"
             << "  fg [job_id]    : Bring a job to foreground\n"
             << "  procinfo [pid] : Show process memory & status (Person 5)\n"
             << "  mytree         : Show process hierarchy tree (Person 5)\n"
             << "  help           : Display this help message\n"
             << "  exit           : Exit the shell\n\n"
             << "====================================================" << endl;
        return true;
    }

    if (command == "exit") {
        int exit_code = EXIT_SUCCESS;
        if (cmd.args.size() >= 2) {
            try { exit_code = stoi(cmd.args[1]); }
            catch (...) { cerr << "myshell: exit: " << cmd.args[1] << ": numeric argument required" << endl; exit_code = 2; }
        }
        cout << "Exiting shell..." << endl;
        exit(exit_code);
    }
    return false;
}
