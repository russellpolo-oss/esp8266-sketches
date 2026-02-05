#!/usr/bin/env perl
use strict;
use warnings;

my $TARGET_WIDTH   = 85;
my $HEIGHT         = 64;
my $PAD_TO_BITS    = 88;
my $BYTES_PER_ROW  = $PAD_TO_BITS / 8; # 11
my $BYTES_PER_FRAME_UNCOMP = $HEIGHT * $BYTES_PER_ROW; # 704
my $total_savings = 0;
my $original_total = 1; # Avoid division by zero

# Rotation amount (positive for left, negative for right)
my $CIRCULAR_SHIFT_BITS = 18;  # Adjust sign if direction wrong

# RLE settings
my $ESCAPE_BYTE = 0x55;  # Unlikely pattern

my @all_compressed_frames;
my @frame_sizes;
## set previous_frame to 704 bites of 0x00 (all black) for first frame diff
my @previous_frame = (0) x $BYTES_PER_FRAME_UNCOMP;
my $frame_num = 0;
my $KEYFRAME_EVERY = 10000;  # set frequency of keyframes (0 = all keyframes, no deltas)

foreach my $filename (@ARGV) {
    open my $fh, '<:raw', $filename or do {
        warn "Cannot open $filename: $!\n";
        next;
    };

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
        warn "$filename: size ${width}x" . abs($height) . "  expected >=${TARGET_WIDTH}x${HEIGHT}\n"; next;
    }

    seek($fh, $bfOffBits, 0) or next;

    my $src_row_bytes = (($width * 3 + 3) & ~3);

    my @bin_rows;

    for (my $y = 0; $y < abs($height); $y++) {
        my $row_data;
        read($fh, $row_data, $src_row_bytes) == $src_row_bytes or last;

        my $bin_row = '';
        for (my $x = 0; $x < $TARGET_WIDTH; $x++) {
            my ($b, $g, $r) = unpack 'C3', substr($row_data, $x * 3, 3);
            my $luma = 0.299 * $r + 0.587 * $g + 0.114 * $b;
            $bin_row .= ($luma > 128) ? '1' : '0';
        }

        # Circular rotation on the 85 bits *before* padding
        my $shift = $CIRCULAR_SHIFT_BITS % $TARGET_WIDTH;
        if ($shift < 0) { $shift += $TARGET_WIDTH; }  # Handle right rotation
        $bin_row = substr($bin_row, $shift) . substr($bin_row, 0, $shift);

        # Padding: duplicate the last bit 3 times
        my $last_bit = substr($bin_row, -1);
        $bin_row .= $last_bit x 3;

        push @bin_rows, $bin_row;
    }

    if (@bin_rows != $HEIGHT) {
        warn "$filename: got " . scalar(@bin_rows) . " rows, expected $HEIGHT\n";
        next;
    }

    # Reverse to make top-down
    @bin_rows = reverse @bin_rows;

    # Pack to uncompressed bytes
    my @uncomp_bytes;
    foreach my $bin_row (@bin_rows) {
        for (my $i = 0; $i < $PAD_TO_BITS; $i += 8) {
            my $byte_bin = substr($bin_row, $i, 8);
            my $byte_val = oct("0b$byte_bin");
            push @uncomp_bytes, $byte_val;
        }
    }
# We need a persistent previous frame across files/frames
# Declare this **outside** the loop, at the top of the script:
# my @previous_frame = ();   # initially empty → first frame is keyframe

# ────────────────────────────────────────────────
# Inside the per-frame processing, after building @uncomp_bytes

my @delta_bytes = @uncomp_bytes;   # start with a copy

if (@previous_frame && $frame_num % $KEYFRAME_EVERY != 0) {
    # XOR with previous reconstructed frame
    for my $i (0 .. $#uncomp_bytes) {
        $delta_bytes[$i] ^= $previous_frame[$i];
    }
}

# Now @delta_bytes contains either full frame (keyframe) or delta

# Compress the delta / full frame
my @compressed = ();
my $i = 0;
while ($i < @delta_bytes) {
    my $byte = $delta_bytes[$i];
    my $count = 1;
    $i++;
    while ($i < @delta_bytes && $delta_bytes[$i] == $byte && $count < 255) {
        $count++;
        $i++;
    }

    if ($count > 2 || $byte == $ESCAPE_BYTE) {
        push @compressed, $ESCAPE_BYTE, $count, $byte;
    } else {
        push @compressed, ($byte) x $count;
    }
}

# IMPORTANT: update previous to the ORIGINAL frame (not the delta!)
@previous_frame = @uncomp_bytes;
$frame_num++;

    push @all_compressed_frames, \@compressed;
    push @frame_sizes, scalar(@compressed);

    print STDERR "Processed $filename: compressed " . scalar(@compressed) . "B\n";
    $total_savings += (scalar(@uncomp_bytes) - scalar(@compressed));
    $original_total += scalar(@uncomp_bytes);
}

# ------------------------------------------------
# Output C code
# ------------------------------------------------

print "#define NUM_FRAMES " . scalar(@all_compressed_frames) . "\n\n";

print "const uint8_t compressed_data[] PROGMEM = {\n";

my $total_size = 0;
my @cum_offsets = (0);
for my $frame (@all_compressed_frames) {
    $total_size += @$frame;
    push @cum_offsets, $total_size;

    print "  // Frame\n";
    for my $b (@$frame) {
        printf "  0x%02x,", $b;
    }
    print "\n";
}

print "};\n\n";

print "const uint32_t frame_offsets[NUM_FRAMES + 1] PROGMEM = {\n";
print "  " . join(", ", @cum_offsets) . "\n";
print "};\n\n";

print "// Total compressed size: $total_size bytes\n";
print "// Avg per frame: " . sprintf("%.1f", $total_size / scalar(@all_compressed_frames)) . "\n";
print "// Rotation: $CIRCULAR_SHIFT_BITS bits before padding\n";
print "// Padding: duplicate last bit x3\n";
print "// Total uncompressed size: $original_total bytes\n";
print "// Total savings: $total_savings bytes (" . sprintf("%.1f", 100 * $total_savings / $original_total) . "%)\n";
