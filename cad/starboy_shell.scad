// StarBoy-style star shell for Waveshare ESP32-S3-LCD-1.28 + ESP32-S3-CAM + LiPo 502030.
// Idea from nadellasripad11/starboy-diy (star body, bevelled faces); re-drawn for this board.
// Set `part` then F6 + Export STL:  "front" | "lid" | "preview"
part = "preview";

/* [Board, measured] */
board_w   = 36.5;   // left-right
board_h   = 38.9;   // top-bottom (the extra is the USB tab at the bottom)
board_t   = 7.9;    // glass to back of parts
usb_in    = 2.2;    // USB-C socket set in from the board edge
glass_view = 32.6;  // round window the screen shows through

/* [Stack behind the board] */
cam_t  = 6.0;   // ESP32-S3-CAM board (check yours)
batt_t = 5.0;   // 502030 LiPo
slack  = 1.0;   // foam / wires
clear  = 0.3;   // fit clearance

/* [Shell] */
points  = 5;
outer_r = 45;    // tip radius
inner_r = 29;    // valley radius (cavity must fit inside it)
wall    = 2.4;
lip     = 1.2;   // front lip that holds the glass border
lid_t   = 2.5;
tip_round = 2.0;
valley_round = 4.0;
bevel   = 2.2;
bevel_scale = 0.93;

cav_d = 2 * (inner_r - wall);                         // central cavity (fits the CAM board diagonal ~48)
H = lip + board_t + cam_t + batt_t + slack + lid_t;   // total thickness
$fn = 96;

module star2d() {
  offset(r = tip_round) offset(delta = -tip_round)
  offset(r = -valley_round) offset(delta = valley_round)
  polygon([for (i = [0 : 2 * points - 1])
    let(a = 90 + i * 180 / points, r = i % 2 ? inner_r : outer_r) [r * cos(a), r * sin(a)]]);
}

module body() {
  linear_extrude(bevel, scale = 1 / bevel_scale) scale(bevel_scale) star2d();
  translate([0, 0, bevel]) linear_extrude(H - 2 * bevel) star2d();
  translate([0, 0, H - bevel]) linear_extrude(bevel, scale = bevel_scale) star2d();
}

// board outline: round top, longer at the bottom for the USB tab
module board2d(e = 0) {
  hull() {
    circle(d = board_w + 2 * e);
    translate([0, -(board_h - board_w)]) circle(d = board_w + 2 * e);
  }
}

board_bottom_y = -(board_w / 2 + (board_h - board_w));
cam_y = (outer_r + inner_r) / 2 * 0.95;   // camera lens in the top point

module front() {
  difference() {
    body();
    // screen window
    translate([0, 0, H - lip - 1]) cylinder(d = glass_view, h = lip + 2);
    // board pocket (board pressed against the lip)
    translate([0, 0, H - lip - board_t - clear]) linear_extrude(board_t + clear + 0.01) board2d(clear);
    // main cavity, open at the back for the lid
    translate([0, 0, -1]) cylinder(d = cav_d, h = H - lip - board_t + 1.01);
    // USB-C cut at the bottom (sized for the cable's plastic overmold)
    translate([-6.5, board_bottom_y - outer_r, H - lip - board_t - 1])
      cube([13, outer_r + usb_in + 4, 8.5]);
    // camera: lens hole in the top point + channel for the module and its flex
    translate([0, cam_y, H - lip - 1]) cylinder(d = 7.5, h = lip + 2);
    hull() {
      translate([0, cam_y, H - lip - 6]) cylinder(d = 10.5, h = 6);
      translate([0, cav_d / 2 - 3, H - lip - 6]) cylinder(d = 10.5, h = 6);
    }
  }
}

module lid() {
  cylinder(d = cav_d - 2 * clear, h = lid_t);   // press-fit plug, glue if loose
}

if (part == "front") front();
else if (part == "lid") lid();
else { front(); color("silver") translate([0, 0, -8]) lid(); }

echo(str("Total thickness ", H, " mm, star width ", 2 * outer_r, " mm, cavity d ", cav_d, " mm"));
