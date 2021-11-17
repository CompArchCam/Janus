def insert(filestr, placeholder, insertstr):
    return filestr.replace(placeholder, insertstr)

f = open("janus-aarch64.h", "r")

flstr = f.read()

for i in range(31):
    flstr = insert(flstr, "...", "JREG_X"+str(i)+", /**< The \"X"+str(i)+"\" register */\n    ...")

for i in range(31):
    flstr = insert(flstr, "...", "JREG_W"+str(i)+", /**< The \"W"+str(i)+"\" register */\n    ...")

for i in range(32):
    flstr = insert(flstr, "...", "JREG_Q"+str(i)+", /**< The \"Q"+str(i)+"\" register */\n    ...")

for i in range(32):
    flstr = insert(flstr, "...", "JREG_D"+str(i)+", /**< The \"D"+str(i)+"\" register */\n    ...")

for i in range(32):
    flstr = insert(flstr, "...", "JREG_S"+str(i)+", /**< The \"S"+str(i)+"\" register */\n    ...")

for i in range(32):
    flstr = insert(flstr, "...", "JREG_H"+str(i)+", /**< The \"H"+str(i)+"\" register */\n    ...")

for i in range(32):
    flstr = insert(flstr, "...", "JREG_B"+str(i)+", /**< The \"B"+str(i)+"\" register */\n    ...")

for i in range(32):
    flstr = insert(flstr, "-----", "JREG_V"+str(i)+", /**< The \"V"+str(i)+"\" register */\n    -----")

print(flstr)

f = open("janus-aarch64.h", "w")

f.write(flstr)
