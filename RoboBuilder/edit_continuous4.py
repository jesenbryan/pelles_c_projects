import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''// resting height in a single shot regardless of how big the actual gap
// turns out to be. Used by settleRockyKneeUnderBody below to answer "how
// far would the body fall if the knee were bent differently," which
// needs a real answer for a potentially large gap, not one tick's worth.'''

new = '''// resting height in a single shot regardless of how big the actual gap
// turns out to be. Used by advanceRockyKneeSettle below to answer "how
// far would the body fall if the knee were bent differently," which
// needs a real answer for a potentially large gap, not one tick's worth.'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)
