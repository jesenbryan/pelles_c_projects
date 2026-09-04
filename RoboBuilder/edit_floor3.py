import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''        // !suppressThisCall here means a FRESH landing (wasLanded was
        // FALSE going into this call) -- reset the convergence flag right
        // at that moment so a robot that's lifted off (E/Q, a drag, or
        // just walking off a ledge) and lands again later still gets its
        // own full settle pass, instead of staying permanently "already
        // converged" from whatever the PREVIOUS landing settled into.
        if (!suppressThisCall)
            rockySettleConverged = FALSE;

        advanceRockySettle();'''

new = '''        // !suppressThisCall here means a FRESH landing (wasLanded was
        // FALSE going into this call) -- reset the convergence flag AND
        // both probes' step sizes right at that moment so a robot that's
        // lifted off (E/Q, a drag, or just walking off a ledge) and lands
        // again later still gets its own full settle pass at full
        // resolution, instead of staying permanently "already converged"
        // (or stuck at whatever fine step size it had refined down to)
        // from whatever the PREVIOUS landing settled into.
        if (!suppressThisCall)
        {
            rockySettleConverged = FALSE;
            rockyKneeSettleStep = SIMULATION_LEG_SETTLE_STEP_DEG;
            rockyBodySettleStep = SIMULATION_BODY_SETTLE_STEP_DEG;
        }

        advanceRockySettle();'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)
