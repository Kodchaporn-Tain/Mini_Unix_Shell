#include "shell.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <dirent.h>
#include <algorithm>

using namespace std;

// ==========================================================
// 1. ฟังก์ชันช่วยสำหรับพิมพ์ mytree แบบ Recursive
// ==========================================================
void print_tree(int pid, const string& prefix, bool is_last, 
                unordered_map<int, vector<int>>& children, 
                unordered_map<int, string>& names) {
    
    cout << prefix;
    if (pid != 0) {
        cout << (is_last ? "\\-- " : "|-- ") << pid << " [" << names[pid] << "]" << endl;
    } else {
        cout << "System (Root)" << endl;
    }

    string new_prefix = prefix;
    if (pid != 0) {
        new_prefix += (is_last ? "    " : "|   ");
    }

    if (children.find(pid) != children.end()) {
        auto& kids = children[pid];
        sort(kids.begin(), kids.end());
        for (size_t i = 0; i < kids.size(); ++i) {
            print_tree(kids[i], new_prefix, (i == kids.size() - 1), children, names);
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
        
        bool is_pid = true;
        for (char c : folder_name) {
            if (!isdigit(c)) {
                is_pid = false;
                break;
            }
        }

        if (is_pid) {
            int pid = stoi(folder_name);
            string stat_path = string("/proc/") + folder_name + "/stat";
            ifstream stat_file(stat_path);
            
            if (stat_file.is_open()) {
                string pid_str, comm, state, ppid_str;
                stat_file >> pid_str >> comm >> state >> ppid_str;
                
                if (comm.front() == '(' && comm.back() == ')') {
                    comm = comm.substr(1, comm.length() - 2);
                }
                
                int ppid = stoi(ppid_str);
                names[pid] = comm;
                children[ppid].push_back(pid);
                
                stat_file.close();
            }
        }
    }
    closedir(dir);

    cout << "=== Process Tree ===" << endl;
    print_tree(0, "", true, children, names);
    cout << "====================" << endl;
}