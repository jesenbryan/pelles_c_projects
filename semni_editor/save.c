#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

#include "save.h"
#include "renderer.h"

int saveCanvasAsBMP(const char* filename, HWND hwnd, AppState* app)
{
    RECT rect;
    GetClientRect(hwnd, &rect);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    if (width <= 0 || height <= 0)
        return 0;

    // BMP rows must be padded out to a multiple of 4 bytes -- telling GL
    // to pack with the same 4-byte alignment means the buffer it fills in
    // is already laid out exactly the way the file needs it
    int rowSize = ((width * 3 + 3) / 4) * 4;
    int dataSize = rowSize * height;

    unsigned char* pixels = (unsigned char*)malloc(dataSize);
    if (!pixels)
        return 0;

    // draw a handle-free version of the scene into the back buffer --
    // nothing here reaches the screen since we never call SwapBuffers,
    // so what's currently displayed (with handles) is untouched
    renderAppForSave(app);
    glFinish();

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    // glReadPixels' rows run bottom-to-top, which is exactly what a BMP
    // with a positive biHeight expects, so no flipping needed -- just
    // swap each pixel's R and B (GL gives RGB, BMP wants BGR)
    for (int y = 0; y < height; y++)
    {
        unsigned char* row = pixels + y * rowSize;

        for (int x = 0; x < width; x++)
        {
            unsigned char* px = row + x * 3;
            unsigned char tmp = px[0];
            px[0] = px[2];
            px[2] = tmp;
        }
    }

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    ZeroMemory(&fileHeader, sizeof(fileHeader));
    ZeroMemory(&infoHeader, sizeof(infoHeader));

    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 24;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = dataSize;

    fileHeader.bfType = 0x4D42; // 'BM'
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + dataSize;

    FILE* f = fopen(filename, "wb");
    if (!f)
    {
        free(pixels);
        return 0;
    }

    fwrite(&fileHeader, sizeof(fileHeader), 1, f);
    fwrite(&infoHeader, sizeof(infoHeader), 1, f);
    fwrite(pixels, dataSize, 1, f);

    fclose(f);
    free(pixels);

    // explicitly the ANSI (narrow) API, regardless of whether the project
    // defines UNICODE -- keeps this a plain char* end to end, matching
    // filename's type and plain printf's %s, instead of the wchar_t/char*
    // mismatch that came from GetCurrentDirectory's TCHAR macro silently
    // picking the wide version
    char path[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, path);
    printf("Saved canvas as 24-bit BMP in: %s\\%s\n", path, filename);

    return 1;
}

int saveRobotAsEquations(const char* filename, AppState* app)
{
    FILE* f = fopen(filename, "w");
    if (!f)
        return 0;

    Semni* robot = &app->robotScene.robot;

    // BODY - Head Circle
    fprintf(f, "HEAD_X=%.6f\n", robot->headX);
    fprintf(f, "HEAD_Y=%.6f\n", robot->y);
    fprintf(f, "HEAD_RADIUS=%.6f\n", robot->headRadius);

    // BODY - Butt Circle
    fprintf(f, "BUTT_X=%.6f\n", robot->buttX);
    fprintf(f, "BUTT_Y=%.6f\n", robot->y);
    fprintf(f, "BUTT_RADIUS=%.6f\n", robot->buttRadius);

    // BODY - Seam Arcs
    fprintf(f, "SEAM_ARC1_ANGLE=%.6f\n", robot->seamArc1Angle);
    fprintf(f, "SEAM_ARC2_ANGLE=%.6f\n", robot->seamArc2Angle);

    // HIP Joint
    fprintf(f, "HIP_X=%.6f\n", robot->innerCircle.x);
    fprintf(f, "HIP_Y=%.6f\n", robot->innerCircle.y);
    fprintf(f, "HIP_RADIUS=%.6f\n", robot->innerRadius);
    fprintf(f, "HIP_ANGLE=%.6f\n", robot->hipAngle);

    // THIGH - Knee Circle
    fprintf(f, "KNEE_X=%.6f\n", robot->kneeCircle.x);
    fprintf(f, "KNEE_Y=%.6f\n", robot->kneeCircle.y);
    fprintf(f, "KNEE_RADIUS=%.6f\n", robot->kneeRadius);
    fprintf(f, "KNEE_ANGLE=%.6f\n", robot->kneeAngle);

    // THIGH - Arcs
    fprintf(f, "THIGH_ARC1_ANGLE=%.6f\n", robot->thighArc1Angle);
    fprintf(f, "THIGH_ARC2_ANGLE=%.6f\n", robot->thighArc2Angle);

    // SHIN - Foot Circle
    fprintf(f, "FOOT_X=%.6f\n", robot->footCircle.x);
    fprintf(f, "FOOT_Y=%.6f\n", robot->footCircle.y);
    fprintf(f, "FOOT_RADIUS=%.6f\n", robot->footRadius);

    // SHIN - Arcs
    fprintf(f, "SHIN_ARC1_ANGLE=%.6f\n", robot->shinArc1Angle);
    fprintf(f, "SHIN_ARC2_ANGLE=%.6f\n", robot->shinArc2Angle);

    // BODY - Whole
    fprintf(f, "BODY_ANGLE=%.6f\n", robot->angle);

    fclose(f);

    char path[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, path);
    printf("Saved robot equations as TXT in: %s\\%s\n", path, filename);

    return 1;
}

int saveRockyAsEquations(const char* filename, AppState* app)
{
    FILE* f = fopen(filename, "w");
    if (!f)
        return 0;

    Rocky* r = &app->robotScene.rocky;

    // BODY - Rectangle
    fprintf(f, "BODY_X=%.6f\n", r->bodyX);
    fprintf(f, "BODY_Y=%.6f\n", r->bodyY);
    fprintf(f, "BODY_HALF_WIDTH=%.6f\n", r->bodyHalfWidth);
    fprintf(f, "BODY_HALF_HEIGHT=%.6f\n", r->bodyHalfHeight);
    fprintf(f, "BODY_ANGLE=%.6f\n", r->angle);

    // LEG - Knee Circle
    fprintf(f, "KNEE_X=%.6f\n", r->kneeCircle.x);
    fprintf(f, "KNEE_Y=%.6f\n", r->kneeCircle.y);
    fprintf(f, "KNEE_RADIUS=%.6f\n", r->kneeRadius);
    fprintf(f, "KNEE_ANGLE=%.6f\n", r->kneeAngle);

    // LEG - Foot Circle
    fprintf(f, "FOOT_X=%.6f\n", r->footCircle.x);
    fprintf(f, "FOOT_Y=%.6f\n", r->footCircle.y);
    fprintf(f, "FOOT_RADIUS=%.6f\n", r->footRadius);

    // LEG - Shin Arcs
    fprintf(f, "SHIN_ARC1_ANGLE=%.6f\n", r->shinArc1Angle);
    fprintf(f, "SHIN_ARC2_ANGLE=%.6f\n", r->shinArc2Angle);

    fclose(f);

    char path[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, path);
    printf("Saved Rocky equations as TXT in: %s\\%s\n", path, filename);

    return 1;
}

int saveStiloAsEquations(const char* filename, AppState* app)
{
    FILE* f = fopen(filename, "w");
    if (!f)
        return 0;

    Stilo* s = &app->robotScene.stilo;

    // BODY - Head Circle
    fprintf(f, "HEAD_X=%.6f\n", s->headX);
    fprintf(f, "HEAD_Y=%.6f\n", s->y);
    fprintf(f, "HEAD_RADIUS=%.6f\n", s->headRadius);

    // BODY - Butt Circle
    fprintf(f, "BUTT_X=%.6f\n", s->buttX);
    fprintf(f, "BUTT_Y=%.6f\n", s->y);
    fprintf(f, "BUTT_RADIUS=%.6f\n", s->buttRadius);

    // BODY - Seam Arcs
    fprintf(f, "SEAM_ARC1_ANGLE=%.6f\n", s->seamArc1Angle);
    fprintf(f, "SEAM_ARC2_ANGLE=%.6f\n", s->seamArc2Angle);

    // HIP Joint
    fprintf(f, "HIP_X=%.6f\n", s->innerCircle.x);
    fprintf(f, "HIP_Y=%.6f\n", s->innerCircle.y);
    fprintf(f, "HIP_RADIUS=%.6f\n", s->innerRadius);
    fprintf(f, "HIP_ANGLE=%.6f\n", s->hipAngle);

    // THIGH - Knee Circle (Stilo now has the same hip->knee->foot chain
    // as Semni -- see app.h's Stilo comment)
    fprintf(f, "KNEE_X=%.6f\n", s->kneeCircle.x);
    fprintf(f, "KNEE_Y=%.6f\n", s->kneeCircle.y);
    fprintf(f, "KNEE_RADIUS=%.6f\n", s->kneeRadius);
    fprintf(f, "KNEE_ANGLE=%.6f\n", s->kneeAngle);

    // THIGH - Arcs (hip-to-knee)
    fprintf(f, "THIGH_ARC1_ANGLE=%.6f\n", s->thighArc1Angle);
    fprintf(f, "THIGH_ARC2_ANGLE=%.6f\n", s->thighArc2Angle);

    // SHIN - Foot Circle
    fprintf(f, "FOOT_X=%.6f\n", s->footCircle.x);
    fprintf(f, "FOOT_Y=%.6f\n", s->footCircle.y);
    fprintf(f, "FOOT_RADIUS=%.6f\n", s->footRadius);

    // SHIN - Arcs (knee-to-foot)
    fprintf(f, "SHIN_ARC1_ANGLE=%.6f\n", s->shinArc1Angle);
    fprintf(f, "SHIN_ARC2_ANGLE=%.6f\n", s->shinArc2Angle);

    // BODY - Whole
    fprintf(f, "BODY_ANGLE=%.6f\n", s->angle);

    // ---- leg 2 -- same fields as leg 1 above, LEG2_-prefixed keys so they
    // can't collide with leg 1's (see app.h's Stilo comment). No LEG2_
    // equivalent of the torso keys (HEAD_*/BUTT_*/SEAM_ARC*/BODY_ANGLE) --
    // both legs share the one torso, already written above.
    fprintf(f, "LEG2_HIP_X=%.6f\n", s->innerCircleLeg2.x);
    fprintf(f, "LEG2_HIP_Y=%.6f\n", s->innerCircleLeg2.y);
    fprintf(f, "LEG2_HIP_RADIUS=%.6f\n", s->innerRadiusLeg2);
    fprintf(f, "LEG2_HIP_ANGLE=%.6f\n", s->hipAngleLeg2);

    fprintf(f, "LEG2_KNEE_X=%.6f\n", s->kneeCircleLeg2.x);
    fprintf(f, "LEG2_KNEE_Y=%.6f\n", s->kneeCircleLeg2.y);
    fprintf(f, "LEG2_KNEE_RADIUS=%.6f\n", s->kneeRadiusLeg2);
    fprintf(f, "LEG2_KNEE_ANGLE=%.6f\n", s->kneeAngleLeg2);

    fprintf(f, "LEG2_THIGH_ARC1_ANGLE=%.6f\n", s->thighArc1AngleLeg2);
    fprintf(f, "LEG2_THIGH_ARC2_ANGLE=%.6f\n", s->thighArc2AngleLeg2);

    fprintf(f, "LEG2_FOOT_X=%.6f\n", s->footCircleLeg2.x);
    fprintf(f, "LEG2_FOOT_Y=%.6f\n", s->footCircleLeg2.y);
    fprintf(f, "LEG2_FOOT_RADIUS=%.6f\n", s->footRadiusLeg2);

    fprintf(f, "LEG2_SHIN_ARC1_ANGLE=%.6f\n", s->shinArc1AngleLeg2);
    fprintf(f, "LEG2_SHIN_ARC2_ANGLE=%.6f\n", s->shinArc2AngleLeg2);

    fclose(f);

    char path[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, path);
    printf("Saved Stilo equations as TXT in: %s\\%s\n", path, filename);

    return 1;
}

// ---- pose file loaders (read back what save*AsEquations wrote) ----

int loadRobotPoseFromFile(const char* filename, Semni* out)
{
    FILE* f = fopen(filename, "r");
    if (!f)
        return 0;

    char line[128];
    char key[64];
    float value;

    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "%63[^=]=%f", key, &value) != 2)
            continue;

        if      (strcmp(key, "HEAD_X") == 0) out->headX = value;
        else if (strcmp(key, "HEAD_Y") == 0) out->y = value;
        else if (strcmp(key, "HEAD_RADIUS") == 0) out->headRadius = value;
        else if (strcmp(key, "BUTT_X") == 0) out->buttX = value;
        else if (strcmp(key, "BUTT_Y") == 0) out->y = value; // same field as HEAD_Y -- both restate Semni's one shared body Y
        else if (strcmp(key, "BUTT_RADIUS") == 0) out->buttRadius = value;
        else if (strcmp(key, "SEAM_ARC1_ANGLE") == 0) out->seamArc1Angle = value;
        else if (strcmp(key, "SEAM_ARC2_ANGLE") == 0) out->seamArc2Angle = value;
        else if (strcmp(key, "HIP_X") == 0) out->innerCircle.x = value;
        else if (strcmp(key, "HIP_Y") == 0) out->innerCircle.y = value;
        else if (strcmp(key, "HIP_RADIUS") == 0) out->innerRadius = value;
        else if (strcmp(key, "HIP_ANGLE") == 0) out->hipAngle = value;
        else if (strcmp(key, "KNEE_X") == 0) out->kneeCircle.x = value;
        else if (strcmp(key, "KNEE_Y") == 0) out->kneeCircle.y = value;
        else if (strcmp(key, "KNEE_RADIUS") == 0) out->kneeRadius = value;
        else if (strcmp(key, "KNEE_ANGLE") == 0) out->kneeAngle = value;
        else if (strcmp(key, "THIGH_ARC1_ANGLE") == 0) out->thighArc1Angle = value;
        else if (strcmp(key, "THIGH_ARC2_ANGLE") == 0) out->thighArc2Angle = value;
        else if (strcmp(key, "FOOT_X") == 0) out->footCircle.x = value;
        else if (strcmp(key, "FOOT_Y") == 0) out->footCircle.y = value;
        else if (strcmp(key, "FOOT_RADIUS") == 0) out->footRadius = value;
        // ANKLE_* is the old key name (from before the ankle->foot rename) --
        // accepted here too so pose files already saved to disk under the
        // old name still load correctly. Only FOOT_* is ever written now.
        else if (strcmp(key, "ANKLE_X") == 0) out->footCircle.x = value;
        else if (strcmp(key, "ANKLE_Y") == 0) out->footCircle.y = value;
        else if (strcmp(key, "ANKLE_RADIUS") == 0) out->footRadius = value;
        else if (strcmp(key, "SHIN_ARC1_ANGLE") == 0) out->shinArc1Angle = value;
        else if (strcmp(key, "SHIN_ARC2_ANGLE") == 0) out->shinArc2Angle = value;
        else if (strcmp(key, "BODY_ANGLE") == 0) out->angle = value;
    }

    fclose(f);
    return 1;
}

int loadRockyPoseFromFile(const char* filename, Rocky* out)
{
    FILE* f = fopen(filename, "r");
    if (!f)
        return 0;

    char line[128];
    char key[64];
    float value;

    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "%63[^=]=%f", key, &value) != 2)
            continue;

        if      (strcmp(key, "BODY_X") == 0) out->bodyX = value;
        else if (strcmp(key, "BODY_Y") == 0) out->bodyY = value;
        else if (strcmp(key, "BODY_HALF_WIDTH") == 0) out->bodyHalfWidth = value;
        else if (strcmp(key, "BODY_HALF_HEIGHT") == 0) out->bodyHalfHeight = value;
        else if (strcmp(key, "BODY_ANGLE") == 0) out->angle = value;
        else if (strcmp(key, "KNEE_X") == 0) out->kneeCircle.x = value;
        else if (strcmp(key, "KNEE_Y") == 0) out->kneeCircle.y = value;
        else if (strcmp(key, "KNEE_RADIUS") == 0) out->kneeRadius = value;
        else if (strcmp(key, "KNEE_ANGLE") == 0) out->kneeAngle = value;
        else if (strcmp(key, "FOOT_X") == 0) out->footCircle.x = value;
        else if (strcmp(key, "FOOT_Y") == 0) out->footCircle.y = value;
        else if (strcmp(key, "FOOT_RADIUS") == 0) out->footRadius = value;
        // legacy key name, see loadRobotPoseFromFile's comment above
        else if (strcmp(key, "ANKLE_X") == 0) out->footCircle.x = value;
        else if (strcmp(key, "ANKLE_Y") == 0) out->footCircle.y = value;
        else if (strcmp(key, "ANKLE_RADIUS") == 0) out->footRadius = value;
        else if (strcmp(key, "SHIN_ARC1_ANGLE") == 0) out->shinArc1Angle = value;
        else if (strcmp(key, "SHIN_ARC2_ANGLE") == 0) out->shinArc2Angle = value;
    }

    fclose(f);
    return 1;
}

int loadStiloPoseFromFile(const char* filename, Stilo* out)
{
    FILE* f = fopen(filename, "r");
    if (!f)
        return 0;

    char line[128];
    char key[64];
    float value;

    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "%63[^=]=%f", key, &value) != 2)
            continue;

        if      (strcmp(key, "HEAD_X") == 0) out->headX = value;
        else if (strcmp(key, "HEAD_Y") == 0) out->y = value;
        else if (strcmp(key, "HEAD_RADIUS") == 0) out->headRadius = value;
        else if (strcmp(key, "BUTT_X") == 0) out->buttX = value;
        else if (strcmp(key, "BUTT_Y") == 0) out->y = value; // same field as HEAD_Y -- both restate Stilo's one shared body Y
        else if (strcmp(key, "BUTT_RADIUS") == 0) out->buttRadius = value;
        else if (strcmp(key, "SEAM_ARC1_ANGLE") == 0) out->seamArc1Angle = value;
        else if (strcmp(key, "SEAM_ARC2_ANGLE") == 0) out->seamArc2Angle = value;
        else if (strcmp(key, "HIP_X") == 0) out->innerCircle.x = value;
        else if (strcmp(key, "HIP_Y") == 0) out->innerCircle.y = value;
        else if (strcmp(key, "HIP_RADIUS") == 0) out->innerRadius = value;
        else if (strcmp(key, "HIP_ANGLE") == 0) out->hipAngle = value;
        else if (strcmp(key, "KNEE_X") == 0) out->kneeCircle.x = value;
        else if (strcmp(key, "KNEE_Y") == 0) out->kneeCircle.y = value;
        else if (strcmp(key, "KNEE_RADIUS") == 0) out->kneeRadius = value;
        else if (strcmp(key, "KNEE_ANGLE") == 0) out->kneeAngle = value;
        else if (strcmp(key, "THIGH_ARC1_ANGLE") == 0) out->thighArc1Angle = value;
        else if (strcmp(key, "THIGH_ARC2_ANGLE") == 0) out->thighArc2Angle = value;
        else if (strcmp(key, "FOOT_X") == 0) out->footCircle.x = value;
        else if (strcmp(key, "FOOT_Y") == 0) out->footCircle.y = value;
        else if (strcmp(key, "FOOT_RADIUS") == 0) out->footRadius = value;
        // legacy key name, see loadRobotPoseFromFile's comment above
        else if (strcmp(key, "ANKLE_X") == 0) out->footCircle.x = value;
        else if (strcmp(key, "ANKLE_Y") == 0) out->footCircle.y = value;
        else if (strcmp(key, "ANKLE_RADIUS") == 0) out->footRadius = value;
        else if (strcmp(key, "SHIN_ARC1_ANGLE") == 0) out->shinArc1Angle = value;
        else if (strcmp(key, "SHIN_ARC2_ANGLE") == 0) out->shinArc2Angle = value;
        // LEG_ARC1_ANGLE/LEG_ARC2_ANGLE (the old key names, from before
        // Stilo had a knee stage) are deliberately NOT accepted here, even
        // though the ANKLE_*/FOOT_* legacy aliasing above is -- unlike a
        // pure rename, those old values described a hip-to-FOOT arc, which
        // means something different from the new hip-to-KNEE thighArc1/2
        // they'd otherwise map onto, so silently reinterpreting them would
        // produce a wrong-looking pose instead of the same one under a new
        // name. Any pose file saved under the old Stilo (no knee) needs to
        // be re-saved from scratch under the new structure.
        else if (strcmp(key, "BODY_ANGLE") == 0) out->angle = value;
        // ---- leg 2 -- same LEG2_-prefixed keys saveStiloAsEquations
        // writes above. An older pose file saved before leg 2 existed
        // simply has none of these keys, so out->*Leg2 is left whatever it
        // already was (zero on a fresh launch) -- same "seed the hardcoded
        // default first, then load on top" fix input.c's
        // ID_STANDING_POSITION_BUTTON/ID_HOME_POSITION_BUTTON already apply
        // for exactly this reason.
        else if (strcmp(key, "LEG2_HIP_X") == 0) out->innerCircleLeg2.x = value;
        else if (strcmp(key, "LEG2_HIP_Y") == 0) out->innerCircleLeg2.y = value;
        else if (strcmp(key, "LEG2_HIP_RADIUS") == 0) out->innerRadiusLeg2 = value;
        else if (strcmp(key, "LEG2_HIP_ANGLE") == 0) out->hipAngleLeg2 = value;
        else if (strcmp(key, "LEG2_KNEE_X") == 0) out->kneeCircleLeg2.x = value;
        else if (strcmp(key, "LEG2_KNEE_Y") == 0) out->kneeCircleLeg2.y = value;
        else if (strcmp(key, "LEG2_KNEE_RADIUS") == 0) out->kneeRadiusLeg2 = value;
        else if (strcmp(key, "LEG2_KNEE_ANGLE") == 0) out->kneeAngleLeg2 = value;
        else if (strcmp(key, "LEG2_THIGH_ARC1_ANGLE") == 0) out->thighArc1AngleLeg2 = value;
        else if (strcmp(key, "LEG2_THIGH_ARC2_ANGLE") == 0) out->thighArc2AngleLeg2 = value;
        else if (strcmp(key, "LEG2_FOOT_X") == 0) out->footCircleLeg2.x = value;
        else if (strcmp(key, "LEG2_FOOT_Y") == 0) out->footCircleLeg2.y = value;
        else if (strcmp(key, "LEG2_FOOT_RADIUS") == 0) out->footRadiusLeg2 = value;
        else if (strcmp(key, "LEG2_SHIN_ARC1_ANGLE") == 0) out->shinArc1AngleLeg2 = value;
        else if (strcmp(key, "LEG2_SHIN_ARC2_ANGLE") == 0) out->shinArc2AngleLeg2 = value;
    }

    fclose(f);
    return 1;
}
