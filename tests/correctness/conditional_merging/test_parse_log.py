import regex as re
import sys

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def get_correct_selected_loops(test_name):
    if (test_name == "cond_merge_test_O1"): 
        return "97 " #Notice the space after the loops, and loops being in ascending order
    if (test_name == "cond_merge_test_O2"): 
        return "104 " 
    else:
        print(f"Unrecognized test name: {test_name}")
        return "WRONG TEST NAME"

def get_native_time(test_name):
    try:
        file = open("native_running_times.txt", "r")
    except OSError:
        return 0.0
    last_time_match = None
    for line in file:

        m = re.search(rf"NATIVE TEST NAME: {test_name}", line)
        if m:
            last_test_name_match = m
            last_assertion_match = None #If the test finished correctly, the assertion results will come after the test name
            last_time_match = None
            last_loop_selection_match = None
        
        m = re.search(r"Finished in (\d*\.\d*) seconds \(real\) (\d*\.\d*) seconds \(proc\)", line)
        if m and last_time_match == None: #We find the first time match after the test name
            last_time_match = m

    if (last_time_match == None):
        return 0.0
    else:
        return float(last_time_match.group(1))


#Scans the entire log file and counts how many total and how many successful tests there are
def print_all_test_summary():
    file = open("test_log.txt", "r")
    expr = re.compile(r"TEST NAME: (.*)")
    expr2 = re.compile(r"(\d*) tests, (\d*) assertions, 0 failures")
    total_test_count = 0
    successful_test_count = 0
    for line in file:

        m = expr.findall(line)
        if (len(m) >= 1): total_test_count += 1

        m = expr2.findall(line)
        if (len(m) >= 1): successful_test_count += 1

    print(f"{bcolors.BOLD}------     SUMMARY     ------{bcolors.ENDC}");
    print(f"{bcolors.BOLD}Count of run tests: {total_test_count}{bcolors.ENDC}")
    fails = total_test_count - successful_test_count
    if (fails == 0):
        print(f"{bcolors.OKGREEN}Count of fails: 0{bcolors.ENDC}\n")
    else:
        print(f"{bcolors.FAIL}Count of fails: {fails}{bcolors.ENDC}")


#Finds the last test result in the log file and prints whether it passed or failed
def print_last_test_result():
    last_test_name_match =      None
    last_assertion_match =      None 
    last_loop_selection_match = None
    last_time_match =           None
    file = open("test_log.txt", "r")
    for line in file:

        m = re.search(r"TEST NAME: (.*)", line)
        if m:
            last_test_name_match = m
            last_assertion_match = None #If the test finished correctly, the assertion results will come after the test name
            last_time_match = None
            last_loop_selection_match = None
        m = re.search(r"(\d*) tests, (\d*) assertions, (\d*) failures", line)
        if m:
            last_assertion_match = m
        
        m = re.search(r"Finished in (\d*\.\d*) seconds \(real\) (\d*\.\d*) seconds \(proc\)", line)
        if m:
            last_time_match = m
        
        m = re.search(r"Final selected DOALL loops: ((\d|\s)*)\n", line)
        if m:
            last_loop_selection_match = m
        
    try:
        test_name = last_test_name_match.group(1)
        #print("------- Test: {} -------".format(test_name))
        selected_loops = last_loop_selection_match.group(1)
        if (selected_loops != get_correct_selected_loops(test_name)):
            print (f"{bcolors.WARNING}Warning: possibly wrong loops selected ({selected_loops}instead of {get_correct_selected_loops(test_name)}){bcolors.ENDC}")

        num_fails = int(last_assertion_match.group(3))
        if (num_fails == 0):
            print (f"{bcolors.OKGREEN}          PASSED!          {bcolors.ENDC}")
        elif (num_fails > 0):
            print (f"{bcolors.FAIL}          FAILED!            {bcolors.ENDC} with {num_fails} failed correctness checks! Check test_log.txt!")
        else:
            print("REGEX PARSING ERROR")
        real_time = float(last_time_match.group(1))
        proc_time = float(last_time_match.group(2))
        print("Real time: {0:.3f}s, processor time: {1:.3f}s".format(real_time, proc_time))
        native_time = get_native_time(test_name)
        print("Native time: {0:.3f}s, parallel speedup relative to native: {1:.2f}x".format(native_time, native_time/real_time))
    except Exception as e:
        print(f"{bcolors.FAIL}ERROR{bcolors.ENDC}: the test may have failed! Check test_log.txt!")
        print(e)
    print("\n")

if (__name__ == "__main__"):
    if len(sys.argv) == 2 and sys.argv[1] == "-s": 
        print_all_test_summary() #Only print a short summary of all tests
    else:
        print_last_test_result()
