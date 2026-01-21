import os
import shutil

# הגדרת התיקייה אליה ייווצרו הטסטים
OUTPUT_DIR = "tdd_tests"

class TestCase:
    def __init__(self, name, code, expected_output):
        self.name = name
        self.code = code
        self.expected_output = expected_output.strip()

def create_tests():
    tests = []

    # ==========================================
    # Phase 1: Arithmetic Expressions (ביטויים חשבוניים)
    # [cite: 137] "התחילו מחישובים פשוטים והתקדמו למורכבים"
    # ==========================================
    
    # 1.1 Basic Operations
    tests.append(TestCase(
        "stage1_01_basic_ops",
        """void main() {
            printi(10 + 20);
            printi(10 - 3);
            printi(5 * 5);
            printi(100 / 2);
        }""",
        "30\n7\n25\n50"
    ))

    # 1.2 Precedence (Sdiv/Mul before Add/Sub)
    tests.append(TestCase(
        "stage1_02_precedence",
        """void main() {
            printi(2 + 3 * 4);
            printi((2 + 3) * 4);
            printi(10 - 2 * 3); 
        }""",
        "14\n20\n4"
    ))

    # 1.3 Byte Truncation (Handling overflow 0-255) [cite: 85]
    tests.append(TestCase(
        "stage1_03_byte_overflow",
        """void main() {
            printi(255b + 1b);
            printi(100b + 200b); 
        }""",
        "0\n44"
    ))

    # 1.4 Division by Zero (Runtime Check) [cite: 77]
    tests.append(TestCase(
        "stage1_04_div_zero",
        """void main() {
            printi(10 / 0);
        }""",
        "Error division by zero"
    ))

    # ==========================================
    # Phase 2: Boolean Expressions (ביטויים בוליאניים)
    # [cite: 138] "חישובים לביטויים בוליאניים מורכבים"
    # הערה: מכיוון שאי אפשר להדפיס bool, נשתמש ב-if בסיסי כדי לבדוק נכונות
    # ==========================================

    # 2.1 Basic Relational Operators
    tests.append(TestCase(
        "stage2_01_relational",
        """void main() {
            if (5 > 3) print("5 > 3");
            if (2 < 4) print("2 < 4");
            if (5 == 5) print("5 == 5");
            if (5 != 3) print("5 != 3");
        }""",
        "5 > 3\n2 < 4\n5 == 5\n5 != 3"
    ))

    # 2.2 Logical Operators (AND/OR/NOT)
    tests.append(TestCase(
        "stage2_02_logical",
        """void main() {
            if (true and true) print("T and T");
            if (true or false) print("T or F");
            if (not false) print("not F");
        }""",
        "T and T\nT or F\nnot F"
    ))

    # 2.3 Short Circuit Evaluation [cite: 87]
    # כדי לבדוק את זה, צריך לוודא שחלק מהביטוי לא מחושב. 
    # זה דורש פונקציות שמדפיסות (נסמך על שלב מתקדם), או בדיקת קוד ה-LLVM ידנית.
    # נשאיר בדיקה לוגית פשוטה:
    tests.append(TestCase(
        "stage2_03_complex_bool",
        """void main() {
            if ((1 == 1) and (2 + 2 == 4)) print("math works");
            if ((1 == 2) or (5 > 2)) print("logic works");
        }""",
        "math works\nlogic works"
    ))

    # ==========================================
    # Phase 3: Local Variables (משתנים במחסנית)
    # [cite: 58-59] "אחסון על מחסנית, alloca"
    # ==========================================

    # 3.1 Basic Declaration and Usage
    tests.append(TestCase(
        "stage3_01_vars",
        """void main() {
            int x = 10;
            int y = 20;
            printi(x);
            printi(y);
            printi(x + y);
        }""",
        "10\n20\n30"
    ))

    # 3.2 Assignment
    tests.append(TestCase(
        "stage3_02_assign",
        """void main() {
            int x = 5;
            x = x + 5;
            printi(x);
        }""",
        "10"
    ))

    # 3.3 Scoping (Block scope)
    tests.append(TestCase(
        "stage3_03_scope",
        """void main() {
            int x = 1;
            {
                int x = 2;
                printi(x);
            }
            printi(x);
        }""",
        "2\n1"
    ))

    # ==========================================
    # Phase 4 & 5: Statements & Control Structures
    # [cite: 139] "מבני בקרה (if, while)"
    # ==========================================

    # 5.1 If-Else
    tests.append(TestCase(
        "stage5_01_if_else",
        """void main() {
            int x = 10;
            if (x > 5) print("big");
            else print("small");
            
            if (x < 5) print("small");
            else print("big again");
        }""",
        "big\nbig again"
    ))

    # 5.2 While Loop
    tests.append(TestCase(
        "stage5_02_while",
        """void main() {
            int i = 0;
            while (i < 3) {
                printi(i);
                i = i + 1;
            }
        }""",
        "0\n1\n2"
    ))

    # 5.3 Break/Continue [cite: 112, 115]
    tests.append(TestCase(
        "stage5_03_break_continue",
        """void main() {
            int i = 0;
            while (i < 10) {
                i = i + 1;
                if (i == 2) continue;
                if (i == 4) break;
                printi(i);
            }
        }""",
        "1\n3"
    ))

    # ==========================================
    # Phase 7: Functions (פונקציות)
    # [cite: 140] "קריאה לפונקציות"
    # ==========================================

    # 7.1 Simple Function Call
    tests.append(TestCase(
        "stage7_01_func_call",
        """
        int add(int a, int b) {
            return a + b;
        }
        void main() {
            printi(add(3, 4));
        }""",
        "7"
    ))

    # 7.2 Recursion (Factorial)
    tests.append(TestCase(
        "stage7_02_recursion",
        """
        int fact(int n) {
            if (n <= 1) return 1;
            return n * fact(n - 1);
        }
        void main() {
            printi(fact(5));
        }""",
        "120"
    ))

    return tests

def write_tests(tests):
    # Create output directory
    if os.path.exists(OUTPUT_DIR):
        shutil.rmtree(OUTPUT_DIR)
    os.makedirs(OUTPUT_DIR)

    print(f"Generating tests in '{OUTPUT_DIR}'...")

    for test in tests:
        # Write .in file
        in_path = os.path.join(OUTPUT_DIR, f"{test.name}.in")
        with open(in_path, "w", newline='\n') as f:
            f.write(test.code)
        
        # Write .out file
        out_path = os.path.join(OUTPUT_DIR, f"{test.name}.out")
        with open(out_path, "w", newline='\n') as f:
            f.write(test.expected_output)
            # Add implicit newline if not empty, similar to how diff expects
            if test.expected_output:
                f.write("\n")
    
    print(f"Successfully generated {len(tests)} tests.")
    print("Don't forget to update your 'run_tests.sh' to point to this directory!")

if __name__ == "__main__":
    tests = create_tests()
    write_tests(tests)