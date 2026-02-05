#!/usr/bin/env perl
use strict;
use warnings;

my $TARGET_WIDTH   = 85;
my $HEIGHT         = 64;
my $PAD_TO_BITS    = 88;               # 85 + 3 zeros
my $BYTES_PER_ROW  = $PAD_TO_BITS / 8; # 11
my $BYTES_PER_FRAME = $HEIGHT * $BYTES_PER_ROW; # 704

# === KEY: circular rotation amount in bits (positive = left, negative = right) ===
my $CIRCULAR_LEFT_SHIFT_BITS = 48;     # 6 bytes * 8 = 48; try -48 if wrong direction

my @all_frames;

foreach my $filename (@ARGV) {
    open my $fh, '<:raw', $filename or do {
        warn "Cannot open $filename: $!\n";
        next;
    };

    # Read BMP file header (14 bytes) + DIB header (at least 40 bytes)
    my $header;
    read($fh, $header, 54) == 54 or do {
        warn "$filename: file too short\n"; next;
    };

    my ($bfType, $bfSize, $bfOffBits,
        $biSize, $width, $height, $planes, $bitcount, $compression)
      = unpack 'a2 L L x4 L l l S S L', $header;

    if ($bfType ne 'BM') {
        warn "$filename: not a valid BMP\n"; next;
    }
    if ($bitcount != 24) {
        warn "$filename: expected 24-bit BMP, got $bitcount bpp\n"; next;
    }
    if ($compression != 0) {
        warn "$filename: only uncompressed BMP supported\n"; next;
    }
    if ($width < $TARGET_WIDTH || abs($height) != $HEIGHT) {
        warn "$filename: size ${width}x" . abs($height) . "  expected at least ${TARGET_WIDTH}x${HEIGHT}\n"; next;
    }

    # Pixel data offset
    seek($fh, $bfOffBits, 0) or next;

    # BMP row bytes (padded to 4-byte boundary)
    my $src_row_bytes = (($width * 3 + 3) & ~3);

    my @bin_rows;

    # BMP is bottom-up; we want top-down ? read & reverse
    for (my $y = 0; $y < abs($height); $y++) {
        my $row_data;
        read($fh, $row_data, $src_row_bytes) == $src_row_bytes or last;

        my $bin_row = '';
        for (my $x = 0; $x < $TARGET_WIDTH; $x++) {
            # BMP = BGR order
            my ($b, $g, $r) = unpack 'C3', substr($row_data, $x * 3, 3);

            # Simple luminance threshold (ITU-R BT.601)
            my $luma = 0.299 * $r + 0.587 * $g + 0.114 * $b;

            # 1 = white (on), 0 = black (off)  common for OLED
            $bin_row .= ($luma > 128) ? '1' : '0';
        }

        push @bin_rows, $bin_row;   # 85 bits, no padding yet
    }

    if (@bin_rows != $HEIGHT) {
        warn "$filename: read only " . scalar(@bin_rows) . " rows (expected $HEIGHT)\n";
        next;
    }

    # Reverse to make top-down
    @bin_rows = reverse @bin_rows;

    # Pack into hex bytes
    my @frame_bytes;
    foreach my $bin_row (@bin_rows) {
        # Circular rotation on the 85 bits
        my $shift = $CIRCULAR_LEFT_SHIFT_BITS % $TARGET_WIDTH;
        if ($shift < 0) { $shift += $TARGET_WIDTH; }   # handle negative as right rotation

        my $shifted = substr($bin_row, $shift) . substr($bin_row, 0, $shift);

        # Append padding zeros (always at the end, after rotation)
        $shifted .= '000';

        # Pack to bytes
        for (my $i = 0; $i < $PAD_TO_BITS; $i += 8) {
            my $byte_bin = substr($shifted, $i, 8);
            # $byte_bin = reverse $byte_bin;   # uncomment if image is mirrored left-right
            my $byte_val = oct("0b$byte_bin");
            push @frame_bytes, sprintf("0x%02x", $byte_val);
        }
    }

    if (@frame_bytes != $BYTES_PER_FRAME) {
        warn "$filename: internal error  wrong byte count\n"; next;
    }

    push @all_frames, \@frame_bytes;
    print STDERR "Processed $filename OK\n";
}

# ------------------------------------------------
# Generate C code
# ------------------------------------------------

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
print "// Each frame: 85×64 pixels padded to 88×64 = 704 bytes\n";
print "// Rows pre-rotated circular left by $CIRCULAR_LEFT_SHIFT_BITS bits (mod 85) before padding\n";
print "// Conversion: 24-bit RGB ? 1-bit via luminance threshold (>128 ? 1)\n";
