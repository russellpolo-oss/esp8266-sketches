#!/usr/bin/env perl
use strict;
use warnings;
use File::Path qw( remove_tree );

my $h_file     = 'baframes.h';
my $gif_file   = 'bad_apple_test.gif';
my $temp_dir   = '/tmp/ba_frames';

# Clear and recreate temp folder
remove_tree($temp_dir, { safe => 0, keep_root => 1 }) if -d $temp_dir;
mkdir $temp_dir or -d $temp_dir or die "Cannot create $temp_dir: $!";

my $FRAME_W = 88;   # including padding
my $FRAME_H = 64;
my $BYTES_PER_FRAME = 704;
my $ESCAPE_BYTE = 0x55;


# Parse baframes.h — frame by frame using // Frame comments
open my $fh, '<', $h_file or die "Cannot open $h_file: $!";
my @lines = <$fh>;
close $fh;

my @frames_hex = ();
my $current_frame = [];
my $in_data = 0;

for my $line (@lines) {
    if ($line =~ m{//\s*Frame}) {
        if (@$current_frame) {
            push @frames_hex, $current_frame;
            $current_frame = [];
        }
        $in_data = 1;
    }
    elsif ($in_data) {
        while ($line =~ /0x([0-9a-fA-F]{2})/gi) {
            push @$current_frame, hex($1);
        }
    }
}
push @frames_hex, $current_frame if @$current_frame;


my $num_frames = scalar(@frames_hex);
print "Parsed $num_frames frames from $h_file\n";


my @previous_frame = (0) x $BYTES_PER_FRAME;  # start all black

my @png_files;

for my $f (0 .. $num_frames - 1) {
    my @compressed = @{ $frames_hex[$f] };

    # Decompress RLE
    my @uncomp_bytes;
    my $i = 0;
    while ($i < @compressed) {
        my $b = $compressed[$i++];
        if ($b == $ESCAPE_BYTE) {
            my $count = $compressed[$i++];
            my $val   = $compressed[$i++];
            push @uncomp_bytes, ($val) x $count;
        }
        else {
            push @uncomp_bytes, $b;
        }
    }

    if (@uncomp_bytes != $BYTES_PER_FRAME) {
        die "Frame $f decompressed to wrong size: " . scalar(@uncomp_bytes) .
            " (expected $BYTES_PER_FRAME)\n";
    }

    # Apply delta (XOR with previous frame)
    my @current_frame;
    for my $j (0 .. $BYTES_PER_FRAME - 1) {
        push @current_frame, $uncomp_bytes[$j] ^ $previous_frame[$j];
    }

    # Update previous for next frame
    @previous_frame = @current_frame;

    # Build PBM (ASCII portable bitmap)
    my $pbm = "P1\n$FRAME_W $FRAME_H\n";
    my $bit_idx = 0;
    for my $y (0 .. $FRAME_H - 1) {
        for my $x (0 .. $FRAME_W - 1) {
            my $byte_idx = int($bit_idx / 8);
            my $bit_pos  = 7 - ($bit_idx % 8);          # MSB left
            my $byte     = $current_frame[$byte_idx];
            my $bit      = ($byte & (1 << $bit_pos)) ? 1 : 0;
            $pbm .= "$bit ";
            $bit_idx++;
        }
        $pbm .= "\n";
    }

    # Zero-padded filename: frame_0000.pbm, frame_0001.pbm, ...
    my $pbm_file = sprintf("%s/frame_%04d.pbm", $temp_dir, $f);
    open my $out, '>', $pbm_file or die $!;
    print $out $pbm;
    close $out;

    # Convert PBM → PNG + negate (invert colors)
    my $png_file = sprintf("%s/frame_%04d.png", $temp_dir, $f);
    system("magick", $pbm_file, "-negate", $png_file) == 0
        or die "magick failed for frame $f: $?";

    push @png_files, $png_file;

    print "Processed frame $f → $png_file\n" if $f % 100 == 0;  # progress
}

# Create animated GIF (100 ms delay = 10 fps)
system("magick", "-delay", "10", "-loop", "0", @png_files, $gif_file) == 0
    or die "GIF creation failed: $?";

print "\nCreated animated GIF: $gif_file\n";
print "  • delay = 10 (100 ms → ~10 fps)\n";
print "  • colors inverted with -negate\n";
print "  • temp files kept in $temp_dir (delete manually if needed)\n";
my $total_compressed = 0;
$total_compressed += scalar(@$_) for @frames_hex;

print "Total compressed size in PROGMEM: $total_compressed bytes\n";
print "  (~" . sprintf("%.1f", $total_compressed / 1024) . " KiB)\n";
print "  Average per frame: " . sprintf("%.1f", $total_compressed / $num_frames) . " bytes\n\n";