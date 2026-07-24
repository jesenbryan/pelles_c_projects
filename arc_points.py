import numpy as np
import matplotlib.pyplot as plt


def circle_from_3_points(p1, p2, p3):
    """
    Calculate circle center and radius from 3 points
    """
    x1, y1 = p1
    x2, y2 = p2
    x3, y3 = p3

    d = 2 * (x1*(y2-y3) + x2*(y3-y1) + x3*(y1-y2))

    if abs(d) < 1e-10:
        raise ValueError("Points are collinear, no circle possible")

    ux = ((x1*x1+y1*y1)*(y2-y3) +
          (x2*x2+y2*y2)*(y3-y1) +
          (x3*x3+y3*y3)*(y1-y2)) / d

    uy = ((x1*x1+y1*y1)*(x3-x2) +
          (x2*x2+y2*y2)*(x1-x3) +
          (x3*x3+y3*y3)*(x2-x1)) / d

    center = np.array([ux, uy])
    radius = np.linalg.norm(center - np.array(p1))

    return center, radius


def plot_arc(p_start, p_end, p_mid):

    # Check collinear
    area = abs(
        (p_mid[0]-p_start[0])*(p_end[1]-p_start[1]) -
        (p_mid[1]-p_start[1])*(p_end[0]-p_start[0])
    )

    if area < 1e-6:
        plt.plot(
            [p_start[0], p_end[0]],
            [p_start[1], p_end[1]],
            'b'
        )
        return


    center, radius = circle_from_3_points(
        p_start, p_end, p_mid
    )


    def get_angle(p):
        return np.arctan2(
            p[1]-center[1],
            p[0]-center[0]
        )


    a_start = get_angle(p_start)
    a_end   = get_angle(p_end)
    a_mid   = get_angle(p_mid)


    # normalize to 0 - 2pi
    a_start %= 2*np.pi
    a_end   %= 2*np.pi
    a_mid   %= 2*np.pi


    # Create both possible arcs

    # CCW arc
    ccw_angles = np.linspace(
        a_start,
        a_start + ((a_end-a_start) % (2*np.pi)),
        200
    )


    # CW arc
    cw_angles = np.linspace(
        a_start,
        a_start - ((a_start-a_end) % (2*np.pi)),
        200
    )


    def angle_on_arc(angle, arc):
        """
        Check if middle angle exists in generated arc
        """
        angle %= 2*np.pi
        arc = arc % (2*np.pi)

        return np.any(
            np.abs(np.angle(
                np.exp(1j*(arc-angle))
            )) < 1e-3
        )


    # Select correct arc
    if angle_on_arc(a_mid, ccw_angles):
        angles = ccw_angles
    else:
        angles = cw_angles


    x = center[0] + radius*np.cos(angles)
    y = center[1] + radius*np.sin(angles)

    plt.plot(x, y, 'b')


# -------------------------
# Read txt file
# -------------------------

filename = "Env.txt"

plt.figure(figsize=(8,8))

with open(filename, "r") as file:
    for line in file:
        values = list(map(float, line.split()))

        if len(values) != 6:
            continue

        start = (values[0], values[1])
        end   = (values[2], values[3])
        mid   = (values[4], values[5])

        plot_arc(start, end, mid)


# Plot settings
plt.axis("equal")
plt.grid(True)
plt.xlabel("X")
plt.ylabel("Y")
plt.title("Generated Arcs")

plt.show()