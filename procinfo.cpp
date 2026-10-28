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

    cout << "=== Process Information (PID: " << pid << ") ===" << endl;
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
            
            cout << line << endl;
        }
    }
    cout << "=======================================" << endl;
    file.close();
}