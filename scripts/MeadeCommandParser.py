# Helper script for parsingt the header file of MEADE
# Can be used to populate WIKI

import os

MEADE_HPP = "..\\src\\MeadeCommandProcessor.cpp"
MODULE_PATH = os.path.dirname(os.path.realpath(__file__))
START_LINE = 0
END_LINE = 0

class Family:
    def __init__(self):
        self.name = None
        self.commands = []


class Command:
    def __init__(self):
        self.command = None
        self.short = str()
        self.long = str()
        self.returns = str()

def removeLinePrefix(line):
    return line.replace("// ", "").strip()

#Meade hpp File
with open(os.path.join(MODULE_PATH, MEADE_HPP)) as f:
    content = f.readlines()
content = [x.strip() for x in content]

# Find start/end block
div = "/////////////////////////////////////////////////////////////////////////////////////////"
startStop = []
for count, line in enumerate(content):
    if div in line:
        startStop.append(count)

if len(startStop) != 2:
    raise Exception("Could not locate start and stop of comment block.")

START_LINE = startStop[0]
END_LINE  = startStop[1]

print(("Start and end of block: {0}, {1} ".format(START_LINE, END_LINE)))

familyDividers = []
for i in range(START_LINE, END_LINE):
    for div in ["//------------------------------------------------------------------", "// --"]:
        if div in content[i]:
            familyDividers.append(i)

print(("Found {0} family groups ".format(len(familyDividers))))

allCommands = []
for i in range(len(familyDividers) - 1):
    start = familyDividers[i]
    end = familyDividers[i + 1]
    
    newFamily = Family()
    if "//------------------------------------------------------------------" in content[start]:
        newFamily.name = removeLinePrefix(content[start + 1])
    elif "// --" in content[start]:
        nameCleanup = content[start].replace("// -- ", "")
        nameCleanup = nameCleanup.replace(" --", "")
        newFamily.name = nameCleanup

    for y in range(start + 1, end - 1):
        newCommand = Command()

        # Command
        if content[y].startswith("// :"):
            newCommand.command = removeLinePrefix(content[y])
        
            # Short Description
            newCommand.short = removeLinePrefix(content[y + 1])
            y+=2

            k = y
            while not content[k].startswith("//      Returns:"):
                newCommand.long += removeLinePrefix(content[k])
                k += 1
            y = k
        
        
        if content[y].startswith("//      Returns:"):
            newCommand.returns += content[y].replace("//      Returns: ", "")
            k = y+1
            while content[k] != "//":
                newCommand.returns += content[k].replace("//               ", " ").strip()
                k += 1
            y = k
        
        if newCommand.command:
            newFamily.commands.append(newCommand)

    allCommands.append(newFamily)


# Example of printing output
for fam in allCommands:
    print("***** {0} *****".format(fam.name))
    print("Command Count: {0}".format(len(fam.commands)))
    for cmd in fam.commands:
        print("\tCommand: {0}".format(cmd.command))
        print("\tShort: {0}".format(cmd.short))
        print("\tLong Description: {0}".format(cmd.long))
        print("\tReturns: {0}".format(cmd.returns))
        print("\r")

print("Family Count: {0}".format(len(allCommands)))
for fam in allCommands:
    print("{0}".format(fam.name))
    print("\t{0}".format(len(fam.commands)))