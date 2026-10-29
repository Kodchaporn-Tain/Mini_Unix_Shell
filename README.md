# Mini Unix Shell (`myshell`)

โปรเจกต์จำลองการทำงานของ Unix Shell พัฒนาด้วยภาษา C++ (C++11) และ POSIX/Linux System Calls สำหรับรายวิชา **Operating Systems (ระบบปฏิบัติการ)** ครอบคลุมการทำงานแบบ REPL, การสร้างและจัดการโปรเซส, การเปลี่ยนทิศทาง I/O และไปป์, การควบคุมงานเบื้องหลังและสัญญาณ รวมถึงการตรวจสอบข้อมูลโปรเซสผ่าน `/proc`

---

## 1. โครงสร้างไฟล์และการแบ่งงานของสมาชิก (Project Structure & Team Roles)

```text
Mini_Unix_Shell/
├── shell.hpp          # Header กลาง: เก็บ struct Command, struct Job และ Prototypes ฟังก์ชันทั้งหมด
├── main.cpp           # Loop หลักของเชลล์ (REPL: Read -> Parse -> Built-in check -> Execute)
├── cli.cpp            # [คนที่ 1] ฟังก์ชันรับข้อความ (read_line), ตัดคำ (parse_command), คำสั่ง Built-in (parser)
├── process.cpp        # [คนที่ 2] ฟังก์ชันสร้างและควบคุมโปรเซสภายนอก (launch_process, waitpid)
├── pipeline.cpp       # [คนที่ 3] ฟังก์ชันจัดการ Redirect (<, >, >>) และท่อส่งข้อมูล (pipe_redir)
├── signals.cpp        # [คนที่ 4] ฟังก์ชันจัดการ Background Process (&), jobs, fg และ Signal Handlers (jobs)
├── procinfo.cpp       # [คนที่ 5] ฟังก์ชันตรวจสอบ PCB จาก /proc/[pid]/status (procinfo)
├── mytree.cpp         # [คนที่ 5] ฟังก์ชันวาดแผนผังต้นไม้โปรเซสจาก /proc (mytree)
├── Makefile           # สคริปต์คอมไพล์โปรเจกต์อัตโนมัติ (make, make test, make clean)
├── test_script.sh     # Bash script สำหรับรันชุดคำสั่งทดสอบระบบอัตโนมัติครบ 8 กรณี
└── README.md          # เอกสารคู่มือการติดตั้ง ใช้งาน และการแบ่งงานของสมาชิก
```

### รายละเอียดความรับผิดชอบของสมาชิกทั้ง 5 คน

| ลำดับ | ไฟล์ที่รับผิดชอบ | ขอบเขตงานและ System Calls ที่ใช้ |
| :--- | :--- | :--- |
| **คนที่ 1** | `cli.cpp`, `main.cpp` | **Command Line & Built-ins:** วนลูปแสดง Prompt `myshell$ `, รับคำสั่งด้วย `std::getline` (ป้องกัน Buffer Overflow), แยกคำสั่ง/เครื่องหมายคำพูด/ตัวแปร `$$`, และจัดการคำสั่ง Built-in (`cd`, `pwd`, `history`, `help`, `exit`) ด้วย `chdir()`, `getcwd()` |
| **คนที่ 2** | `process.cpp` | **Process Management:** สร้าง Child Process ด้วย `fork()`, โหลดโปรแกรมภายนอกด้วย `execvp()`, รอโปรเซสลูกด้วย `waitpid()`, และแกะสถานะการจบทำงานด้วย `WIFEXITED`, `WEXITSTATUS`, `WIFSIGNALED`, `WIFSTOPPED` |
| **คนที่ 3** | `pipeline.cpp` | **I/O Redirection & Pipes:** เปลี่ยนทิศทางอินพุต/เอาต์พุต (`<`, `>`, `>>`) ด้วย `open()` และ `dup2()`, เชื่อมต่อโปรเซสผ่านท่อส่งข้อมูลหลายชั้น (`|`) ด้วย `pipe()` พร้อมปิด File Descriptors ที่ไม่ได้ใช้ |
| **คนที่ 4** | `signals.cpp` | **Job Control & Signals:** รองรับการรันงานเบื้องหลัง (`&`), ตารางเก็บงาน (`jobs`) แสดงสถานะ `Running`/`Stopped`/`Done`, คำสั่งดึงงานกลับเบื้องหน้า (`fg <job_id>`), ดักจับ `SIGINT` (`Ctrl+C`), `SIGTSTP` (`Ctrl+Z`), และเคลียร์ Zombie Process อัตโนมัติผ่าน `SIGCHLD` ด้วย `waitpid(-1, &status, WNOHANG)` |
| **คนที่ 5** | `procinfo.cpp`, `mytree.cpp` | **System & PCB Inspector:** คำสั่ง `procinfo [pid]` อ่านข้อมูล PCB จาก `/proc/[pid]/status` (State, PPid, VmSize, VmRSS, Threads, Context Switches) และคำสั่ง `mytree` สแกน `/proc/[pid]/stat` ด้วย `opendir()`/`readdir()` เพื่อวาดแผนผัง Process Hierarchy Tree เริ่มจาก Root Process (`systemd` PID 1) |

---

## 2. การติดตั้งและคอมไพล์ (Installation & Compilation)

ต้องรันบนระบบปฏิบัติการ **Linux** หรือ **WSL (Windows Subsystem for Linux)** ที่มี `g++` และ `make`:

```bash
# 1. คอมไพล์โปรเจกต์
make

# 2. รันโปรแกรมเชลล์
./myshell

# 3. ลบไฟล์ Object และ Binary ที่คอมไพล์ไว้
make clean
```

---

## 3. คู่มือการใช้งานคำสั่ง (Supported Commands)

### 3.1 คำสั่งภายใน (Built-in Commands)
* `cd [dir]` : เปลี่ยนไดเรกทอรีปัจจุบัน (หากไม่ระบุ `dir` หรือระบุ `~` จะกลับไปยัง `$HOME`)
* `pwd` : แสดงเส้นทางไดเรกทอรีปัจจุบัน
* `history` : แสดงประวัติคำสั่งที่พิมพ์ในเซสชันปัจจุบันทั้งหมด
* `jobs` : แสดงรายการโปรเซสที่ทำงานอยู่เบื้องหลังพร้อมสถานะ (`Running`, `Stopped`)
* `fg <job_id>` : ดึงโปรเซสเบื้องหลังจากหมายเลข `job_id` กลับมาทำงานเบื้องหน้า
* `procinfo [pid]` : แสดงข้อมูล PCB และหน่วยความจำของโปรเซส (รองรับ `procinfo $$` สำหรับเช็คตัวเชลล์เอง)
* `mytree` : แสดงแผนผังต้นไม้ความสัมพันธ์ของโปรเซสทั้งหมดในระบบ
* `help` : แสดงตารางช่วยเหลือของคำสั่งทั้งหมด
* `exit [code]` : ออกจากโปรแกรมเชลล์พร้อมคืนค่าสถานะ

### 3.2 คำสั่งภายนอก, Redirection, Pipeline และ Background
* **External Command:** `ls -la`, `echo Hello World`
* **Input/Output Redirection:**
  * `ls -l > out.txt` (เขียนทับไฟล์)
  * `echo "more text" >> out.txt` (เขียนต่อท้ายไฟล์)
  * `cat < out.txt` (อ่านข้อมูลเข้าจากไฟล์)
* **Multi-stage Pipeline:**
  * `cat /etc/passwd | cut -d: -f1 | sort`
* **Background Execution:**
  * `sleep 10 &`

---

## 4. การรันชุดทดสอบอัตโนมัติ (Automated Testing)

สามารถรันชุดทดสอบทั้ง 8 กรณีตามข้อกำหนดได้ด้วยคำสั่งเดียว:

```bash
chmod +x test_script.sh
./test_script.sh
# หรือใช้คำสั่ง:
make test
```

### กรณีทดสอบที่เตรียมไว้สาธิตในวันตรวจรับงาน
1. **ทดสอบ Built-in:** `cd ..` -> `pwd` -> `history`
2. **ทดสอบ External Command:** `ls -la` และ `echo Hello World`
3. **ทดสอบ Error:** พิมพ์ `invalid_command` (เชลล์แจ้งเตือนว่าไม่พบคำสั่งและไม่ Crash)
4. **ทดสอบ Redirection:** `ls -l > out.txt` ตามด้วย `cat < out.txt`
5. **ทดสอบ Pipe:** `cat /etc/passwd | cut -d: -f1 | sort`
6. **ทดสอบ Background & Zombie Prevention:** `sleep 10 &` -> `jobs` -> `ps aux` (ตรวจสอบว่าไม่มี `<defunct>`)
7. **ทดสอบ Signals:** รัน `sleep 20` แล้วกด `Ctrl+C` (เฉพาะ `sleep` หยุดทำงาน แต่เชลล์หลักไม่ดับ)
8. **ทดสอบ Inspector:** `procinfo $$` และ `mytree`

