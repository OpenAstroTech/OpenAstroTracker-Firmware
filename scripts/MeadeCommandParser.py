# Helper script for parsingt the header file of MEADE
# Can be used to populate WIKI

import os
import re

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

def remove_line_prefix(line):
    fixed_line = line.replace("// ", "").strip()
    fixed_line = fixed_line.replace("|", "\|")
    fixed_line = re.sub(r"[\n\t]*", "", fixed_line)
    return fixed_line

def format_return_string(line):
    line = line.replace("//            ", " ")
    line = line.replace("//      ", " ")
    word_split = line.split(" ")
    for i in range(len(word_split)):
        word_split[i] = word_split[i].replace("<", "")
        word_split[i] = word_split[i].replace(">", "")
        if '#' in word_split[i]:
            word_split[i] = "`" + word_split[i] + "`"
            
    new_line = ' '.join(word_split)
    print(new_line)
    return new_line


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
        newFamily.name = remove_line_prefix(content[start + 1])
    elif "// --" in content[start]:
        nameCleanup = content[start].replace("// -- ", "")
        nameCleanup = nameCleanup.replace(" --", "")
        newFamily.name = nameCleanup

    for y in range(start + 1, end - 1):
        newCommand = Command()

        # Command
        if content[y].startswith("// :"):
            newCommand.command = remove_line_prefix(content[y])
        
            # Short Description
            newCommand.short = remove_line_prefix(content[y + 1])
            y+=2

            k = y
            while not content[k].startswith("//      Returns:"):
                newCommand.long += remove_line_prefix(content[k])
                k += 1
            y = k
        
        if content[y].startswith("//      Returns:"):
            newCommand.returns += format_return_string(content[y].replace("//      Returns: ", ""))
            k = y+1
            while content[k] != "//":
                return_line = content[k].replace("//               ", "")
                newCommand.returns += format_return_string(return_line)
                k += 1
            y = k

        if newCommand.command:
            newFamily.commands.append(newCommand)

    allCommands.append(newFamily)


def output_wiki():
    f = open("./scripts/MeadeToWikiOutput.txt", "w")
    
    f.write("# MEADE Command Index\n\n")
    for fam in allCommands:
        f.write(f"## {fam.name}\n")

        for cmd in fam.commands:
            f.write(f"### {cmd.short}\n")
            f.write(f"**Command:** `{cmd.command}`\n")
            f.write("\n")
            f.write("**Description:**\n")
            f.write(f"{cmd.long}\n")
            f.write("\n")
            f.write("**Returns:**\n")
            f.write(f"{cmd.returns}\n")
            f.write("<br>\n")

    f.write("\n\n")

    f.close()
    print("FilOutpute written to: ./scripts/MeadeToWikiOutput.txt")

if __name__ == "__main__":
    output_wiki()

"""
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
"""