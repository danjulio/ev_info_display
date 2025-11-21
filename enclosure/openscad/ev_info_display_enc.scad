//
// EV Info Display enclosure for Wired (CAN Interface Board) and Wireless
// (USB powered) Waveshare 2.8" round LCD module
//
// Copyright 2025 Dan Julio
// 
// This is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// It is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this software.  If not, see <https://www.gnu.org/licenses/>.
//

// Render control
//   1. Top
//   2. Bottom - Short (wireless)
//   3. Bottom - Tall (wired)
//   4. Combined 1+2
//   5. Combined 1+3
//   6. Mount plate
//   7. Combined 2+6
//   8. Combined 3+6
render = 1;


// CAN Interface PCB
standoff_h = 5;
standoff_hole_d = 2.5;
standoff_d = 5.5;

// Top related
sph_d = 105;
sph_neg_offset_d = 12;

top_h = 15;
top_inner_d = 88;

// Bottom related
base_d = 102;
base_floor_h = 3;
short_base_h = base_floor_h + 12;
tall_base_h = 26 + standoff_h + 1.7;
base_lcd_d = 96.5;
base_lcd_h = 1;
base_inner_d = 92;
mount_hole_d = 2.75;
usb_cutout_w = 13;
usb_cutout_h = 8;
usb_center_offset = 7.7;
wire_hole_d = 7;

// Mounting related
flange_h = 6;
flange_w = 10;
flange_thickness = 3;

$fn=240;


module top_outer_shell() {
    difference() {
        translate([0, 0, -sph_neg_offset_d]) {
            sphere(d = sph_d);
        }
        
        // inner cutout
        translate([0, 0, -(sph_neg_offset_d + sph_d/2 + 0.1)]) {
            cylinder(d = top_inner_d, h = sph_d + 0.2);
        }
        
        // cut top
        translate([-sph_d/2, -sph_d/2, top_h]) {
            cube([sph_d, sph_d, sph_d/2]);
        }
        
        // cut bottom
        translate([-sph_d/2, -sph_d/2, -(sph_neg_offset_d + sph_d/2 + 0.1)]) {
            cube([sph_d, sph_d, sph_neg_offset_d + sph_d/2 + 0.1]);
        }
    }
}


module mounting_flange(radius, thickness, angle, height) {
    x = radius * cos(angle);
    y = radius * sin(angle);
    
    translate([x, y, 0]) {
        rotate(angle) {
            translate([0, -flange_w/2, 0]) {
                cube([thickness, flange_w, height]);
            }
        }
    }
}


module top() {
    top_outer_shell();
    mounting_flange(base_d/2 - 2, 2,   0, flange_h);
    mounting_flange(base_d/2 - 2, 2,  90, flange_h);
    mounting_flange(base_d/2 - 2, 2, 180, flange_h);
    mounting_flange(base_d/2 - 2, 2, 270, flange_h);
}


module base_shell(height) {
    difference() {
        cylinder(d=base_d, h = height);
        
        // Cut out interior
        translate([0, 0, base_floor_h]) {
            cylinder(d=base_inner_d, h=height - base_floor_h + 0.1);
        }
        
        // Cut out lcd mount
        translate([0, 0, height - base_lcd_h]) {
            cylinder(d=base_lcd_d, h=base_lcd_h+0.1);
        }
    }
}


module usb_cutout(height, angle) {
    rotate(angle) {
        translate([-usb_cutout_w/2, 88/2, height-(usb_center_offset + usb_cutout_h/2)]) {
            cube([usb_cutout_w, 10, usb_cutout_h]);
        }
    }
}


module short_base() {
    base_h = short_base_h;
    
    difference() {
        union() {
            base_shell(base_h);
            
            // Mounting flanges
            mounting_flange(base_d/2-1, flange_thickness +  1,   0, base_h);
            mounting_flange(base_d/2,   flange_thickness,        0, base_h+flange_h);
            mounting_flange(base_d/2-1, flange_thickness +  1,  90, base_h);
            mounting_flange(base_d/2,   flange_thickness,       90, base_h+flange_h);
            mounting_flange(base_d/2-1, flange_thickness +  1, 180, base_h);
            mounting_flange(base_d/2,   flange_thickness,      180, base_h+flange_h);
            mounting_flange(base_d/2-1, flange_thickness +  1, 270, base_h);
            mounting_flange(base_d/2,   flange_thickness,      270, base_h+flange_h);
        }
        
        // Wire access hole
        translate([0, 0, -0.1]) {
            cylinder(d=wire_hole_d, h=base_floor_h + 0.2);
        }
        
        // Mounting holes
        translate([-10, -10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        translate([10, -10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        translate([-10, 10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        translate([10, 10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        
        // USB cutouts (only for native USB port - we don't expose the port
        // connected to the ESP32S3 serial lines because those are used for
        // the hardware CAN bus interface and we don't want someone plugging
        // power into that USB port and disconnecting them from the CAN driver. 
        usb_cutout(base_h, 225);
    }
}


module tall_base() {
    base_h = tall_base_h;
    
    difference() {
        union() {
            base_shell(base_h);
            
            // Mounting flanges
            mounting_flange(base_d/2-1, flange_thickness +  1,   0, base_h);
            mounting_flange(base_d/2,   flange_thickness,        0, base_h+flange_h);
            mounting_flange(base_d/2-1, flange_thickness +  1,  90, base_h);
            mounting_flange(base_d/2,   flange_thickness,       90, base_h+flange_h);
            mounting_flange(base_d/2-1, flange_thickness +  1, 180, base_h);
            mounting_flange(base_d/2,   flange_thickness,      180, base_h+flange_h);
            mounting_flange(base_d/2-1, flange_thickness +  1, 270, base_h);
            mounting_flange(base_d/2,   flange_thickness,      270, base_h+flange_h);
            
            // CAN Interface board standoffs
            translate([-15.24, 19.21, 0]) {
                cylinder(d=standoff_d, h=base_floor_h + standoff_h);
            }
            translate([15.24, 19.21, 0]) {
                cylinder(d=standoff_d, h=base_floor_h + standoff_h);
            }
            translate([-15.24, -24.605, 0]) {
                cylinder(d=standoff_d, h=base_floor_h + standoff_h);
            }
            translate([15.24, -24.605, 0]) {
                cylinder(d=standoff_d, h=base_floor_h + standoff_h);
            }
        }
        
        // Wire access hole
        translate([0, 0, -0.1]) {
            cylinder(d=wire_hole_d, h=base_floor_h + 0.2);
        }
        
        // Mounting holes
        translate([-10, -10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        translate([10, -10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        translate([-10, 10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        translate([10, 10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        
        // CAN Interface board standoff holes
        translate([-15.24, 19.21, 0.5]) {
            cylinder(d=standoff_hole_d, h=base_floor_h + standoff_h);
        }
        translate([15.24, 19.21, 0.5]) {
            cylinder(d=standoff_hole_d, h=base_floor_h + standoff_h);
        }
        translate([-15.24, -24.605, 0.5]) {
            cylinder(d=standoff_hole_d, h=base_floor_h + standoff_h);
        }
        translate([15.24, -24.605, 0.5]) {
            cylinder(d=standoff_hole_d, h=base_floor_h + standoff_h);
        }
        
        // USB cutouts (only for native USB port - we don't expose the port
        // connected to the ESP32S3 serial lines because those are used for
        // the hardware CAN bus interface and we don't want someone plugging
        // power into that USB port and disconnecting them from the CAN driver. 
        usb_cutout(base_h, 225);
    }
}


module mount_plate() {
    mount_w = 30;
    mount_thickness = 3;
    tolerance = 0.2;
    
    difference () {
        translate([-(mount_w/2), -(base_d/2 + flange_thickness + mount_thickness + tolerance), 0]) {
            cube([30, (base_d/2 + 2 + 20), base_floor_h]);
        }
        
        // Mounting holes
        translate([-10, -10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        translate([10, -10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        translate([-10, 10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        translate([10, 10, -0.1]) {
            cylinder(d=mount_hole_d, h=base_floor_h + 0.2);
        }
        
        // Wire access hole
        translate([0, 0, -0.1]) {
            cylinder(d=wire_hole_d, h=base_floor_h + 0.2);
        }
        
        // Wire access slot (lets us attach mount after wire is in place)
        translate([-wire_hole_d/2, 0, -0.1]) {
            cube([wire_hole_d, 20, base_floor_h + 0.2]);
        }
    }
    
    translate([-(mount_w/2), -(base_d/2 + flange_thickness + mount_thickness + tolerance), 0]) {
        cube([mount_w, mount_thickness, base_floor_h+short_base_h]);
    }
    
    // Strengthening
    translate([-(mount_w/2), -(base_d/2 + mount_thickness + tolerance), mount_thickness]) {
        rotate([0, 90, 0]) {
            cylinder(d=6, h=2);
        }
    }
    translate([mount_w/2 - 2, -(base_d/2 + mount_thickness + tolerance), mount_thickness]) {
        rotate([0, 90, 0]) {
            cylinder(d=2*mount_thickness, h=2);
        }
    }
    
    translate([-(flange_w/2 + 3 + tolerance/2), -(base_d/2 + flange_thickness + 1 + tolerance), 0]) {
        cube([3, flange_thickness + 1, base_floor_h + short_base_h]);
    }
    
    translate([flange_w/2 + tolerance/2, -(base_d/2 + flange_thickness + 1 + tolerance), 0]) {
        cube([3, flange_thickness + 1, base_floor_h + short_base_h]);
    }
}


if (render == 1) {
    top();
}


if (render == 2) {
    short_base();
}


if (render == 3) {
    tall_base();
}


if (render == 4) {
    translate([0, 0, short_base_h]) {
        #top();
    }
    short_base();
}


if (render == 5) {
    translate([0, 0, tall_base_h]) {
        #top();
    }
    tall_base();
}


if (render == 6) {
    mount_plate();
}


if (render == 7) {
    translate([0, 0, -base_floor_h]) {
        #mount_plate();
    }
    short_base();
}


if (render == 8) {
    translate([0, 0, -base_floor_h]) {
        #mount_plate();
    }
    tall_base();
}
