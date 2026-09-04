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

# 1. Create the two buttons right after hDebugLogButton's creation block.
old_create = """             app->ui.hDebugLogButton = CreateWindow(
                L"BUTTON",
                L"Debug Log",
                WS_VISIBLE | WS_CHILD,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_DEBUG_LOG_BUTTON,
                NULL,
                NULL
            );
             SendMessage(app->ui.hDebugLogButton, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);
"""
assert content.count(old_create) == 1, f"create match: {content.count(old_create)}"

new_create = old_create + """
             // Rocky-only testing toggles (see app.h's
             // ID_ROCKY_TOGGLE_LEG_BUTTON/ID_ROCKY_TOGGLE_BODY_BUTTON and
             // Rocky's own legHidden/bodyHidden comment) -- same checkbox-
             // as-button convention as hViewSegmentsButton above, so each
             // button IS its own toggle state (read via BM_GETCHECK in
             // WM_COMMAND below). Created for all kinds like the rest of
             // this panel, but only actually SHOWN while Rocky is active
             // -- see ID_ROBOT_SELECTOR's CBN_SELCHANGE handling, same
             // "hide what has nothing to act on" treatment as
             // hMirrorButton2.
             app->ui.hRockyToggleLegButton = CreateWindow(
                L"BUTTON",
                L"Remove Leg",
                WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_ROCKY_TOGGLE_LEG_BUTTON,
                NULL,
                NULL
            );
             SendMessage(app->ui.hRockyToggleLegButton, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);

             app->ui.hRockyToggleBodyButton = CreateWindow(
                L"BUTTON",
                L"Remove Body",
                WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                0, 0, 10, 10,
                hwnd,
                (HMENU)ID_ROCKY_TOGGLE_BODY_BUTTON,
                NULL,
                NULL
            );
             SendMessage(app->ui.hRockyToggleBodyButton, WM_SETFONT, (WPARAM)g_semniUIFont, TRUE);
"""

content = content.replace(old_create, new_create)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("input.c step1 done")
