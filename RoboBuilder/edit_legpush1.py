import shutil, glob

path = "canvas.c"
existing = glob.glob(path + ".bak*")
nums = [int(p.rsplit("bak", 1)[1]) for p in existing if p.rsplit("bak", 1)[1].isdigit()]
nextnum = max(nums) + 1 if nums else 1
backup = f"{path}.bak{nextnum}"
shutil.copyfile(path, backup)

with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old = '''// Whether Rocky's FOOT specifically (not the rest of the robot) is
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
// the body already resting on the ground.
static BOOL rockyFootCollides(void)
{
    Rocky* r = &app.robotScene.rocky;
    PointF center = getRockyCenter(*r);
    PointF footWorld = jointToWorld(r->footCircle, r->kneeCircle, r->kneeAngle, center, r->angle);

    float ecx, ecy;
    robotPointToEnvWorld(footWorld.x, footWorld.y, &ecx, &ecy);
    float eRadius = robotLengthToEnvWorld(r->footRadius);

    return pointCollidesWithAnyEnvironmentStroke(ecx, ecy, eRadius);
}

// The E/Q handler (canvas.c's WM_KEYDOWN) assumes its OWN starting
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
// "stay put" fallback handle it.
static void rockyEscapeFootPenetration(void)
{
    if (!rockyFootCollides())
        return; // already clear -- nothing to do

    Rocky* r = &app.robotScene.rocky;
    float original = r->kneeAngle;

    // Widened to a full +/-180 (a complete circle either way) after a
    // real [STUCK] log showed the old +/-15 degree window finding no
    // escape at all. The knee's position is fixed relative to the body,
    // so as kneeAngle sweeps through every value the foot traces out a
    // full circle around it -- unless the environment happens to
    // surround that ENTIRE circle (essentially the knee sealed inside
    // solid geometry, not a real "resting on the ground" situation),
    // there has to be SOME angle on that circle where the foot is clear.
    // Searching the whole thing instead of a narrow local window is what
    // actually guarantees an escape exists to find, rather than hoping
    // one happens to fall within an arbitrary small range.
    const float maxEscapeDeg = 180.0f;
    const int escapeSteps = 360; // ~0.5 degree resolution around the circle

    for (int i = 1; i <= escapeSteps; i++)
    {
        float delta = (maxEscapeDeg * (float)i) / (float)escapeSteps;

        r->kneeAngle = original + delta;
        if (!rockyFootCollides())
            return;

        r->kneeAngle = original - delta;
        if (!rockyFootCollides())
            return;
    }

    // No escape ANYWHERE on the full circle -- log exactly where this is
    // happening (which point, how deep) instead of silently giving up, so
    // a report of "it's stuck" can be matched to a concrete kneeAngle/gap
    // instead of guessed at. Foot world position computed the same way
    // rockyFootCollides itself does internally; the printed gap now
    // matches pointCollidesWithAnyEnvironmentStroke's ACTUAL combinedRadius
    // (eRadius + simEnvLineHalfWidthWorld) -- the first version of this
    // print compared against eRadius alone, which under-reported how deep
    // the overlap really was (looked barely-positive/"almost flush" even
    // when the real collision test was firmly TRUE).
    r->kneeAngle = original;
    {
        PointF center = getRockyCenter(*r);
        PointF footWorld = jointToWorld(r->footCircle, r->kneeCircle, r->kneeAngle, center, r->angle);
        float ecx, ecy;
        robotPointToEnvWorld(footWorld.x, footWorld.y, &ecx, &ecy);
        float eRadius = robotLengthToEnvWorld(r->footRadius);
        float combinedRadius = eRadius + simEnvLineHalfWidthWorld();
        float dist = nearestEnvDistance(ecx, ecy);
        printf("[STUCK] rockyEscapeFootPenetration: NO ESCAPE ANYWHERE on the full circle around kneeCircle -- kneeAngle=%.2f (foot env=(%.5f,%.5f) combinedRadius=%.5f nearestDist=%.5f gap=%.5f)\\n",
               original, ecx, ecy, combinedRadius, dist, dist - combinedRadius);
    }
}'''

new = '''// The E/Q handler (canvas.c's WM_KEYDOWN) always applies its full
// requested kneeAngle rotation now -- rather than refusing a rotation
// that would bury the leg, it pushes the whole body up instead (see that
// handler's own comment) -- so it needs its OWN starting kneeAngle to
// already be collision-free before doing that, exactly like
// applyGravityStep's own binary search assumes the robot doesn't already
// overlap before it starts. That assumption can be wrong: a resting pose
// that JUST barely clears robotCollidesWithEnvironment's test one frame
// can land a hair on the wrong side of it the next (the same kind of
// floating-point-close case applyGravityStep itself deals with). When
// that happens, the E/Q handler's own push-up search has no genuinely
// safe position to shrink toward and degenerates to "stuck exactly where
// it is" -- which reads as the foot or shin stuck through the floor with
// E/Q unable to do anything about it.
//
// This is the self-healing step the E/Q handler calls before trusting
// its own starting angle: if the robot already collides, walk the KNEE
// outward in BOTH rotation directions at once, in small increments, and
// snap to the first angle (either sign) where robotCollidesWithEnvironment
// reports clear again. Searches the FULL circle (see maxEscapeDeg below)
// rather than a narrow local window -- a real report showed a narrower
// search finding nothing at all -- so if nothing on the entire circle is
// safe (the knee sealed inside solid geometry, or the torso itself is
// embedded independently of anything kneeAngle could ever fix), it
// leaves the angle untouched and lets the caller's own push-up/fallback
// handle it.
static void rockyEscapeLegPenetration(void)
{
    if (!robotCollidesWithEnvironment())
        return; // already clear -- nothing to do

    Rocky* r = &app.robotScene.rocky;
    float original = r->kneeAngle;

    // Widened to a full +/-180 (a complete circle either way) after a
    // real [STUCK] log showed the old +/-15 degree window finding no
    // escape at all. The knee's position is fixed relative to the body,
    // so as kneeAngle sweeps through every value the foot/shin trace out
    // a full circle around it -- unless the environment happens to
    // surround that ENTIRE circle (essentially the knee sealed inside
    // solid geometry, not a real "resting on the ground" situation),
    // there has to be SOME angle on that circle where the leg is clear.
    // Searching the whole thing instead of a narrow local window is what
    // actually guarantees an escape exists to find, rather than hoping
    // one happens to fall within an arbitrary small range.
    const float maxEscapeDeg = 180.0f;
    const int escapeSteps = 360; // ~0.5 degree resolution around the circle

    for (int i = 1; i <= escapeSteps; i++)
    {
        float delta = (maxEscapeDeg * (float)i) / (float)escapeSteps;

        r->kneeAngle = original + delta;
        if (!robotCollidesWithEnvironment())
            return;

        r->kneeAngle = original - delta;
        if (!robotCollidesWithEnvironment())
            return;
    }

    // No escape ANYWHERE on the full circle -- log where the FOOT (a
    // representative point on the leg -- the part still overlapping
    // could just as easily be a shin arc instead) ends up, instead of
    // silently giving up, so a report of "it's stuck" can be matched to
    // a concrete kneeAngle/gap instead of guessed at. The printed gap
    // matches pointCollidesWithAnyEnvironmentStroke's ACTUAL
    // combinedRadius (eRadius + simEnvLineHalfWidthWorld) -- an earlier
    // version of this print compared against eRadius alone, which
    // under-reported how deep the overlap really was (looked
    // barely-positive/"almost flush" even when the real collision test
    // was firmly TRUE).
    r->kneeAngle = original;
    {
        PointF center = getRockyCenter(*r);
        PointF footWorld = jointToWorld(r->footCircle, r->kneeCircle, r->kneeAngle, center, r->angle);
        float ecx, ecy;
        robotPointToEnvWorld(footWorld.x, footWorld.y, &ecx, &ecy);
        float eRadius = robotLengthToEnvWorld(r->footRadius);
        float combinedRadius = eRadius + simEnvLineHalfWidthWorld();
        float dist = nearestEnvDistance(ecx, ecy);
        printf("[STUCK] rockyEscapeLegPenetration: NO ESCAPE ANYWHERE on the full circle around kneeCircle -- kneeAngle=%.2f (foot env=(%.5f,%.5f) combinedRadius=%.5f nearestDist=%.5f gap=%.5f)\\n",
               original, ecx, ecy, combinedRadius, dist, dist - combinedRadius);
    }
}'''

assert content.count(old) == 1, f"old match count: {content.count(old)}"
content = content.replace(old, new)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)

print("OK, backup:", backup)
