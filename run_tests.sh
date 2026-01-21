#!/bin/bash

# Define color variables for readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Executable name
EXEC_NAME="./hw5"

# Directories
TEST_DIRS=("./segel_tests/")
OUTPUT_DIR="./tests_results/"

# Check for verbose flag
VERBOSE=0
if [[ "$1" == "-v" ]]; then
    VERBOSE=1
fi

# ================= Compile The code =================
echo -e "${BLUE}============== Compiling the code! ==============${NC}"
make
if [[ $? != 0 ]]; then
    echo -e "${RED}Cannot build the code!${NC}"
    exit 1
fi

echo -e "${GREEN}============== The code compiled successfully ! ==============${NC}"

# ================= Setup Directories =================
rm -rf ${OUTPUT_DIR}
mkdir -p ${OUTPUT_DIR}

passed_tests=0
total_tests=0

for TESTS_DIR in "${TEST_DIRS[@]}"; do
    if [ ! -d "$TESTS_DIR" ]; then
        echo -e "${YELLOW}Directory $TESTS_DIR does not exist. Skipping.${NC}"
        continue
    fi

    echo -e "${BLUE}============== Running Tests from ${TESTS_DIR} ==============${NC}"

    # Loop over all .in files in the tests directory
    for test_file in ${TESTS_DIR}*.in; do
        # Check if files exist to avoid error if directory is empty
        [ -e "$test_file" ] || continue

        # Increment total tests
        total_tests=$((total_tests + 1))

        # Extract filename without extension (e.g., "tests/t1.in" -> "t1")
        filename=$(basename -- "$test_file")
        test_name="${filename%.*}"
        
        expected_output="${TESTS_DIR}${test_name}.out"
        actual_output="${OUTPUT_DIR}${test_name}.res"

        # Run the test
        # redirecting stderr to stdout (2>&1) as per instructions
        $EXEC_NAME < "$test_file" > "$actual_output" 2>&1

        # Compare output
        diff_output=$(diff "$expected_output" "$actual_output")
        
        if [ -n "$diff_output" ]; then
            echo -e "${RED}Failed test: ${test_name}!${NC}"
            
            # Check verbose flag to decide whether to print diff
            if [ $VERBOSE -eq 1 ]; then
                echo "Diff:"
                diff -u "$expected_output" "$actual_output"
                echo "------------------------------------------------"
            fi
        else
            echo -e "${GREEN}Test ${test_name} passed!${NC}"
            passed_tests=$((passed_tests + 1))
        fi
    done
done

# ================= Summary =================
echo -e "\n${BLUE}============== Summary ==============${NC}"
if [ $passed_tests -eq $total_tests ] && [ $total_tests -gt 0 ]; then
    echo -e "${GREEN}All tests passed! ($passed_tests/$total_tests)${NC}"
else
    echo -e "${YELLOW}Passed $passed_tests out of $total_tests tests.${NC}"
    if [ $VERBOSE -eq 0 ]; then
        echo -e "${YELLOW}Tip: Run with './run_tests.sh -v' to see the diffs for failed tests.${NC}"
    fi
fi