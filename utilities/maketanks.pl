#!/usr/bin/perl
use strict;
use warnings;

# =============================================================
# Perl script: Generate 16 tank directions from 4 base sprites
# Direction 0 = UP, 4 = RIGHT, 8 = DOWN, 12 = LEFT (clockwise)
# Each sprite = 8 bytes (8x8 pixels, MSB = left pixel)
# =============================================================

# ================== EDIT THESE 4 BASE SPRITES ONLY ==================

my @base = (

    # 0: UP
    [
        0b00010000,
        0b00010000,
        0b11010110,
        0b11111110,
        0b11111110,
        0b11000110,
        0b11000110,
        0b00000000,
    ],

    # 1: ~22.5°
    [
        0b00011001,
        0b00111010,
        0b01111110,
        0b11111100,
        0b11011111,
        0b00011110,
        0b00001100,
        0b00011000,
    ],

    # 2: ~45°
    [
        0b00001110,
        0b00111000,
        0b11111011,
        0b01111100,
        0b00011000,
        0b00001111,
        0b00011110,
        0b00110000,
    ],

    # 3: 67.5° rotation of 0 (RIGHT)   
    [
        0b00000110,
        0b01111110,
        0b01111100,
        0b00111101,
        0b00100110,
        0b00111100,
        0b01111110,
        0b11100000,
    ]



);

# =============================================================
# Helper functions
# =============================================================

sub vflip {
    my $s = shift;
    return [ reverse @$s ];
}

sub hflip {
    my $s = shift;
    my @flipped;

    foreach my $byte (@$s) {
        # Step 1: explode byte into 8-element array
        # temp_array[0] = bit 7 (MSB, leftmost pixel)
        # temp_array[7] = bit 0 (LSB, rightmost pixel)
        my @temp_array;
        push @temp_array, ($byte & 0x80) ? 1 : 0;   # bit 7
        push @temp_array, ($byte & 0x40) ? 1 : 0;   # bit 6
        push @temp_array, ($byte & 0x20) ? 1 : 0;   # bit 5
        push @temp_array, ($byte & 0x10) ? 1 : 0;   # bit 4
        push @temp_array, ($byte & 0x08) ? 1 : 0;   # bit 3
        push @temp_array, ($byte & 0x04) ? 1 : 0;   # bit 2
        push @temp_array, ($byte & 0x02) ? 1 : 0;   # bit 1
        push @temp_array, ($byte & 0x01) ? 1 : 0;   # bit 0

        # Step 2: rebuild byte — reverse the array
        # temp_array[7] becomes new bit 7 (new MSB)
        # temp_array[0] becomes new bit 0 (new LSB)
        my $new_byte = 0;
        $new_byte |= $temp_array[7] << 7;   # becomes MSB
        $new_byte |= $temp_array[6] << 6;
        $new_byte |= $temp_array[5] << 5;
        $new_byte |= $temp_array[4] << 4;
        $new_byte |= $temp_array[3] << 3;
        $new_byte |= $temp_array[2] << 2;
        $new_byte |= $temp_array[1] << 1;
        $new_byte |= $temp_array[0] << 0;   # becomes LSB

        push @flipped, $new_byte;
    }

    return \@flipped;
}

sub rotate90_cw {
    my $s = shift;
    my @new = (0) x 8;
    for my $y (0 .. 7) {
        for my $x (0 .. 7) {
            # Get bit at (x,y) — x=0 is left (MSB)
            my $bit = ($s->[$y] >> (7 - $x)) & 1;
            # In CW: new column = old row (y becomes new x)
            # new row   = 7 - old column (x becomes new y mirrored)
            $new[7 - $x] |= $bit << (7 - $y);
        }
    }
    return \@new;
}

# =============================================================
# Build all 16 directions
# =============================================================

my @tank;
##0:0
##1:22.5
##2:45
##3:67.5
##4:90
##5:112.5
##6:135
##7:157.5
##8:180
##9:202.5
##10:225
##11:247.5
##12:270
##13:292.5
##14:315
##15:337.5
##16:360 aka -> 0

# Base directions
$tank[0] = $base[0];# UP = base 0
$tank[1] = $base[1]; # right 22.5° = base 1
$tank[2] = $base[2]; # right 45° = base 2
$tank[3] = $base[3]; # right 67.5° = base 3                  
$tank[4] = rotate90_cw(rotate90_cw(rotate90_cw($base[0])));  # RIGHT = 90° CW of UP


$tank[5] = vflip($base[3]);    # right 102.5° = vertical flip of 3
$tank[6] = vflip($base[2]);    # right 135° = vertical flip of 2
$tank[7] = vflip($base[1]);    # right 157.5° = vertical flip of 1
$tank[8] = vflip($base[0]);    # down            # vertical flip of 0 

# 9-15 = horizontal flips of 7,6,5,4,3,2,1
$tank[9]  = hflip($tank[7]);
$tank[10] = hflip($tank[6]);
$tank[11] = hflip($tank[5]);
$tank[12] = hflip($tank[4]);
$tank[13] = hflip($tank[3]);
$tank[14] = hflip($tank[2]);
$tank[15] = hflip($tank[1]);


# =============================================================
# Output Arduino PROGMEM array
# =============================================================

print "// 16 directions × 8x8 tank sprites\n";
print "// dir 0 = UP, 4 = RIGHT, 8 = DOWN, 12 = LEFT (clockwise)\n";
print "// Generated from 4 base sprites - edit only the bases above!\n\n";

print "const uint8_t tank[16][8] PROGMEM = {\n";

for my $d (0..15) {
    print "  {  // Direction $d\n";
    for my $row (0..7) {
        my $byte = $tank[$d][$row];
        printf "    0x%02X%s   // %08b\n", $byte, ($row==7 ? " " : ","), $byte;
    }
    print "  }". ($d < 15 ? ",\n" : "\n");
}

print "};\n\n";

print "// Usage: u8g2.drawXBMP(x, y, 8, 8, tank[current_direction]);\n";