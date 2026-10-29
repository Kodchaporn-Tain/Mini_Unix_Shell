#include "shell.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace std;

// ==========================================================
// ฟังก์ชันดึงข้อมูลโปรเซส (คำสั่ง procinfo <pid>)
// ==========================================================
void builtin_procinfo(int pid) {
    if (pid == -1) {
        pid = getpid();
    }

    string path = "/proc/" + to_string(pid) + "/status";
    ifstream file(path);

    if (!file.is_open()) {
        cerr << "myshell: procinfo: Cannot find process with PID " << pid << endl;
        return;
    }

    cout << CLR_CYAN << "=== Process Information (PID: " << pid << ") ===" << CLR_RESET << endl;
    string line;
    while (getline(file, line)) {
        if (line.find("Name:") == 0 || 
            line.find("State:") == 0 || 
            line.find("PPid:") == 0 || 
            line.find("VmSize:") == 0 ||
            line.find("VmRSS:") == 0 ||
            line.find("Threads:") == 0 ||
            line.find("voluntary_ctxt_switches:") == 0 ||
            line.find("nonvoluntary_ctxt_switches:") == 0) {
            
            size_t colon = line.find(':');
            if (use_color() && colon != string::npos) {
                cout << CLR_YELLOW << line.substr(0, colon + 1) << CLR_GREEN << line.substr(colon + 1) << CLR_RESET << endl;
            } else {
                cout << line << endl;
            }
        }
    }
    cout << CLR_CYAN << "=======================================" << CLR_RESET << endl;
    file.close();
}