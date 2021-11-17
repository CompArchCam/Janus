#!/usr/bin/python3
import sys, os
import subprocess, signal

builtins = ["time", "jpar"]
testargs = []

def lex(lnstring):
    return lnstring.replace('\n', '').split(' ')

def parseAndRun(tokenLst, startIndex):
    i = startIndex
    while i < len(tokenLst) and not (tokenLst[i][0] == '[' and tokenLst[i][-1] == ']'):
        i+=1
    if i >= len(tokenLst):
        runCmd(tokenLst)
    else:
        numLst = parseRng(tokenLst[i])
        for j in numLst:
            tokenLst[i] = str(j)
            parseAndRun(tokenLst, i+1)

def runCmd(cmd):
    here = os.getcwd()
    if(cmd[0] in builtins):
        if(cmd[0] == "time"):
            exedirname, exefile = os.path.split(os.path.abspath(cmd[1]))
            os.chdir(exedirname)
            cmd[1] = "./"+exefile

        cmd = ["sh", "-c"] + [" ".join(cmd)]

    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output = proc.stdout.decode("utf-8") + proc.stderr.decode("utf-8")
        print(output)
        ret = proc.returncode
        if ret == -signal.SIGSEGV:
            print("SEG FAULT in: "+cmd, end="")
    except FileNotFoundError:
        print("Error - couldn't find file: "+cmd[0])

    os.chdir(here)

def parseRng(rngStr):
    # parse range of the form [a-b], where a and b may be >1 digit long
    i = 0
    num1Str = ""
    num2Str = ""
    numStrLst = []
    while(rngStr[i] == '['):
        i+=1
    while(rngStr[i] != '-' and rngStr[i] != ']'):
        num1Str += rngStr[i]
        i+=1
    i+=1
    while(rngStr[i] != ']'):
        num2Str += rngStr[i]
        i+=1
    for j in range(int(num1Str), int(num2Str)+1, 1):
        numStrLst.append(str(j))
    return numStrLst

if(len(sys.argv) <= 1):
    print("Usage: "+sys.argv[0]+" <test file> [<args>..]")
    sys.exit()
try:
    dirname, testFile = os.path.split(os.path.abspath(sys.argv[1]))
    os.chdir(dirname)
    f = open(testFile)
    testargs = []
except OSError as e:
    print("Error opening "+e.filename+", maybe it doesn't exist")
    sys.exit()

for line in f:
    if(line.split(" ")[0] == "\n" or line[0] == "#"):
        pass
    elif(line.split(" ")[0] == "args" and len(line.split(" ")) > 1):
        testargs = line.replace("\n", "").split(" ")[1:]
        if(len(sys.argv) < len(testargs) + 2):
            print("Error: this test requires arguments: "+", ".join(testargs))
            sys.exit()
    elif(line.split(" ")[0] == "title" and len(line.split(" ")) > 1):
        print("Running test: "+" ".join(line.split(" ")[1:]))
    elif(line.split(" ")[0] == "also" and len(line.split(" ")) > 1):
        print()
        os.system(sys.argv[0]+" "+line.split(" ")[1])
    else:
        for i in range(len(testargs)):
            line = line.replace(testargs[i], sys.argv[i+2])
        tokens = lex(line)
        parseAndRun(tokens, 0)



