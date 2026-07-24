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
        # straight line
        plt.plot(
            [p_start[0], p_end[0]],
            [p_start[1], p_end[1]],
            'b'
        )
        return


    center, radius = circle_from_3_points(
        p_start, p_end, p_mid
    )


    def angle(p):
        return np.arctan2(
            p[1]-center[1],
            p[0]-center[0]
        ) % (2*np.pi)


    a_start = angle(p_start)
    a_end = angle(p_end)
    a_mid = angle(p_mid)


    def ccw_distance(a, b):
        return (b - a) % (2*np.pi)


    # Two possible arcs
    ccw_size = ccw_distance(a_start, a_end)
    cw_size  = ccw_distance(a_end, a_start)


    # Check if middle is inside CCW arc
    mid_in_ccw = ccw_distance(a_start, a_mid) <= ccw_size


    # Choose the smallest arc that contains middle point
    if mid_in_ccw and ccw_size <= cw_size:
        start_angle = a_start
        end_angle = a_start + ccw_size

    elif (not mid_in_ccw) and cw_size <= ccw_size:
        start_angle = a_start
        end_angle = a_start - cw_size

    elif mid_in_ccw:
        start_angle = a_start
        end_angle = a_start + ccw_size

    else:
        start_angle = a_start
        end_angle = a_start - cw_size


    angles = np.linspace(
        start_angle,
        end_angle,
        100
    )


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