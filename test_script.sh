#!/bin/bash
# ==============================================================================
# test_script.sh - สคริปต์ทดสอบระบบอัตโนมัติสำหรับ Mini Unix Shell (myshell)
# ครอบคลุมกรณีทดสอบทั้ง 8 หัวข้อตามข้อกำหนดวิชา Operating Systems
# ==============================================================================

# หากผู้ใช้รันด้วยคำสั่ง `sh test_script.sh` ให้สลับมารันด้วย `bash` อัตโนมัติ
if [ -z "${BASH_VERSION:-}" ]; then
    exec bash "$0" "$@"
fi

set -u

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS_COUNT=0
FAIL_COUNT=0

print_header() {
    echo -e "\n${CYAN}================================================================${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}================================================================${NC}"
}

check_result() {
    local test_name="$1"
    local condition="$2"
    if eval "$condition"; then
        echo -e "${GREEN}[PASS]${NC} $test_name"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}[FAIL]${NC} $test_name"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
}

# 0. ตรวจสอบและคอมไพล์โปรแกรม
print_header "0. Compiling Mini Unix Shell (make)"
make clean > /dev/null 2>&1
if make; then
    echo -e "${GREEN}[PASS]${NC} Compilation succeeded"
else
    echo -e "${RED}[FAIL]${NC} Compilation failed"
    exit 1
fi

MYSHELL="$(pwd)/myshell"

# 1. ทดสอบ Built-in Commands (cd, pwd, history, help, exit)
print_header "1. ทดสอบ Built-in Commands (cd .., pwd, history, help)"
EXPECTED_PARENT_DIR="$(cd .. && pwd)"
OUT_BUILTIN=$("$MYSHELL" << 'EOF'
pwd
cd ..
pwd
history
help
exit
EOF
)
echo "$OUT_BUILTIN"
check_result "cd .. และ pwd เปลี่ยนและแสดงไดเรกทอรีถูกต้อง" 'echo "$OUT_BUILTIN" | grep -Fq "$EXPECTED_PARENT_DIR"'
check_result "history แสดงประวัติคำสั่งย้อนหลังถูกต้อง" 'echo "$OUT_BUILTIN" | grep -Fq "cd .."'
check_result "help แสดงคู่มือคำสั่งถูกต้อง" 'echo "$OUT_BUILTIN" | grep -Fq "Built-in Commands:"'

# 2. ทดสอบ External Commands (ls -la, echo Hello World)
print_header "2. ทดสอบ External Commands (ls -la, echo Hello World)"
OUT_EXT=$("$MYSHELL" << 'EOF'
echo Hello World
ls -la
exit
EOF
)
echo "$OUT_EXT" | head -n 15
check_result "echo Hello World ทำงานถูกต้อง" 'echo "$OUT_EXT" | grep -Fq "Hello World"'
check_result "ls -la แสดงรายการไฟล์ในไดเรกทอรีได้" 'echo "$OUT_EXT" | grep -Fq "main.cpp"'

# 3. ทดสอบ Error Handling (invalid_command)
print_header "3. ทดสอบ Error Handling (invalid_command)"
OUT_ERR=$("$MYSHELL" 2>&1 << 'EOF'
invalid_command
echo Still Alive After Error
exit
EOF
)
echo "$OUT_ERR"
check_result "แจ้งเตือนเมื่อไม่พบคำสั่ง invalid_command" 'echo "$OUT_ERR" | grep -Eiq "invalid_command.*(No such file|not found)"'
check_result "เชลล์ไม่ Crash และทำงานต่อได้หลังเกิด Error" 'echo "$OUT_ERR" | grep -Fq "Still Alive After Error"'

# 4. ทดสอบ I/O Redirection (>, >>, <)
print_header "4. ทดสอบ Redirection (ls -l > out.txt, cat < out.txt, >>)"
rm -f out.txt
OUT_REDIR=$("$MYSHELL" << 'EOF'
ls -l > out.txt
echo APPEND_TEST_LINE >> out.txt
cat < out.txt
exit
EOF
)
echo "$OUT_REDIR" | tail -n 10
check_result "สร้างไฟล์ด้วย > และอ่านกลับด้วย < สำเร็จ" 'echo "$OUT_REDIR" | grep -Fq "Makefile"'
check_result "เขียนต่อท้ายไฟล์ด้วย >> สำเร็จ" 'echo "$OUT_REDIR" | grep -Fq "APPEND_TEST_LINE"'
rm -f out.txt

# 5. ทดสอบ Pipeline (cat /etc/passwd | cut -d: -f1 | sort)
print_header "5. ทดสอบ Pipe (cat /etc/passwd | cut -d: -f1 | sort)"
OUT_PIPE=$("$MYSHELL" << 'EOF'
cat /etc/passwd | cut -d: -f1 | sort
exit
EOF
)
echo "$OUT_PIPE" | head -n 10
check_result "Pipeline 3 ต่อ (cat | cut | sort) ทำงานถูกต้อง" 'echo "$OUT_PIPE" | grep -Fq "root"'

# 6. ทดสอบ Background (&), jobs, fg และ Zombie Prevention
print_header "6. ทดสอบ Background (&), jobs, fg และ Zombie Prevention (<defunct>)"
OUT_BG=$("$MYSHELL" << 'EOF'
sleep 2 &
jobs
sleep 3
ps aux
sleep 5 &
fg 2
exit
EOF
)
echo "$OUT_BG" | grep -E "(\[1\]|\[2\]|Running|Done|sleep [25]|<defunct>)" || true
check_result "รันคำสั่งเบื้องหลัง (&) และแสดงใน jobs ว่า Running" 'echo "$OUT_BG" | grep -Eq "\[1\].*Running.*sleep 2"'
check_result "ไม่มี Zombie Process (<defunct>) ค้างหลังจากโปรเซสเบื้องหลังจบ" '! echo "$OUT_BG" | grep -Fq "sleep 2 <defunct>"'
check_result "ดึงงานกลับมาเบื้องหน้าด้วย fg สำเร็จ" 'echo "$OUT_BG" | grep -Fq "sleep 5"'

# 7. ทดสอบ Signals (Ctrl+C / SIGINT เฉพาะโปรเซสลูกหยุด แต่เชลล์หลักไม่ดับ)
print_header "7. ทดสอบ Signals (ส่ง SIGINT ให้โปรเซสลูกที่กำลังรัน โดยเชลล์หลักไม่ดับ)"
FIFO_PATH="/tmp/myshell_test_fifo_$$"
rm -f "$FIFO_PATH"
mkfifo "$FIFO_PATH"

"$MYSHELL" < "$FIFO_PATH" > /tmp/myshell_sig_out_$$ 2>&1 &
SHELL_PID=$!
exec 3>"$FIFO_PATH"

# สั่งรัน sleep 20 ในเชลล์
echo "sleep 20" >&3
sleep 1

# ค้นหา PID ของ sleep 20 ที่เป็นลูกของ myshell แล้วส่ง SIGINT (เหมือนกด Ctrl+C)
CHILD_SLEEP_PID=$(pgrep -P "$SHELL_PID" sleep || true)
if [ -n "$CHILD_SLEEP_PID" ]; then
    kill -INT "$CHILD_SLEEP_PID" 2>/dev/null || true
    # ส่ง SIGINT ให้ตัว myshell เองด้วยเพื่อพิสูจน์ว่า myshell เพิกเฉยต่อ SIGINT
    kill -INT "$SHELL_PID" 2>/dev/null || true
fi
sleep 1

# สั่งพิมพ์ข้อความต่อเพื่อยืนยันว่าเชลล์หลักยังไม่ดับ
echo "echo SHELL_SURVIVED_SIGINT" >&3
echo "exit" >&3
exec 3>&-
wait "$SHELL_PID" 2>/dev/null || true

OUT_SIG=$(cat /tmp/myshell_sig_out_$$)
rm -f "$FIFO_PATH" /tmp/myshell_sig_out_$$
echo "$OUT_SIG"
check_result "โปรเซสลูก (sleep 20) หยุดทำงานเมื่อได้รับ SIGINT" 'echo "$OUT_SIG" | grep -Fq "terminated by signal 2"'
check_result "เชลล์หลักไม่ดับเมื่อได้รับ SIGINT และรับคำสั่งต่อได้" 'echo "$OUT_SIG" | grep -Fq "SHELL_SURVIVED_SIGINT"'

# 8. ทดสอบ Inspector (procinfo $$ และ mytree)
print_header "8. ทดสอบ Inspector (procinfo \$\$ และ mytree)"
OUT_INSP=$("$MYSHELL" << 'EOF'
procinfo $$
mytree
exit
EOF
)
echo "$OUT_INSP" | head -n 30
check_result "procinfo \$\$ ดึงข้อมูล PCB (State, VmSize, ctxt_switches) ได้ถูกต้อง" 'echo "$OUT_INSP" | grep -Fq "State:" && echo "$OUT_INSP" | grep -Fq "voluntary_ctxt_switches:"'
check_result "mytree แสดงแผนผังต้นไม้โปรเซสได้สำเร็จ" 'echo "$OUT_INSP" | grep -Fq "=== Process Tree ===" && echo "$OUT_INSP" | grep -Fq "myshell"'

# สรุปผล
print_header "สรุปผลการทดสอบ (Test Summary)"
echo -e "${GREEN}ผ่าน (Passed): $PASS_COUNT${NC}"
echo -e "${RED}ไม่ผ่าน (Failed): $FAIL_COUNT${NC}"

if [ "$FAIL_COUNT" -eq 0 ]; then
    echo -e "\n${GREEN}>>> ระบบทำงานครบถ้วนสมบูรณ์ผ่านทุกกรณีทดสอบ! <<<${NC}"
    exit 0
else
    echo -e "\n${RED}>>> มีบางกรณีทดสอบไม่ผ่าน กรุณาตรวจสอบด้านบน <<<${NC}"
    exit 1
fi

