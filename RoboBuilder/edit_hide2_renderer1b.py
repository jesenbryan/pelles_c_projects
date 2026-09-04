import shutil, glob, re

def backup(path):
    existing = glob.glob(path + ".bak*")
    nums = [int(re.search(r"\.bak(\d+)$", f).group(1)) for f in existing if re.search(r"\.bak(\d+)$", f)]
    n = max(nums) + 1 if nums else 1
    b = f"{path}.bak{n}"
    shutil.copyfile(path, b)
    print("backup:", b)
    return b

path = "renderer.c"
backup(path)
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

old_mc = """PointF computeRockyMassCenterWorld(Rocky b)
{
    PointF rectCentroidWorld, legCentroidWorld;
    computeRockyMassCenterEndpointsWorld(b, &rectCentroidWorld, &legCentroidWorld);

    // Falls back to an even 50/50 split (not a pure rectangle-only
    // point) whenever both weights are ~0 -- same guard save.c's own
    // massCenter computation uses. Previously this fell back to
    // rectCentroidWorld outright, which put the dot exactly on top of
    // the Body handle instead of the intended default halfway point
    // whenever bodyWeight/legWeight ever read as 0/0 (reported: the dot
    // sitting on the body handle at "startup" even though the boxes
    // showed 0.5/0.5) -- this guarantees the ON-SCREEN default always
    // matches the documented 0.5/0.5 starting point regardless of
    // whatever left the underlying fields at 0.
    float bw = b.bodyWeight;
    float lw = b.legWeight;
    float totalWeight = bw + lw;
    if (totalWeight <= 1e-6f)
    {
        bw = 0.5f;
        lw = 0.5f;
        totalWeight = 1.0f;
    }
"""
assert content.count(old_mc) == 1, f"old_mc match: {content.count(old_mc)}"

new_mc = """PointF computeRockyMassCenterWorld(Rocky b)
{
    PointF rectCentroidWorld, legCentroidWorld;
    computeRockyMassCenterEndpointsWorld(b, &rectCentroidWorld, &legCentroidWorld);

    // Falls back to an even 50/50 split (not a pure rectangle-only
    // point) whenever both weights are ~0 -- same guard save.c's own
    // massCenter computation uses. Previously this fell back to
    // rectCentroidWorld outright, which put the dot exactly on top of
    // the Body handle instead of the intended default halfway point
    // whenever bodyWeight/legWeight ever read as 0/0 (reported: the dot
    // sitting on the body handle at "startup" even though the boxes
    // showed 0.5/0.5) -- this guarantees the ON-SCREEN default always
    // matches the documented 0.5/0.5 starting point regardless of
    // whatever left the underlying fields at 0.
    float bw = b.bodyWeight;
    float lw = b.legWeight;

    // Testing-only toggles (ID_ROCKY_TOGGLE_LEG_BUTTON/
    // ID_ROCKY_TOGGLE_BODY_BUTTON) -- with one part hidden, the mass
    // center has nowhere else to be but entirely on the part that's
    // actually still there, regardless of whatever bodyWeight/legWeight
    // currently say. Deliberately overrides the LOCAL bw/lw used for
    // just this blend rather than the underlying fields themselves, so
    // the user's own weight ratio is preserved underneath and comes
    // right back the moment the hidden part is restored -- nothing here
    // is lost, only temporarily not applied. (Both hidden at once is a
    // degenerate case with nothing left to weight at all -- falls
    // through to the ordinary totalWeight<=1e-6 guard below, same
    // 50/50-of-nothing-meaningful fallback as the plain 0/0 case.)
    if (b.legHidden && !b.bodyHidden)
    {
        bw = 1.0f;
        lw = 0.0f;
    }
    else if (b.bodyHidden && !b.legHidden)
    {
        bw = 0.0f;
        lw = 1.0f;
    }

    float totalWeight = bw + lw;
    if (totalWeight <= 1e-6f)
    {
        bw = 0.5f;
        lw = 0.5f;
        totalWeight = 1.0f;
    }
"""

content = content.replace(old_mc, new_mc)

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("renderer.c (mass center) done")
