import os
import shutil

# תיקיית היעד
OUTPUT_DIR = "tdd_tests"

class TestCase:
    def __init__(self, name, code, expected_output):
        self.name = name
        self.code = code
        self.expected_output = expected_output.strip()

def create_tests():
    tests = []

    # =========================================================================
    # GROUP 1: Arithmetic & Types (Deep Dive)
    # =========================================================================

    # 1.1: Byte Truncation (Mod 256)
    # [cite: 85] "תוצאת פעולה חשבונית מסוג byte... על ידי truncation"
    tests.append(TestCase(
        "test_01_byte_overflow",
        """void main() {
            byte b = 255b;
            printi(b);
            b = b + 1b; // Should wrap to 0
            printi(b);
            b = 100b;
            b = b * 3b; // 300 -> 44 (300 % 256)
            printi(b);
        }""",
        "255\n0\n44"
    ))

    # 1.2: Mixed Types (Byte + Int)
    # [cite: 74] "השוואות... יתייחסו לערכים המספריים עצמם"
    # [cite: 357] "טיפוס החזרה... הוא הטיפוס עם טווח הייצוג הגדול יותר"
    tests.append(TestCase(
        "test_02_mixed_types",
        """void main() {
            byte b = 10b;
            int i = 20;
            // Byte promotes to Int, result is Int
            printi(b + i); 
            // Check huge number with byte
            int res = b + 1000; 
            printi(res);
        }""",
        "30\n1010"
    ))

    # 1.3: Signed Integers & Negative Math
    # [cite: 72] "הטיפוס המספרי int הינו signed"
    tests.append(TestCase(
        "test_03_negative_math",
        """void main() {
            int x = 0 - 5; // -5
            printi(x);
            printi(x * x); // 25
            printi(x + 10); // 5
            printi(20 / x); // -4
        }""",
        "-5\n25\n5\n-4"
    ))

    # 1.4: Division by Zero (Complex Expression)
    # [cite: 77] "יש לממש שגיאת חלוקה באפס"
    tests.append(TestCase(
        "test_04_div_zero_complex",
        """void main() {
            int x = 5;
            int y = 5;
            printi(100 / (x - y));
        }""",
        "Error division by zero"
    ))

    # 1.5: Operator Precedence (Mul/Div before Add/Sub)
    tests.append(TestCase(
        "test_05_arith_precedence",
        """void main() {
            printi(2 + 3 * 4);     // 14
            printi((2 + 3) * 4);   // 20
            printi(10 - 20 / 2);   // 0
            printi(10 * 2 + 5 * 3);// 35
        }""",
        "14\n20\n0\n35"
    ))

    # =========================================================================
    # GROUP 2: Boolean Logic & Short Circuit
    # =========================================================================

    # 2.1: Short Circuit OR
    #  "במידה וניתן לקבוע... אין להמשיך לחשב"
    tests.append(TestCase(
        "test_06_short_circuit_or",
        """
        bool check() {
            print("Should not see this");
            return true;
        }
        void main() {
            // true OR ... -> second part skipped
            if (true or check()) {
                print("SC OR Works");
            }
        }""",
        "SC OR Works"
    ))

    # 2.2: Short Circuit AND
    tests.append(TestCase(
        "test_07_short_circuit_and",
        """
        bool check() {
            print("Should not see this");
            return true;
        }
        void main() {
            // false AND ... -> second part skipped
            if (false and check()) {
                print("Bad");
            } else {
                print("SC AND Works");
            }
        }""",
        "SC AND Works"
    ))

    # 2.3: Nested Logic & Precedence (AND before OR usually, but strictly left-to-right in some simplistic parsers, check standard C rules)
    # Standard C: AND > OR.
    tests.append(TestCase(
        "test_08_bool_precedence",
        """void main() {
            // true OR false AND false -> true OR (false) -> true
            if (true or false and false) print("Pass 1");
            
            // false AND true OR true -> (false) OR true -> true
            if (false and true or true) print("Pass 2");
            
            // not false AND false -> true AND false -> false
            if (not false and false) print("Fail"); else print("Pass 3");
        }""",
        "Pass 1\nPass 2\nPass 3"
    ))

    # 2.4: Relational Operators Correctness
    tests.append(TestCase(
        "test_09_relational",
        """void main() {
            if (10 > 5) print("GT");
            if (5 < 10) print("LT");
            if (5 >= 5) print("GE");
            if (5 <= 5) print("LE");
            if (5 == 5) print("EQ");
            if (5 != 6) print("NE");
        }""",
        "GT\nLT\nGE\nLE\nEQ\nNE"
    ))

    # =========================================================================
    # GROUP 3: Variable Lifecycle & Scoping
    # =========================================================================

    # 3.1: Default Initialization
    # [cite: 66-67] "הטיפוסים המספריים יאותחלו ל-0... הבוליאני יאותחל ל-false"
    tests.append(TestCase(
        "test_10_default_init",
        """void main() {
            int i;
            byte b;
            bool z;
            printi(i); // 0
            printi(b); // 0
            if (z) print("Bad"); else print("False");
        }""",
        "0\n0\nFalse"
    ))

    # 3.2: Scoping and Inner Blocks (Access Outer, Unique Inner)
    tests.append(TestCase(
        "test_11_scoping_rules",
        """void main() {
            int x = 10;
            {
                int y = 20;
                printi(x); // Access outer
                printi(y); // Access inner
                x = 30;    // Modify outer
            }
            printi(x); // Modified value
            // y is gone here
        }""",
        "10\n20\n30\n30"
    ))

    # 3.3: Variable Shadowing Check (Is it allowed? HW3 says NO shadowing).
    # HW3[cite: 327]: "אסור להכריז על משתנה... שכבר מוגדר באותו ה-scope או באב קדמון"
    # This means the Semantic Analyzer should catch it.
    # If your compiler passes semantic checks, we assume valid code for CodeGen.
    # We will SKIP shadowing tests here as they cause compilation errors, not runtime output.

    # =========================================================================
    # GROUP 4: Control Flow (Complex)
    # =========================================================================

    # 4.1: While Loop Logic
    tests.append(TestCase(
        "test_12_while_loop",
        """void main() {
            int i = 3;
            while (i > 0) {
                printi(i);
                i = i - 1;
            }
        }""",
        "3\n2\n1"
    ))

    # 4.2: Break and Continue
    # [cite: 113] "Break... המשפט הבא אחרי הלולאה"
    # [cite: 116] "Continue... קפיצה לתנאי הלולאה"
    tests.append(TestCase(
        "test_13_break_continue",
        """void main() {
            int i = 0;
            while (i < 5) {
                i = i + 1;
                if (i == 2) continue; // Skip printing 2
                if (i == 4) break;    // Stop at 4 (don't print 4)
                printi(i);
            }
            print("Done");
        }""",
        "1\n3\nDone"
    ))

    # 4.3: Nested Loops & Break/Continue scopes
    tests.append(TestCase(
        "test_14_nested_loops",
        """void main() {
            int i = 0;
            while (i < 2) {
                int j = 0;
                print("Outer");
                while (j < 3) {
                    j = j + 1;
                    if (j == 2) break; // Break INNER loop only
                    printi(j);
                }
                i = i + 1;
            }
        }""",
        "Outer\n1\nOuter\n1"
    ))

    # 4.4: If-Else Chain
    tests.append(TestCase(
        "test_15_if_else_chain",
        """void main() {
            int x = 50;
            if (x < 10) print("Low");
            else if (x < 40) print("Mid");
            else if (x < 60) print("Target");
            else print("High");
        }""",
        "Target"
    ))

    # =========================================================================
    # GROUP 5: Functions & Recursion
    # =========================================================================

    # 5.1: Argument Evaluation Order
    # [cite: 96] "ישוערכו... משמאל לימין"
    tests.append(TestCase(
        "test_16_arg_eval_order",
        """
        int f1() { print("1"); return 1; }
        int f2() { print("2"); return 2; }
        void foo(int a, int b) { print("call"); }
        
        void main() {
            foo(f1(), f2());
        }""",
        "1\n2\ncall"
    ))

    # 5.2: Implicit Return
    # [cite: 103] "הערך שיוחזר יהיה ערך ברירת המחדל"
    tests.append(TestCase(
        "test_17_implicit_return",
        """
        int getZero() { 
            int x = 5; 
            // No return statement
        }
        bool getFalse() {
            // No return statement
        }
        void main() {
            printi(getZero());
            if (getFalse()) print("Bad"); else print("Good");
        }""",
        "0\nGood"
    ))

    # 5.3: Recursion (Factorial)
    tests.append(TestCase(
        "test_18_recursion_fact",
        """
        int fact(int n) {
            if (n <= 1) return 1;
            return n * fact(n - 1);
        }
        void main() {
            printi(fact(5)); // 120
        }""",
        "120"
    ))

    # 5.4: Recursion (Fibonacci - multiple recursive calls)
    tests.append(TestCase(
        "test_19_recursion_fib",
        """
        int fib(int n) {
            if (n == 0) return 0;
            if (n == 1) return 1;
            return fib(n-1) + fib(n-2);
        }
        void main() {
            printi(fib(6)); // 8
        }""",
        "8"
    ))

    # 5.5: Mutual Recursion (Odd/Even)
    # [cite: 338] "ניתן לקרוא לפונקציה גם לפני הגדרתה"
    tests.append(TestCase(
        "test_20_mutual_recursion",
        """
        bool isEven(int n); // Forward decl handling
        
        bool isOdd(int n) {
            if (n == 0) return false;
            return isEven(n - 1);
        }
        
        bool isEven(int n) {
            if (n == 0) return true;
            return isOdd(n - 1);
        }
        
        void main() {
            if (isOdd(3)) print("3 is odd");
            if (isEven(2)) print("2 is even");
        }""",
        "3 is odd\n2 is even"
    ))

    # =========================================================================
    # GROUP 6: Complex Integration Tests
    # =========================================================================

    # 6.1: Complex Arithmetic + Logic
    tests.append(TestCase(
        "test_21_complex_expr",
        """void main() {
            int a = 5;
            int b = 10;
            if ((a * 2 == b) and (b / 2 == a)) {
                print("Math matches logic");
            }
        }""",
        "Math matches logic"
    ))

    # 6.2: Loop with Logic and Arithmetic
    tests.append(TestCase(
        "test_22_loop_sum",
        """void main() {
            int sum = 0;
            int i = 1;
            while (i <= 5) {
                sum = sum + i;
                i = i + 1;
            }
            printi(sum); // 1+2+3+4+5 = 15
        }""",
        "15"
    ))

    return tests

def write_tests(tests):
    # Create output directory
    if os.path.exists(OUTPUT_DIR):
        shutil.rmtree(OUTPUT_DIR)
    os.makedirs(OUTPUT_DIR)

    print(f"Generating {len(tests)} comprehensive tests in '{OUTPUT_DIR}'...")

    for test in tests:
        # Write .in file
        in_path = os.path.join(OUTPUT_DIR, f"{test.name}.in")
        with open(in_path, "w", newline='\n') as f:
            f.write(test.code)
        
        # Write .out file
        out_path = os.path.join(OUTPUT_DIR, f"{test.name}.out")
        with open(out_path, "w", newline='\n') as f:
            f.write(test.expected_output)
            # Add implicit newline if not empty
            if test.expected_output:
                f.write("\n")
    
    print("Done! Don't forget to update your 'run_tests.sh' to point to this directory.")

if __name__ == "__main__":
    tests = create_tests()
    write_tests(tests)