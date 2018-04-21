<?php

  if ($_SERVER['argc'] < 4) die("ERROR: Use: php -f " . $_SERVER['argv'][0] . " <source_name> <dest_h_name> <dest_c_name>\r\n");
  $src_file = $_SERVER['argv'][1];
  $dest_h_file = $_SERVER['argv'][2];
  $dest_c_file = $_SERVER['argv'][3];

  if (!file_exists($src_file)) die('ERROR: File ' . $src_file . " does not exist!\r\n");

  // Если менялся сам скрипт, то учтём это тоже и перегенерируем файл
  $mtime = max(filemtime($src_file), filemtime($_SERVER["SCRIPT_FILENAME"]));

  if (file_exists($dest_h_file) && file_exists($dest_c_file) &&
     (filemtime($dest_h_file) == $mtime) && (filemtime($dest_c_file) == $mtime)) {
    die($src_file . " was not modified\r\n");
  }

  print("Converting $src_file into $dest_h_file, $dest_c_file...\r\n");


  $f = fopen($src_file, 'rb') or die("ERROR: cannot open file $src_file\r\n");

  function closedie($message) {
    global $f;
    if ($f) fclose($f);
    return die($message);
  }

  $header_data = fread($f, 54);
  if (strlen($header_data) < 54) die("ERROR: Error reading the bitmap file header\r\n");

  $header = unpack('vtype/Vsize/Vreserved/Voffbits/Vinfosize/Vwidth/Vheight/vplanes/vbitcount/Vcompression/Vsizeimage/Vxppm/Vyppm/Vcirused/Vcirimportant', $header_data);

  if ($header['type'] != 0x4D42) closedie("ERROR: Unsupported file format\r\n");
  if ($header['infosize'] < 40) closedie("ERROR: Unsupported header type\r\n");
  if ($header['bitcount'] != 24) closedie("ERROR: Unsupported pixel format (24 bits per pixel expected)\r\n");
  if ($header['compression'] != 0) closedie("ERROR: Unsupported compression type (plain RGB expected)\r\n");

  $width = $header['width'];
  $height = $header['height'];

  if (($width < 160) || ($height < 32) || ($width > 4096) || ($height > 1024)) closedie('ERROR: Unsupported bitmap size: ' . $width . ' x ' . $height . "\r\n");


  $bits_offset = $header['offbits'];

  $bytes_in_row = ($width * 3) + ($width % 4);

  $symbols_per_font = (int)floor($width / 16);
  $active_width = $symbols_per_font * 16;

  $num_fonts = (int)floor($height / 32);

  $blocks_data = Array();
  $blocks_count = 0;
  $blocks_top = 0;

  $fonts_data = Array();

  for ($font = 0 ; $font < $num_fonts ; $font++) {

    $pixels = Array();

    $font_y = $font * 32;

    for ($y = 0 ; $y < 32; $y++) {
      fseek($f, $bits_offset + (($height - ($font_y + $y) - 1) * $bytes_in_row));
      $row_data = fread($f, $width * 3);

      $pix_line = '';
      for ($x = 0 ; $x < $active_width ; $x++) {
        if ($x > $width) {
          $pix_line .= '0';
        } else {
          $i = $x * 3;
          $pix_line .= ((ord($row_data[$i]) + ord($row_data[$i + 1]) * 2 + ord($row_data[$i + 2])) >= 512) ? '1' : '0';
        }
      }
      $pixels[$y] = $pix_line;
    }

    $font_refs = Array();
    $dup_count = 0;


    for ($digit = 0 ; $digit < $symbols_per_font ; $digit++) {
      $digitx = $digit * 16;
      $digit_data = Array();
      for ($ypg = 0 ; $ypg < 32 ; $ypg += 8) {
        $line = '';
        $eq = '';
        for ($cx = 0 ; $cx < 16 ; $cx++) {
          $x = $digitx + $cx;
          $d = 0;
          for ($y = 0 ; $y < 8 ; $y++) {
            if ($pixels[$ypg + $y][$x] == '1')
              $d |= (1 << $y);
          }
          if ($cx > 0) $line .= ', ';
          $line .= '0x' . str_pad(strtoupper(dechex($d)), 2, '0', STR_PAD_LEFT);
        }

        if (isset($blocks_data[$line])) {
          $digit_data[] = $blocks_data[$line];
          $dup_count++;
        } else {
          $digit_data[] = $blocks_count;
          $blocks_data[$line] = $blocks_count;
          $blocks_count++;
        }
      }
      $font_refs[$digit] = $digit_data;

    }
    if ($blocks_count > 256) {
      print('Warning: Fnt #' . $font . " is not preprocessed: too many data blocks. Further fonts are ignored.\r\n");
    } else {
      $blocks_top = $blocks_count;
      $fonts_data[] = $font_refs;
      print("Fnt #$font is preprocessed, $dup_count duplicated blocks found \r\n");
    }
  }
  fclose($f);


  $bytes = count($fonts_data) * $symbols_per_font * 4 + $blocks_top * 16;
  $save = count($fonts_data) * $symbols_per_font * 4 * 16 - $bytes;

  print("Generating source for " . count($fonts_data) . " fonts, referencing " . $blocks_top . " data blocks (" . $bytes . " bytes of data, " . $save . " bytes saved)\r\n");

//////////////////////////////// H /////////////////////////////////

  $h_only_name = pathinfo($dest_h_file, PATHINFO_BASENAME);

  $h_macros = preg_replace('#[^A-Z]#', '_', strtoupper($h_only_name)) . '_';

  $fout = fopen($dest_h_file, 'wb') or die('ERROR: cannot write file ' . $dest_h_file);

  fputs($fout, 
"// This file is generated automatically
// Source file timestamp " . date('j.n.Y H:i:s', $mtime) ."
// Generated at " . date('j.n.Y H:i:s') . "

#ifndef $h_macros
#define $h_macros

#include <avr/io.h>
#include <avr/pgmspace.h>

#define DIGITS_COLS_PER_BLOCK 16

#define DIGITS_BLOCKS_PER_SYMBOL 4

#define DIGITS_SYMBOLS_PER_FONT $symbols_per_font

#define DIGITS_FONTS_COUNT " . count($fonts_data) . "

#define DIGITS_BLOCKS_COUNT $blocks_top

extern PROGMEM uint8_t const digits_datablocks[DIGITS_BLOCKS_COUNT][DIGITS_COLS_PER_BLOCK];

extern PROGMEM uint8_t const digits_fonts[DIGITS_FONTS_COUNT][DIGITS_SYMBOLS_PER_FONT][DIGITS_BLOCKS_PER_SYMBOL];

#endif /* $h_macros */

");

  fclose($fout);

  touch($dest_h_file, $mtime);

//////////////////////////////// C /////////////////////////////////



  $fout = fopen($dest_c_file, 'wb') or die('ERROR: cannot write file ' . $dest_c_file);

  fputs($fout, 
"// This file is generated automatically
// Source file timestamp " . date('j.n.Y H:i:s', $mtime) ."
// Generated at " . date('j.n.Y H:i:s') . "
// " . count($fonts_data) . " fonts, $blocks_top data blocks ($bytes bytes of data, $save bytes saved)

#include \"$h_only_name\"

");

  $is_first = true;
  
  $text = "PROGMEM uint8_t const digits_datablocks[][16] = {\r\n";
  foreach($blocks_data as $block => $n) {
    if ($n >= $blocks_top) break;
    if ($is_first) 
      $is_first = false;
    else 
      $text .= ",\r\n";
    $text .= "  { " . $block . " }";
  }
  $text .= "\r\n};\r\n\r\n";
  fputs($fout, $text);


  $text = "PROGMEM uint8_t const digits_fonts[][DIGITS_SYMBOLS_PER_FONT][DIGITS_BLOCKS_PER_SYMBOL] = {\r\n";
  $is_first_font = true;
  foreach ($fonts_data as $fontidx => $font_refs) {
    if ($is_first_font) 
      $is_first_font = false;
    else 
      $text .= ",\r\n";
    $text .= "  { // font #" . $fontidx ."\r\n";

    foreach ($font_refs as $digit => $refs) {
      $text .= "    { ";
     
      foreach($refs as $n => $r) {
        if ($n > 0)  $text .= ", ";
        $text .= str_pad($r, 3, ' ', STR_PAD_LEFT);
      }
      $text .= " }";
      $text .= ($digit < ($symbols_per_font - 1)) ? "," : " ";
      $text .= "  // " . (($digit < 10) ? $digit : (($digit == 10) ? ':' : (($digit == 11) ? '.' : ''))) ."\r\n";
    }
    $text .= "  }";
  }
  $text .= "\r\n};\r\n\r\n";
  fputs($fout, $text);

  fclose($fout);
  touch($dest_c_file, $mtime);

  print("Digits fonts sources have been successfuly generated\r\n");

?>