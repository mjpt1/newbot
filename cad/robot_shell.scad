// Cute shell for Waveshare ESP32-S3-LCD-1.28 + LiPo 502030 (no camera).
// Same idea as starboy_shell.scad, other outlines.
// Set `shape` ("heart" | "cloud" | "ghost" | "cat") and `part` ("front" | "lid" | "preview").
shape = "heart";
part  = "preview";

/* [Board, measured] */
board_w = 36.5;     // left-right
board_h = 38.9;     // top-bottom (USB tab at the bottom)
board_t = 7.9;      // glass to back of parts
usb_in  = 2.2;      // USB-C socket set in from the board edge
glass_view = 32.6;  // round window for the screen

/* [Stack behind the board] */
batt_t = 5.0;   // 502030 LiPo (30 x 20 x 5)
slack  = 1.0;
clear  = 0.3;

/* [Shell] */
wall  = 2.4;
lip   = 1.2;
lid_t = 2.5;
round_r = 2.0;
bevel = 2.0;
bevel_scale = 0.94;

cav_d = 44;                                          // fits the board and the battery diagonal (36)
H = lip + board_t + batt_t + slack + lid_t;          // total thickness
$fn = 96;

// ---------- outlines (screen centre at the origin, USB at -Y) ----------
module heart2d() {
  hull() { translate([-18, 10]) circle(r = 26); translate([0, -50]) circle(r = 3); }
  hull() { translate([ 18, 10]) circle(r = 26); translate([0, -50]) circle(r = 3); }
}
module cloud2d() {
  translate([0, 4]) circle(r = 27);
  for (s = [-1, 1]) {
    translate([s * 27, -6]) circle(r = 19);
    translate([s * 15, 18]) circle(r = 19);
  }
  hull() for (s = [-1, 1]) translate([s * 36, -14]) circle(r = 12);  // flat-ish bottom
}
module ghost2d() {
  translate([0, 4]) circle(r = 29);
  translate([-29, -30]) square([58, 34]);
  for (x = [-19.3, 0, 19.3]) translate([x, -30]) circle(r = 9.7);  // wavy hem
  for (s = [-1, 1]) translate([s * 31, -8]) rotate(s * 25) scale([1, 0.6]) circle(r = 8);  // little arms
}
module cat2d() {
  circle(r = 30);
  for (s = [-1, 1])
    offset(r = 3) offset(delta = -3)
      polygon([[s * 4, 22], [s * 25, 8], [s * 25, 47]]);  // ears (base sunk into the head)
}
module outline2d() {
  offset(r = round_r) offset(delta = -round_r)
    if (shape == "heart") heart2d();
    else if (shape == "cloud") cloud2d();
    else if (shape == "ghost") ghost2d();
    else cat2d();
}

module body() {
  linear_extrude(bevel, scale = 1 / bevel_scale) scale(bevel_scale) outline2d();
  translate([0, 0, bevel]) linear_extrude(H - 2 * bevel) outline2d();
  translate([0, 0, H - bevel]) linear_extrude(bevel, scale = bevel_scale) outline2d();
}

module board2d(e = 0) {
  hull() {
    circle(d = board_w + 2 * e);
    translate([0, -(board_h - board_w)]) circle(d = board_w + 2 * e);
  }
}
board_bottom_y = -(board_w / 2 + (board_h - board_w));

module front() {
  difference() {
    body();
    translate([0, 0, H - lip - 1]) cylinder(d = glass_view, h = lip + 2);                        // screen window
    translate([0, 0, H - lip - board_t - clear]) linear_extrude(board_t + clear + 0.01) board2d(clear);  // board pocket
    translate([0, 0, -1]) cylinder(d = cav_d, h = H - lip - board_t + 1.01);                       // battery cavity
    translate([-6.5, board_bottom_y - 80, H - lip - board_t - 1]) cube([13, 80 + usb_in + 4, 8.5]);   // USB-C
  }
}

module lid() { cylinder(d = cav_d - 2 * clear, h = lid_t); }  // press-fit, glue if loose

if (part == "front") front();
else if (part == "lid") lid();
else { front(); color("silver") translate([0, 0, -8]) lid(); }

echo(str(shape, ": thickness ", H, " mm"));
