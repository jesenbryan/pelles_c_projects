import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

path = "canvas.c"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = """        // Plain Left/Right (no Shift): rotates the WHOLE robot
        // (Semni.angle), not any one joint -- joints are a Shift+scroll-
        // over-the-hovered-circle action instead (WM_MOUSEWHEEL above),
        // gated behind Shift specifically so a plain scroll still zooms
        // (see that branch's own comment) -- unrelated to this. Same angle
        // field and same per-press step Design > Robot mode's own
        // Left/Right already nudges (input.c's WM_KEYDOWN, `angle +=
        // 2.0f` / `-= 2.0f`), same Left = positive / Right = negative sign
        // convention too, so a press feels identical between the two
        // editors. Windows' own key-repeat re-fires this while held, same
        // as plain G above, so no repeat bookkeeping needed.
        if ((wParam == VK_LEFT || wParam == VK_RIGHT) && appMode == APP_MODE_SIMULATION)
        {
            float step = (wParam == VK_LEFT) ? SIMULATION_WHOLE_BODY_ROTATE_STEP_DEG : -SIMULATION_WHOLE_BODY_ROTATE_STEP_DEG;

            app.robotScene.robot.angle += step;

            // Same re-settle WM_MOUSEWHEEL's joint-rotate branch and slope
            // response both use -- keeps a rotate from visibly sinking the
            // robot into ground it's already resting on.
            resolveUpwardIfPenetrating(hWnd, SIMULATION_SLOPE_CORRECTION_MAX);

            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }"""
assert content.count(old) == 1, f"match: {content.count(old)}"

new = """        // Plain Left/Right (no Shift): rotates the WHOLE robot, not any
        // one joint -- joints are a Shift+scroll-over-the-hovered-circle
        // action instead (WM_MOUSEWHEEL above), gated behind Shift
        // specifically so a plain scroll still zooms (see that branch's
        // own comment) -- unrelated to this. Same angle field and same
        // per-press step Design > Robot mode's own Left/Right already
        // nudges (input.c's WM_KEYDOWN, `angle += 2.0f` / `-= 2.0f`),
        // same Left = positive / Right = negative sign convention too,
        // so a press feels identical between the two editors. Windows'
        // own key-repeat re-fires this while held, same as plain G
        // above, so no repeat bookkeeping needed.
        //
        // Dispatches on activeKind now -- this used to unconditionally
        // write app.robotScene.robot.angle (Semni's own field) no matter
        // which kind was actually active, so a real report showed Left/
        // Right silently doing nothing visible while Rocky or Stilo was
        // selected: it WAS rotating something, just the invisible,
        // inactive Semni sitting behind it, not the robot actually on
        // screen. Every other kind-specific action in this file (E/Q,
        // robotCollidesWithEnvironment, applyGravityStep's settle, etc.)
        // already reads app.robotScene.activeKind for exactly this
        // reason -- this was the one plain-rotate branch that never got
        // the same treatment.
        if ((wParam == VK_LEFT || wParam == VK_RIGHT) && appMode == APP_MODE_SIMULATION)
        {
            float step = (wParam == VK_LEFT) ? SIMULATION_WHOLE_BODY_ROTATE_STEP_DEG : -SIMULATION_WHOLE_BODY_ROTATE_STEP_DEG;

            switch (app.robotScene.activeKind)
            {
                case ROBOT_KIND_ROCKY:
                    app.robotScene.rocky.angle += step;
                    break;
                case ROBOT_KIND_STILO:
                    app.robotScene.stilo.angle += step;
                    break;
                case ROBOT_KIND_SEMNI:
                default:
                    app.robotScene.robot.angle += step;
                    break;
            }

            // Same re-settle WM_MOUSEWHEEL's joint-rotate branch and slope
            // response both use -- keeps a rotate from visibly sinking the
            // robot into ground it's already resting on. Already reads
            // activeKind internally (robotCollidesWithEnvironment's own
            // switch), so no change needed here to act on the right kind.
            resolveUpwardIfPenetrating(hWnd, SIMULATION_SLOPE_CORRECTION_MAX);

            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }"""

content = content.replace(old, new)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("canvas.c (Left/Right rotate fix) done")
