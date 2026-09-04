import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

path = "input.c"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old1 = """             app->ui.hRockyToggleLegButton = CreateWindow(
                L"BUTTON",
                L"Remove Leg",
                WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,"""
assert content.count(old1) == 1, f"old1 match: {content.count(old1)}"
new1 = """             app->ui.hRockyToggleLegButton = CreateWindow(
                L"BUTTON",
                L"Remove Leg",
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,"""
content = content.replace(old1, new1)

old2 = """             app->ui.hRockyToggleBodyButton = CreateWindow(
                L"BUTTON",
                L"Remove Body",
                WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,"""
assert content.count(old2) == 1, f"old2 match: {content.count(old2)}"
new2 = """             app->ui.hRockyToggleBodyButton = CreateWindow(
                L"BUTTON",
                L"Remove Body",
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,"""
content = content.replace(old2, new2)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("done -- both now WS_VISIBLE (matches hMirrorButton2's own convention: default activeKind is ROCKY, per app_init.c, so visible-by-default + CBN_SELCHANGE hide-for-other-kinds is the correct startup state)")
