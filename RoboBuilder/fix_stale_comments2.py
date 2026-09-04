import shutil, glob

path = "canvas.c"

# find next backup number
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old1 = '''// Whether Rocky's FOOT specifically (not the rest of the robot) is
// currently touching the environment -- exactly the same per-part test
// robotCollidesWithEnvironment's Rocky branch runs for bodyCircles[1] (see
// computeRockyBodyCircles' own comment on that indexing: 0 = knee, 1 =
// foot), just isolated to only that one part instead of the whole robot.
// Needed because robotCollidesWithEnvironment answers "is ANY part of the
// robot touching," which is TRUE the instant the body/rect lands (the
// normal, common case) even while the foot itself is still dangling in
// midair off a ledge -- exactly the situation the knee pendulum inside
// advanceAutoGravity below exists to react to, so it needs its own,
// narrower answer.'''

new1 = '''// Whether Rocky's FOOT specifically (not the rest of the robot) is
// currently touching the environment -- exactly the same per-part test
// robotCollidesWithEnvironment's Rocky branch runs for bodyCircles[1] (see
// computeRockyBodyCircles' own comment on that indexing: 0 = knee, 1 =
// foot), just isolated to only that one part instead of the whole robot.
// Needed because robotCollidesWithEnvironment answers "is ANY part of the
// robot touching," which is TRUE the instant the body/rect lands (the
// normal, common case) even while the foot itself is still dangling in
// midair off a ledge -- exactly the situation the E/Q handler (canvas.c's
// WM_KEYDOWN) needs its own, narrower answer for, since rotating the knee
// should only be blocked by the FOOT actually touching something, not by
// the body already resting on the ground.'''

assert content.count(old1) == 1, f"old1 match count: {content.count(old1)}"
content = content.replace(old1, new1)

old2 = '''// Both the knee pendulum below and the E/Q handler (canvas.c's WM_KEYDOWN)
// assume their OWN starting kneeAngle is already collision-free, and only
// ever search the narrow range between that starting angle and one
// proposed new angle -- exactly like applyGravityStep's own binary search
// assumes the robot doesn't already overlap before it starts. That
// assumption can be wrong here even though it (almost) never is for
// applyGravityStep: the whole-body landing test (robotCollidesWithEnvironment)
// and this file's foot-only test (rockyFootCollides) are two separate
// evaluations of "does the environment touch this circle," computed via
// slightly different paths (whole-robot part list vs. a single
// jointToWorld + robotPointToEnvWorld lookup) -- so a resting pose that
// JUST barely clears one test can still land on the wrong side of the
// other by a hair. When that happens, both consumers below degenerate to
// "the safe search never leaves position zero" and just freeze exactly
// where they are -- which, if that position happens to be a hair inside
// the ground, reads as the foot stuck through the floor with E/Q or the
// pendulum unable to do anything about it.
//
// This is the self-healing step both call before trusting their own
// starting angle: if the foot already collides, walk outward in BOTH
// rotation directions at once, in small increments, and snap to the
// first angle (either sign) where it clears again. Searches the FULL
// circle (see maxEscapeDeg below) rather than a narrow local window -- a
// real report showed a narrower search finding nothing at all -- so if
// nothing on the entire circle is safe (the knee sealed inside solid
// geometry), it leaves the angle untouched and lets the caller's own
// "stay put" fallback handle it.'''

new2 = '''// The E/Q handler (canvas.c's WM_KEYDOWN) assumes its OWN starting
// kneeAngle is already collision-free, and only ever searches the narrow
// range between that starting angle and one proposed new angle --
// exactly like applyGravityStep's own binary search assumes the robot
// doesn't already overlap before it starts. That assumption can be wrong
// here even though it (almost) never is for applyGravityStep: the
// whole-body landing test (robotCollidesWithEnvironment) and this file's
// foot-only test (rockyFootCollides) are two separate evaluations of
// "does the environment touch this circle," computed via slightly
// different paths (whole-robot part list vs. a single jointToWorld +
// robotPointToEnvWorld lookup) -- so a resting pose that JUST barely
// clears one test can still land on the wrong side of the other by a
// hair. When that happens, the E/Q handler's own binary search degenerates
// to "the safe search never leaves position zero" and just freezes
// exactly where it is -- which, if that position happens to be a hair
// inside the ground, reads as the foot stuck through the floor with E/Q
// unable to do anything about it.
//
// This is the self-healing step the E/Q handler calls before trusting its
// own starting angle: if the foot already collides, walk outward in BOTH
// rotation directions at once, in small increments, and snap to the
// first angle (either sign) where it clears again. Searches the FULL
// circle (see maxEscapeDeg below) rather than a narrow local window -- a
// real report showed a narrower search finding nothing at all -- so if
// nothing on the entire circle is safe (the knee sealed inside solid
// geometry), it leaves the angle untouched and lets the caller's own
// "stay put" fallback handle it.'''

assert content.count(old2) == 1, f"old2 match count: {content.count(old2)}"
content = content.replace(old2, new2)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)
