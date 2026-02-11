#!/usr/bin/perl
use strict;
use warnings;

my $WIDTH  = 128;
my $HEIGHT = 64;
my $BYTES  = $WIDTH / 8;   # 16 bytes per row

print "// 128×64 monochrome bitmap – thin frame (top/bottom full, sides 1px)\n";
print "// Generated " . localtime() . "\n\n";

print "const uint8_t frame_bitmap[] PROGMEM = {\n";

# Top row: all pixels on (0xFF × 16)
print "  // Row 0 – full line\n";
print "  " . join(", ", ("0xFF") x $BYTES) . ",\n";

# Middle rows 1–62: 0x80 + 14×0x00 + 0x01
print "  // Rows 1–62 – leftmost + rightmost pixel on\n";
my $middle_row = "0x80, " . join(", ", ("0x00") x 14) . ", 0x01";
for my $row (1 .. 62) {
    print "  $middle_row,\n";
}

# Bottom row: same as top
print "  // Row 63 – full line\n";
print "  " . join(", ", ("0xFF") x $BYTES) . ",\n";

print "};\n\n";

print "// Total size: " . ($HEIGHT * $BYTES) . " bytes\n";
print "// Use with: u8g2.drawXBMP(0, 0, 128, 64, frame_bitmap);\n";