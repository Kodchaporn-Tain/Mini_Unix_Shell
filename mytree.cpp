#include "shell.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <dirent.h>
#include <algorithm>

using namespace std;

// ==========================================================
// 1. ฟังก์ชันช่วยสำหรับพิมพ์ mytree แบบ Recursive
// ==========================================================
void print_tree(int pid, const string& prefix, bool is_last, bool is_root,
                unordered_map<int, vector<int>>& children, 
                unordered_map<int, string>& names) {
    
    cout << prefix;
    if (!is_root) {
        cout << (is_last ? "\\-- " : "|-- ") << pid << " [" << names[pid] << "]" << endl;
    } else {
        if (pid == 0) {
            cout << "System (Root)" << endl;
        } else {
            cout << pid << " [" << names[pid] << "]" << endl;
        }
    }

    string new_prefix = prefix;
    if (!is_root) {
        new_prefix += (is_last ? "    " : "|   ");
    }

    if (children.find(pid) != children.end()) {
        auto& kids = children[pid];
        sort(kids.begin(), kids.end());
        for (size_t i = 0; i < kids.size(); ++i) {
            print_tree(kids[i], new_prefix, (i == kids.size() - 1), false, children, names);
        }
    }
}

// ==========================================================
// 2. ฟังก์ชันสร้าง Process Tree (คำสั่ง mytree)
// ==========================================================
void builtin_mytree() {
    unordered_map<int, vector<int>> children;
    unordered_map<int, string> names;

    DIR* dir = opendir("/proc");
    if (!dir) {
        cerr << "myshell: mytree: Cannot open /proc" << endl;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string folder_name = entry->d_name;
        if (folder_name.empty()) continue;
        
        bool is_pid = true;
        for (char c : folder_name) {
            if (!isdigit(static_cast<unsigned char>(c))) {
                is_pid = false;
                break;
            }
        }

        if (is_pid) {
            int pid = stoi(folder_name);
            string stat_path = string("/proc/") + folder_name + "/stat";
            ifstream stat_file(stat_path);
            
            if (stat_file.is_open()) {
                string line;
                if (getline(stat_file, line)) {
                    // ชื่อโปรเซสใน /proc/[pid]/stat อยู่ในวงเล็บ (...) และอาจมีช่องว่างหรือวงเล็บซ้อนได้
                    size_t open_paren = line.find('(');
                    size_t close_paren = line.rfind(')');
                    if (open_paren != string::npos && close_paren != string::npos && close_paren > open_paren) {
                        string comm = line.substr(open_paren + 1, close_paren - open_paren - 1);
                        istringstream iss(line.substr(close_paren + 1));
                        string state;
                        int ppid = 0;
                        if (iss >> state >> ppid) {
                            names[pid] = comm;
                            children[ppid].push_back(pid);
                        }
                    }
                }
                stat_file.close();
            }
        }
    }
    closedir(dir);

    cout << "=== Process Tree ===" << endl;
    // เริ่มจาก Root Process (systemd / init: pid = 1) หากมีในระบบ มิฉะนั้นเริ่มจาก 0
    if (names.find(1) != names.end()) {
        print_tree(1, "", true, true, children, names);
    } else {
        print_tree(0, "", true, true, children, names);
    }
    cout << "====================" << endl;
}
