#define UNICODE
#define _UNICODE

#include <windows.h>
#include <GL/gl.h>
#include <math.h>
#include <commctrl.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")

// ---------------- DATA ----------------

typedef struct {
    float x, y;
} Point;

typedef struct {
    float leftX, rightX;
    float y;

    float leftRadius;
    float rightRadius;

    Point topCtrl;
    Point bottomCtrl;

    Point innerCircle;     
    float innerRadius;     

    float angle;
} CapsuleBody;

CapsuleBody robot;

HDC hdc;
HGLRC hrc;

float MIN_R = 0.05f;
float MAX_R = 0.35f;

HWND hwndSlider;

HWND labelLeft;
HWND sliderLeft;

HWND labelRight;
HWND sliderRight;

int draggingTop = 0;
int draggingBottom = 0;

int activeHandle = 0; // 0 = none, 1 = top, 2 = bottom

int sliderDraggingLeft = 0;
int sliderDraggingRight = 0;

int draggingInner = 0;

#define ID_LEFT  101
#define ID_RIGHT 102

#define HANDLE_RADIUS 0.06f

#define INNER_HANDLE_RADIUS 0.03f

// ---------------- DRAW ----------------

void drawCircle(Point c, float r)
{
    glBegin(GL_LINE_LOOP);
    for (float t = 0; t < 6.28318f; t += 0.05f)
    {
        glVertex2f(
            c.x + cosf(t) * r,
            c.y + sinf(t) * r
        );
    }
    glEnd();
}

void drawBezier(Point p0, Point p1, Point p2)
{
    glBegin(GL_LINE_STRIP);

    for (float t = 0; t <= 1.0f; t += 0.02f)
    {
        float u = 1 - t;

        float x =
            u*u*p0.x +
            2*u*t*p1.x +
            t*t*p2.x;

        float y =
            u*u*p0.y +
            2*u*t*p1.y +
            t*t*p2.y;

        glVertex2f(x, y);
    }

    glEnd();
}

Point rotatePoint(Point p, Point center, float angleDeg)
{
    float a = angleDeg * 3.1415926f / 180.0f;

    float s = sinf(a);
    float c = cosf(a);

    // translate to origin
    p.x -= center.x;
    p.y -= center.y;

    // rotate
    float xnew = p.x * c - p.y * s;
    float ynew = p.x * s + p.y * c;

    // translate back
    p.x = xnew + center.x;
    p.y = ynew + center.y;

    return p;
}

Point inverseRotate(Point p, Point center, float angleDeg)
{
    float a = -angleDeg * 3.1415926f / 180.0f;

    float s = sinf(a);
    float c = cosf(a);

    p.x -= center.x;
    p.y -= center.y;

    float xnew = p.x * c - p.y * s;
    float ynew = p.x * s + p.y * c;

    p.x = xnew + center.x;
    p.y = ynew + center.y;

    return p;
}

Point getCenter(CapsuleBody b)
{
    Point c;
    c.x = (b.leftX + b.rightX) * 0.5f;
    c.y = b.y;
    return c;
}

Point circleEdge(Point center, float radius, float angleDeg)
{
    float a = angleDeg * 3.1415926f / 180.0f;

    Point p;
    p.x = center.x + cosf(a) * radius;
    p.y = center.y + sinf(a) * radius;

    return p;
}

void drawHandle(Point p, int selected)
{
    if (selected)
        glColor3f(1.0f, 0.85f, 0.35f); // soft amber yellow highlight
    else
        glColor3f(1.0f, 0.0f, 0.0f); // normal red

    glBegin(GL_TRIANGLE_FAN);

    glVertex2f(p.x, p.y);

    for (float t = 0; t <= 6.28318f; t += 0.1f)
    {
        glVertex2f(
            p.x + cosf(t) * HANDLE_RADIUS,
            p.y + sinf(t) * HANDLE_RADIUS
        );
    }

    glEnd();
}

void drawInnerHandle(Point p, int selected)
{
    if (selected)
        glColor3f(1.0f, 0.85f, 0.35f); // highlight
    else
        glColor3f(1.0f, 0.0f, 0.0f); // solid red

    glBegin(GL_TRIANGLE_FAN);

    glVertex2f(p.x, p.y);

    for (float t = 0; t <= 6.28318f; t += 0.1f)
    {
        glVertex2f(
            p.x + cosf(t) * INNER_HANDLE_RADIUS,
            p.y + sinf(t) * INNER_HANDLE_RADIUS
        );
    }

    glEnd();
}

void drawCapsuleBody(CapsuleBody b, int activeHandle)
{
    Point center = getCenter(b);

    float angle = b.angle;

	int topActive = draggingTop;
	int bottomActive = draggingBottom;

    // rotated circle centers
    Point leftCenter  = rotatePoint((Point){b.leftX,  b.y}, center, angle);
    Point rightCenter = rotatePoint((Point){b.rightX, b.y}, center, angle);

	// inner circle
	Point inner = rotatePoint(b.innerCircle, center, angle);

    // circles
	if (sliderDraggingLeft)
	    glColor3f(0.2f, 0.4f, 1.0f); 
	else
	    glColor3f(0, 0, 0);

	drawCircle(leftCenter, robot.leftRadius);

    if (sliderDraggingRight)
	    glColor3f(0.2f, 0.4f, 1.0f); 
	else
	    glColor3f(0, 0, 0);

	drawCircle(rightCenter, robot.rightRadius);

	if (draggingInner)
	    glColor3f(1.0f, 0.85f, 0.35f); // highlight
	else
	    glColor3f(0, 0, 0);

	drawCircle(inner, b.innerRadius);

    // TOP curve (use circle surface, not center)
    Point topP0 = circleEdge(leftCenter,  b.leftRadius,  angle + 90);
    Point topP2 = circleEdge(rightCenter, b.rightRadius, angle + 90);
    Point topP1 = rotatePoint(b.topCtrl, center, angle);

    // BOTTOM curve
    Point botP0 = circleEdge(leftCenter,  b.leftRadius,  angle - 90);
    Point botP2 = circleEdge(rightCenter, b.rightRadius, angle - 90);
    Point botP1 = rotatePoint(b.bottomCtrl, center, angle);

	if (topActive)
	    glColor3f(0.2f, 0.4f, 1.0f); // blue
	else
	    glColor3f(0, 0, 0);

	drawBezier(topP0, topP1, topP2);

	if (bottomActive)
	    glColor3f(0.2f, 0.4f, 1.0f);
	else
	    glColor3f(0, 0, 0);

	drawBezier(botP0, botP1, botP2);


	// HANDLE
	Point topHandle = rotatePoint(b.topCtrl, center, angle);
	Point bottomHandle = rotatePoint(b.bottomCtrl, center, angle);

	drawHandle(topHandle, activeHandle == 1);
	drawHandle(bottomHandle, activeHandle == 2);

	drawInnerHandle(inner, draggingInner);
	
	if (activeHandle == 1)
	    glColor3f(1.0f, 0.3f, 0.3f); // highlight top
	else if (activeHandle == 2)
	    glColor3f(0.3f, 0.3f, 1.0f); // highlight bottom
	else
	    glColor3f(0, 0, 0);
}

// ---------------- OPENGL ----------------

void setupOpenGL(HWND hwnd)
{
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,
        0,0,0,0,0,0,
        0,0,
        0,0,0,0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0,0,0
    };

    hdc = GetDC(hwnd);

    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);

    hrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hrc);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.5, 1.5, -1.5, 1.5, -1, 1);

    glClearColor(1,1,1,1);
}

void resizeGL(int w, int h)
{
    if (h == 0) h = 1;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (float)w / (float)h;

    if (aspect >= 1.0f)
        glOrtho(-1.5f * aspect, 1.5f * aspect, -1.5f, 1.5f, -1, 1);
    else
        glOrtho(-1.5f, 1.5f, -1.5f / aspect, 1.5f / aspect, -1, 1);

    glMatrixMode(GL_MODELVIEW);
}

void screenToGL(HWND hwnd, int mx, int my, float *x, float *y)
{
    RECT r;
    GetClientRect(hwnd, &r);

    float w = (float)r.right;
    float h = (float)r.bottom;

    float aspect = w / h;

    float nx = (mx / w) * 2.0f - 1.0f;
    float ny = 1.0f - (my / h) * 2.0f;

    if (aspect >= 1.0f)
    {
        *x = nx * 1.5f * aspect;
        *y = ny * 1.5f;
    }
    else
    {
        *x = nx * 1.5f;
        *y = ny * 1.5f / aspect;
    }
}

int isNear(Point a, Point b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return (dx*dx + dy*dy) < (HANDLE_RADIUS * HANDLE_RADIUS);
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(0,0,0);

    drawCapsuleBody(robot, activeHandle);

    SwapBuffers(hdc);
}

// ---------------- SLIDERS ----------------

void createSliders(HINSTANCE hInst)
{
    hwndSlider = CreateWindow(
        L"SliderWin",
        L"Controls",
        WS_OVERLAPPEDWINDOW,
        850, 100,
        400, 350,
        NULL, NULL,
        hInst, NULL
    );

	labelLeft = CreateWindow(
	    L"STATIC",
	    L"Left Circle Radius",
	    WS_CHILD | WS_VISIBLE,
	    20, 0, 200, 20,
	    hwndSlider,
	    NULL,
	    hInst,
	    NULL
	);

    sliderLeft = CreateWindowEx(
        0, TRACKBAR_CLASS, L"Left Radius",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        20, 20, 240, 40,
        hwndSlider,
        (HMENU)ID_LEFT,
        hInst,
        NULL
    );

	SendMessage(sliderLeft, TBM_SETRANGE, TRUE, MAKELPARAM(100, 300));
	SendMessage(sliderLeft, TBM_SETPOS, TRUE, 200);

	labelRight = CreateWindow(
	    L"STATIC",
	    L"Right Circle Radius",
	    WS_CHILD | WS_VISIBLE,
	    20, 60, 200, 20,
	    hwndSlider,
	    NULL,
	    hInst,
	    NULL
	);

    sliderRight = CreateWindowEx(
        0, TRACKBAR_CLASS, L"Right Radius",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        20, 80, 240, 40,
        hwndSlider,
        (HMENU)ID_RIGHT,
        hInst,
        NULL
    );

	SendMessage(sliderRight, TBM_SETRANGE, TRUE, MAKELPARAM(100, 300));
    SendMessage(sliderRight, TBM_SETPOS, TRUE, 200);

    ShowWindow(hwndSlider, SW_SHOW);
}

// ---------------- WINDOW PROC ----------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
		case WM_SIZE:
		{
		    int w = LOWORD(lParam);
		    int h = HIWORD(lParam);

		    if (h == 0) h = 1;

		    glViewport(0, 0, w, h);

		    glMatrixMode(GL_PROJECTION);
		    glLoadIdentity();

		    float aspect = (float)w / (float)h;

		    if (aspect >= 1.0f)
		        glOrtho(-1.5f * aspect, 1.5f * aspect, -1.5f, 1.5f, -1, 1);
		    else
		        glOrtho(-1.5f, 1.5f, -1.5f / aspect, 1.5f / aspect, -1, 1);

		    glMatrixMode(GL_MODELVIEW);
		}
		break;

        case WM_HSCROLL:
		{
		    HWND src = (HWND)lParam;
		    int code = LOWORD(wParam);

		    if (src == sliderLeft)
		    {
		        if (code == TB_THUMBTRACK)
		            sliderDraggingLeft = 1;
		        else if (code == TB_ENDTRACK)
		            sliderDraggingLeft = 0;
	
				LRESULT pos = SendMessage(sliderLeft, TBM_GETPOS, 0, 0);
		        int value = (int)pos;

		        robot.leftRadius = value / 1000.0f;
		    }
		    else if (src == sliderRight)
		    {
		        if (code == TB_THUMBTRACK)
		            sliderDraggingRight = 1;
		        else if (code == TB_ENDTRACK)
		            sliderDraggingRight = 0;

		        LRESULT pos = SendMessage(sliderRight, TBM_GETPOS, 0, 0);
		        int value = (int)pos;

		        robot.rightRadius = value / 1000.0f;
		    }
		}
		break;

		case WM_LBUTTONDOWN:
		{
			draggingTop = 0;
			draggingBottom = 0;
			draggingInner = 0;

		    int mx = LOWORD(lParam);
		    int my = HIWORD(lParam);

		    float x, y;
		    screenToGL(hwnd, mx, my, &x, &y);

		    Point mouse = { x, y };

		    Point topHandle = rotatePoint(robot.topCtrl, getCenter(robot), robot.angle);
			Point bottomHandle = rotatePoint(robot.bottomCtrl, getCenter(robot), robot.angle);



			Point center = getCenter(robot);
			float angle = robot.angle;

			Point innerWorld = rotatePoint(robot.innerCircle, center, angle);

			activeHandle = 0;

			if (isNear(mouse, topHandle))
			{
			    draggingTop = 1;
			    activeHandle = 1;
			}
			else if (isNear(mouse, bottomHandle))
			{
			    draggingBottom = 1;
			    activeHandle = 2;
			}
			else if (isNear(mouse, innerWorld))
			{
			    draggingInner = 1;
			    activeHandle = 3; // optional
			}
		}
		break;

		case WM_MOUSEMOVE:
		{
		    if (!draggingTop && !draggingBottom && !draggingInner)
    		break;

		    int mx = LOWORD(lParam);
		    int my = HIWORD(lParam);

		    float x, y;
		    screenToGL(hwnd, mx, my, &x, &y);

		    Point mouse = { x, y };

		    Point center = getCenter(robot);
		    float angle = robot.angle;

		    Point localMouse = inverseRotate(mouse, center, angle);

		    if (draggingTop)
			    robot.topCtrl = localMouse;

			if (draggingBottom)
			    robot.bottomCtrl = localMouse;

			if (draggingInner)
			    robot.innerCircle = localMouse;
		}
		break;

		case WM_LBUTTONUP:
		{
		    draggingTop = 0;
		    draggingBottom = 0;
		    activeHandle = 0;
			draggingInner = 0;
		}
		break;

		case WM_KEYDOWN:
		{
		    if (wParam == VK_LEFT)
		        robot.angle += 2.0f;

		    if (wParam == VK_RIGHT)
		        robot.angle -= 2.0f;
		}
		break;

        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ---------------- MAIN ----------------

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShowCmd)
{
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    robot.leftX = -0.6f;
    robot.rightX = 0.6f;
    robot.y = 0.0f;
    robot.leftRadius = 0.2f;
    robot.rightRadius = 0.2f;

	robot.topCtrl.x = 0.0f;
	robot.topCtrl.y = 0.4f;

	robot.bottomCtrl.x = 0.0f;
	robot.bottomCtrl.y = -0.4f;

	robot.innerCircle.x = 0.0f;
	robot.innerCircle.y = 0.0f;
	robot.innerRadius = 0.12f;

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CapsuleRobot";

    RegisterClass(&wc);

    WNDCLASS wcSlider = {0};
    wcSlider.lpfnWndProc = WndProc;
    wcSlider.hInstance = hInst;
    wcSlider.lpszClassName = L"SliderWin";

    RegisterClass(&wcSlider);

    HWND hwnd = CreateWindow(
        L"CapsuleRobot",
        L"Capsule Robot Body",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL,
        hInst, NULL
    );

    ShowWindow(hwnd, nShowCmd);

    setupOpenGL(hwnd);
    createSliders(hInst);

    MSG msg;

    while (1)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return 0;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        render();
    }
}
