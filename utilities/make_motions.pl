#!/usr/bin/perl
use strict;
use warnings;
use Math::Trig;   # for deg2rad()

print "// 16-direction movement tables (direction 0 = UP, clockwise)\n";
print "// Generated with sin/cos - direction 0 is straight up\n\n";

# Direction 0 = UP → angle = -90° in standard math
print "static const float dx_table[16] = {\n";
for my $d (0..15) {
    my $angle_deg = -90.0 + $d * 22.5;
    my $angle_rad = deg2rad($angle_deg);
    my $dx = cos($angle_rad);
    printf "    %6.3ff,%s   // dir %2d  (%6.1f°)\n", 
           $dx, ($d == 15 ? " " : ""), $d, $angle_deg;
}
print "};\n\n";

print "static const float dy_table[16] = {\n";
for my $d (0..15) {
    my $angle_deg = -90.0 + $d * 22.5;
    my $angle_rad = deg2rad($angle_deg);
    my $dy = sin($angle_rad);
    printf "    %6.3ff,%s   // dir %2d  (%6.1f°)\n", 
           $dy, ($d == 15 ? " " : ""), $d, $angle_deg;
}
print "};\n";