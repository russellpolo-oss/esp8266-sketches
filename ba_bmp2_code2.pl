#!/usr/bin/env perl
use strict;
use warnings;
use Imager;

my $TARGET_WIDTH   = 85;
my $HEIGHT         = 64;
my $PAD_TO_BITS    = 88;               # next multiple of 8
my $BYTES_PER_ROW  = $PAD_TO_BITS / 8; # 11
my $BYTES_PER_FRAME = $HEIGHT * $BYTES_PER_ROW; # 704

my @all_frames;

foreach my $filename (@ARGV) {
    my $img = Imager->new;
    $img->read(file => $filename) or do {
        warn "Cannot read $filename: " . $img->errstr . "\n";
        next;
    };

    my $w = $img->getwidth;
    my $h = $img->getheight;

    if ($w < $TARGET_WIDTH || $h != $HEIGHT) {
        warn "$filename: wrong size ${w}x${h} (want >=${TARGET_WIDTH}x$HEIGHT)\n";
        next;
    }

    # We'll work with a grayscale version for simplicity
    my $gray = $img->convert(preset => 'gray');

    my @frame_bytes;

    for my $y (0 .. $HEIGHT-1) {           # top to bottom
        my $bitstring = '';

        for my $x (0 .. $TARGET_WIDTH-1) { # left to right
            my $color = $gray->getpixel(x => $x, y => $y);
            my ($val) = $color->rgba;      # 0..255 gray value
            $bitstring .= ($val > 128) ? '1' : '0';
        }

        # Pad with exactly 3 zero bits
        $bitstring .= '000';

        # Pack into 11 bytes
        for (my $i = 0; $i < $PAD_TO_BITS; $i += 8) {
            my $byte_bin = substr($bitstring, $i, 8);
            my $byte_val = oct("0b$byte_bin");
            push @frame_bytes, sprintf("0x%02x", $byte_val);
        }
    }

    if (@frame_bytes != $BYTES_PER_FRAME) {
        warn "$filename: generated wrong byte count\n";
        next;
    }

    push @all_frames, \@frame_bytes;
    print STDERR "Processed $filename OK\n";
}

# Output C array (same as before)
print "const uint8_t bad_apple[][704] PROGMEM = {\n";

for (my $f = 0; $f < @all_frames; $f++) {
    print "  // Frame $f\n  {\n";
    my $bytes = $all_frames[$f];
    for (my $i = 0; $i < $BYTES_PER_FRAME; $i++) {
        printf "    %s,", $bytes->[$i];
        print "\n" if ($i % $BYTES_PER_ROW == $BYTES_PER_ROW - 1);
    }
    print "  }";
    print "," if $f < $#all_frames;
    print "\n\n";
}

print "};\n\n";
print "// Total frames: " . scalar(@all_frames) . "\n";
print "// Generated with Imager - 85×64 pixels padded to 88 bits/row\n";

